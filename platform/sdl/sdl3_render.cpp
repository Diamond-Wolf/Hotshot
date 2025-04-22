/*
Except for portions of the polymodel code, the code contained in this file 
is not the property of Parallax Software, and is not under the terms of the
Parallax Software Source license. Instead, it is released under the terms
of the MIT License.

The polymodel code is subject to the Parallax Software Sourece license:

THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

//#define MOCK_FUTURE

#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <tuple>
#include <bit>
#include <mutex>

#include <SDL_gpu.h>

#include "2d/gr.h"
#include "2d/rle.h"
#include "main/polyobj.h"
#include "cfile/cfile.h"
#include "main/inferno.h"
#include "misc/error.h"
#include "misc/types.h"
#include "platform/mono.h"
#include "platform/renderapi.h"
#include "main/game.h"
#include "main/kconfig.h"
#include "main/gamestat.h"
#include "main/gauges.h"
#include "main/bm.h"
#include "main/player.h"
#include "3d/globvars.h"

#ifndef MOCK_FUTURE
#include <future>
#include <main/endlevel.h>
#include <main/ai.h>
#include <main/newcheat.h>
#else
namespace std {

	enum class launch {
		async,
		deferred
	};

	template<class T> struct future {
		T val;
		bool valid = false;
		T get() {
			valid = false;
			return val;
		};
	};

	template<class T, class... TArgs> future<T> async(launch l, function<T(TArgs...)> f, TArgs... args) {
		future<T> fut;
		fut.val = f(args...);
		fut.valid = true;
		return fut;
	}

	template<class T, class... TArgs> future<T> async(launch l, T(*f)(TArgs...), TArgs... args) {
		return async<T, TArgs...>(l, function<T(TArgs...)>(f), args...);
	}

	template<class T> future<T> async(launch l, function<T(void)> f) {
		future<T> fut;
		fut.val = f();
		fut.valid = true;
		return fut;
	}

	template<class T> future<T> async(launch l, T(*f)(void)) {
		return async<T>(l, function(f));
	}


}
#endif

constexpr fix MAX_VELOCITY = i2f(50);

#define OP_EOF				0	//eof
#define OP_DEFPOINTS		1	//defpoints
#define OP_FLATPOLY		2	//flat-shaded polygon
#define OP_TMAPPOLY		3	//texture-mapped polygon
#define OP_SORTNORM		4	//sort by normal
#define OP_RODBM			5	//rod bitmap
#define OP_SUBCALL		6	//call a subobject
#define OP_DEFP_START	7	//defpoints with start
#define OP_GLOW			8	//glow value for next poly

#define w(p)  (*((short *) (p)))
#define wp(p)  ((short *) (p))
#define vp(p)  ((vms_vector *) (p))

thread_local std::vector<g3s_point> Interp_point_list;
thread_local std::vector<g3s_point*> point_list;

struct InterpColor {
	short pal_entry;
	unsigned short rgb15;
};
std::vector<InterpColor> interp_color_table; 

const vms_angvec zero_angles = { 0,0,0 };

#ifdef NDEBUG
# define ENABLE_SDL_DEBUG false
#else
# define ENABLE_SDL_DEBUG true
#endif

//#ifdef __APPLE__
#if 0
# define SHADER_EXTENSION ".ir"
# define SHADER_FORMAT SDL_GPU_SHADERFORMAT_MSL
#else
# define SHADER_EXTENSION ".spv"
# define SHADER_FORMAT SDL_GPU_SHADERFORMAT_SPIRV
#endif

#define SHADER(fname) (fname SHADER_EXTENSION)

extern SDL_Window* gameWindow;
//extern int CurWindowWidth, CurWindowHeight;

namespace HRender {

	enum ViewCloneMode {
		VCM_NO_CLONE,
		VCM_CLONE_LR,
		VCM_CLONE_RL
	};

	enum MatrixID {
		MID_ANIM = 0,
		MID_MODEL = 1,
		MID_VIEW = 2,
		MID_PROJ = 3
	};

	struct TexturePage {
		
		grs_bitmap bitmap;
		float* convertedData;

	};

	struct TransferBuffer {
		SDL_GPUTransferBuffer* buffer = NULL;
		uint8_t* memoryMap = NULL;
		uint32_t size = -1;
	};

	struct WorldVertex {
		float pos[3];
		float uv[2];
		int32_t props[4];
		float colormod[4];
	};

	typedef float (mat4f[4])[4];

#define M4_IDENTITY_MATRIX_MACRO {	\
		{1,0,0,0},					\
		{0,1,0,0},					\
		{0,0,1,0},					\
		{0,0,0,1},					\
}

	constexpr mat4f M4_IDENTITY_MATRIX M4_IDENTITY_MATRIX_MACRO;

	constexpr float CLEAR_DEPTH = 0;

	constexpr SDL_GPULoadOp WORLD_LOAD_OP = SDL_GPU_LOADOP_LOAD;
	constexpr SDL_GPUStoreOp WORLD_STORE_OP = SDL_GPU_STOREOP_STORE;

	static struct SDLRenderStruct {

		SDL_GPUDevice* device = NULL;

		SDL_GPUShader* screenFrag = NULL;
		SDL_GPUShader* screenVert = NULL;

		SDL_GPUShader* worldFrag = NULL;
		SDL_GPUShader* worldVert = NULL;

		SDL_GPUCommandBuffer* mainCommandBuffer = NULL;

		uint32_t renderWidth, renderHeight;

		SDL_GPUTexture* windowTexture = NULL;

		std::future<SDL_GPUFence*> mainPortalListBuilt;
		std::future<SDL_GPUFence*> leftWindowPortalListBuilt;
		std::future<SDL_GPUFence*> rightWindowPortalListBuilt;

		SDL_GPUBuffer* mainPortalBuffer;
		SDL_GPUBuffer* leftPortalBuffer;
		SDL_GPUBuffer* rightPortalBuffer;

		mat4f mainViewMatrix;
		mat4f subViewMatrixL;
		mat4f subViewMatrixR;

		mat4f mainProjectionMatrix;
		mat4f subProjectionMatrix;

		ViewCloneMode cloneMode;

		SDL_GPUBuffer* paletteBuffer = NULL;

		SDL_GPUBuffer* screenVertBuffer = NULL;
		SDL_GPUBuffer* screenIndBuffer = NULL;
		SDL_GPUSampler* defaultSampler = NULL;

		SDL_GPUGraphicsPipeline* screenPipeline = NULL;
		SDL_GPUGraphicsPipeline* worldPipeline = NULL;

		TexturePage* primaryPage = NULL;
		TexturePage* secondaryPage = NULL;

		SDL_GPUTexture* primaryTexture = NULL;
		SDL_GPUTexture* secondaryTexture = NULL;

		TransferBuffer primaryBuffer;
		TransferBuffer secondaryBuffer;

		struct {
			float primary[2];
			float secondary[2];
		} numTexturesInPage;

		std::vector<TexturePage> tpages;
		std::unordered_map<int, std::pair<int, int>> tpageLocations; //bm index : (tpage, index in page)

		SDL_GPUFence* activeDrawFence = NULL;
		
		TransferBuffer textureTransferBuffer;
		TransferBuffer drawTransferBuffer;

		uint32_t textureTransferOffset;
		uint32_t drawTransferOffset;

		uint32_t targetTextureTransferSize;
		
		int drawCallObjID = -2;

		SDL_GPUColorTargetInfo windowCTarget {
			.texture = NULL,
			.mip_level = 0,
			//.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_STORE,
			.cycle = true
		};

		SDL_GPUDepthStencilTargetInfo windowDTarget {
			.texture = NULL,
			.clear_depth = CLEAR_DEPTH,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUColorTargetInfo mainCTarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = WORLD_LOAD_OP,
			.store_op = WORLD_STORE_OP,
			.cycle = false
		};

		SDL_GPUDepthStencilTargetInfo mainDTarget {
			.texture = NULL,
			.clear_depth = CLEAR_DEPTH,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUColorTargetInfo leftCTarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = WORLD_LOAD_OP,
			.store_op = WORLD_STORE_OP,
			.cycle = false
		};

		SDL_GPUDepthStencilTargetInfo leftDTarget {
			.texture = NULL,
			.clear_depth = CLEAR_DEPTH,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUColorTargetInfo rightCTarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = WORLD_LOAD_OP,
			.store_op = WORLD_STORE_OP,
			.cycle = false
		};

		SDL_GPUDepthStencilTargetInfo rightDTarget {
			.texture = NULL,
			.clear_depth = CLEAR_DEPTH,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUSamplerCreateInfo defaultSamplerInfo {
			.min_filter = SDL_GPU_FILTER_NEAREST,
			.mag_filter = SDL_GPU_FILTER_NEAREST,
			.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
			.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
			.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		};

	} rendererState;

	//typedef void(*drawcall)();
	typedef std::function<void(SDL_GPUCommandBuffer*)> SideDrawCall;
	typedef std::tuple<TexturePage*, TexturePage*, ViewTarget> SideDrawKey;

	typedef std::function<void(SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass)> ObjDrawCall;

	struct dkeyCompare {

		bool operator()(const SideDrawKey a, const SideDrawKey b) const {
			const auto& [primary1, secondary1, view1] = a;
			const auto& [primary2, secondary2, view2] = b;

			if (primary1 != primary2)
				return primary1 < primary2;
			else if (secondary1 != secondary2)
				if (!secondary2)
					return true;
				else if (!secondary1)
					return false;
				else
					return secondary1 < secondary2;
			else
				return view1 < view2;
		}

	};
	
	std::vector<WorldVertex> worldVertices;
	std::vector<uint32_t> worldIndices;

	std::vector<SDL_GPUTexture*> textureFreeQueue;

	bool mineRenderingReady = false;

	const std::launch ASYNC_POLICY = std::launch::deferred;
	const SDL_GPUTextureFormat TEXTURE_FORMAT = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;

	std::map<SideDrawKey, std::vector<SideDrawCall>, dkeyCompare> sideDrawCalls;
	std::mutex sideDrawCallMutex;

	std::map<SideDrawKey, std::vector<ObjDrawCall>, dkeyCompare> modelDrawCalls;
	std::mutex modelDrawCallMutex;

	std::map<ViewTarget, std::vector<ObjDrawCall>> objDrawCalls;
	std::mutex objDrawCallMutex;

	void UploadTexturePage(TexturePage* page, SDL_GPUCopyPass* cpass, const bool secondary = false);

#pragma region SDLHelpers

	template <int bufN, int attrN> SDL_GPUGraphicsPipeline* CreateGraphicsPipeline(
		SDL_GPUShader* vertex, SDL_GPUShader* fragment, SDL_GPUPrimitiveType primitiveType,
		std::array<SDL_GPUVertexBufferDescription, bufN> buffers,
		std::array<SDL_GPUVertexAttribute, attrN> attributes,
		bool enableDepth,
		bool enableStencil,
		uint8_t stencilWriteMask = 0,
		uint8_t stencilReadMask = UINT8_MAX
	) {

		SDL_GPUColorTargetDescription ctd {
			.format = TEXTURE_FORMAT,
		};

		SDL_GPUGraphicsPipelineCreateInfo gpci {
			.vertex_shader = vertex,
			.fragment_shader = fragment,
			.vertex_input_state = {
				.vertex_buffer_descriptions = buffers.data(),
				.num_vertex_buffers = bufN,
				.vertex_attributes = attributes.data(),
				.num_vertex_attributes = attrN,
			},
			.primitive_type = primitiveType,
			.rasterizer_state = {
				.fill_mode = SDL_GPU_FILLMODE_FILL,
				.cull_mode = SDL_GPU_CULLMODE_BACK,
				.front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
				.enable_depth_bias = false,
				.enable_depth_clip = false
			},
			.multisample_state = {
				.sample_count = SDL_GPU_SAMPLECOUNT_1
			},
			.depth_stencil_state = {
				.compare_op = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
				.back_stencil_state = {
					.fail_op = SDL_GPU_STENCILOP_KEEP,
					.pass_op = SDL_GPU_STENCILOP_REPLACE,
					.depth_fail_op = SDL_GPU_STENCILOP_KEEP,
					.compare_op = SDL_GPU_COMPAREOP_ALWAYS
				},
				.front_stencil_state = {
					.fail_op = SDL_GPU_STENCILOP_KEEP,
					.pass_op = SDL_GPU_STENCILOP_REPLACE,
					.depth_fail_op = SDL_GPU_STENCILOP_KEEP,
					.compare_op = SDL_GPU_COMPAREOP_ALWAYS
				},
				.compare_mask = stencilReadMask,
				.write_mask = stencilWriteMask,
				.enable_depth_test = enableDepth,
				.enable_depth_write = enableDepth,
				.enable_stencil_test = enableStencil,
			},
			.target_info = {
				.color_target_descriptions = &ctd,
				.num_color_targets = 1,
				.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
				.has_depth_stencil_target = true
			},
		};

		return SDL_CreateGPUGraphicsPipeline(rendererState.device, &gpci);

	}

	void InitCommandBuffer() {
		if (rendererState.mainCommandBuffer == NULL)
			rendererState.mainCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);
		if (rendererState.mainCommandBuffer == NULL)
			mprintf((1, "Error acquiring command buffer: %s", SDL_GetError()));
	}

	SDL_GPURenderPass* BeginDefaultRenderPass() {
		return SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.windowCTarget, 1, &rendererState.windowDTarget);
	}

	TransferBuffer CreateTransferBuffer(const SDL_GPUTransferBufferCreateInfo* tbci, const bool cycle) {

		TransferBuffer buf;

		buf.buffer = SDL_CreateGPUTransferBuffer(rendererState.device, tbci);

		if (buf.buffer == NULL) {
			buf.memoryMap = NULL;
			mprintf((1, "Error creating transfer buffer: %s", SDL_GetError()));
			return buf;
		}

		buf.memoryMap = (uint8_t*)SDL_MapGPUTransferBuffer(rendererState.device, buf.buffer, cycle);
		if (buf.memoryMap == NULL) {
			SDL_ReleaseGPUTransferBuffer(rendererState.device, buf.buffer);
			mprintf((1, "Error mapping transfer buffer: %s", SDL_GetError()));
			return buf;
		}

		buf.size = tbci->size;
		return buf;

	}

	void FreeTransferBuffer(TransferBuffer buffer) {
		SDL_UnmapGPUTransferBuffer(rendererState.device, buffer.buffer);
		SDL_ReleaseGPUTransferBuffer(rendererState.device, buffer.buffer);
	}

#pragma endregion SDLHelpers

	bool BuildShader(const char* name, const SDL_GPUShaderStage stage, SDL_GPUShader** shader, const uint32_t samplers, const uint32_t uniforms, const uint32_t buffers) {

		mprintf((0, "Building shader %s\n", name));

		CFILE* shaderCode = cfopen(name, "rb");
		if (shaderCode == NULL) {
			mprintf((1, "Could not open shader <%s>!\n", name));
			return false;
		}

		uint8_t* codeBytes = new uint8_t[shaderCode->size];
		cfread(codeBytes, shaderCode->size, 1, shaderCode);
		
		SDL_GPUShaderCreateInfo shaderInfo {
			.code_size = (size_t)shaderCode->size,
			.code = codeBytes,
			.entrypoint = "main",
			.format = SHADER_FORMAT,
			.stage = stage,
			.num_samplers = samplers,
			.num_storage_textures = 0,
			.num_storage_buffers = buffers,
			.num_uniform_buffers = uniforms,
			.props = 0
		};

		cfclose(shaderCode);

		*shader = SDL_CreateGPUShader(rendererState.device, &shaderInfo);
		if (*shader == NULL) {
			mprintf((1, "Error creating shader from %s: %s\n", name, SDL_GetError()));
		}

		delete[] codeBytes;

		return (*shader != NULL);
	}

	void UploadPalette(uint8_t* palette) {

		const int NUM_COLORS = SDL_arraysize(gr_palette) / 3;

		SDL_GPUCommandBuffer* upbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);

		static float expandedPalette[NUM_COLORS * 4];

		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = sizeof(expandedPalette)
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, false);
		if (tbuf.memoryMap == NULL) {
			SDL_CancelGPUCommandBuffer(upbuf);
			return;
		}

		const float DIV_FACTOR = 63.f;
		for (int i = 0; i < NUM_COLORS; i++) { //std430 packs nicely, but SDL thought it would be a good idea to upload std140 data anyway
			expandedPalette[i * 4] = palette[i * 3] / DIV_FACTOR;
			expandedPalette[i * 4 + 1] = palette[i * 3 + 1] / DIV_FACTOR;
			expandedPalette[i * 4 + 2] = palette[i * 3 + 2] / DIV_FACTOR;
		}

		SDL_memcpy(tbuf.memoryMap, expandedPalette, sizeof(expandedPalette));
		
		SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(upbuf);

		SDL_GPUTransferBufferLocation tbloc {
			.transfer_buffer = tbuf.buffer,
			.offset = 0
		};

		SDL_GPUBufferRegion breg {
			.buffer = rendererState.paletteBuffer,
			.offset = 0,
			.size = sizeof(expandedPalette)
		};

		SDL_UploadToGPUBuffer(pass, &tbloc, &breg, false);

		FreeTransferBuffer(tbuf);

		SDL_EndGPUCopyPass(pass);
		SDL_SubmitGPUCommandBuffer(upbuf);

	}

	void UploadPalette() {
		UploadPalette(gr_palette);
	}

	void ResizeWindow() {

		uint32_t size;

		switch (Cockpit_mode) {
			
			case CM_FULL_SCREEN:
			case CM_REAR_VIEW:
			default:
				size = rendererState.renderWidth / 6;
			break;

			case CM_FULL_COCKPIT: {
				auto& box = gauge_boxes[COCKPIT_PRIMARY_BOX];
				size = box.right - box.left;
			} break;

			case CM_STATUS_BAR: {
				auto& box = gauge_boxes[SB_PRIMARY_BOX];
				size = box.right - box.left;
			} break;
			
		}

		if (rendererState.rightCTarget.texture) {
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.rightCTarget.texture);
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.rightDTarget.texture);
		}

		if (rendererState.leftCTarget.texture) {
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.leftCTarget.texture);
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.leftDTarget.texture);
		}

		SDL_GPUTextureCreateInfo tciC {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = TEXTURE_FORMAT,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.width = size,
			.height = size,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		SDL_GPUTextureCreateInfo tciD {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
			.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
			.width = size,
			.height = size,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		rendererState.rightCTarget.texture = SDL_CreateGPUTexture(rendererState.device, &tciC);
		rendererState.leftCTarget.texture = SDL_CreateGPUTexture(rendererState.device, &tciC);
		rendererState.rightDTarget.texture = SDL_CreateGPUTexture(rendererState.device, &tciD);
		rendererState.leftDTarget.texture = SDL_CreateGPUTexture(rendererState.device, &tciD);

	}

	void ResizeRenderTarget(const unsigned int w, const unsigned int h) {

		rendererState.renderWidth = w;
		rendererState.renderHeight = h;

		if (rendererState.windowCTarget.texture != NULL) {
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.windowCTarget.texture);
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.windowDTarget.texture);
		}

		SDL_GPUTextureCreateInfo texCreateInfo {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = TEXTURE_FORMAT,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.width = w,
			.height = h,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};
		
		rendererState.windowCTarget.texture = SDL_CreateGPUTexture(rendererState.device, &texCreateInfo);
		if (rendererState.windowCTarget.texture == NULL) {
			Error("Error creating render texture: %s", SDL_GetError());
		}

		texCreateInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
		texCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

		rendererState.windowDTarget.texture = SDL_CreateGPUTexture(rendererState.device, &texCreateInfo);
		if (rendererState.windowDTarget.texture == NULL) {
			Error("Error creating render depth texture: %s", SDL_GetError());
		}

		SyncCockpit();

	}

	void InitScreenRendering(SDL_GPUCopyPass* cpass) {
	
		rendererState.screenPipeline = CreateGraphicsPipeline<1,1>(rendererState.screenVert, rendererState.screenFrag, SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
			std::array { SDL_GPUVertexBufferDescription { //Because the official examples give an error that the array needs to be an expression in VS
				.slot = 0,
				.pitch = sizeof(float) * 4,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
			} },
			std::array { SDL_GPUVertexAttribute {
				.location = 0,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.offset = 0,
			} },
		false, false);

		if (rendererState.screenPipeline == NULL) {
			Error("Error creating screen pipeline: %s", SDL_GetError());
		}

		std::array<float, 4 * 4> verts {
			-1.f, 1.f, 0.5f, 0.f,
			1.f, 1.f, 0.5f, 0.f,
			-1.f, -1.f, 0.5f, 0.f,
			1.f, -1.f, 0.5f, 0.f,
		};

		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = sizeof(verts)
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, false);
		if (tbuf.memoryMap == NULL)
			Error("Error creating screen vertex memory map!");

		memcpy(tbuf.memoryMap, verts.data(), sizeof(verts));
		
		SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(verts)
		};

		rendererState.screenVertBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);

		SDL_GPUTransferBufferLocation tbl {
			.transfer_buffer = tbuf.buffer,
			.offset = 0
		};

		SDL_GPUBufferRegion br {
			.buffer = rendererState.screenVertBuffer,
			.offset = 0,
			.size = sizeof(verts)
		};

		SDL_UploadToGPUBuffer(cpass, &tbl, &br, false);

		FreeTransferBuffer(tbuf);

		std::array<uint16_t, 4> inds {
			0, 1, 2, 3
		};

		tbci.size = sizeof(inds);
		tbuf = CreateTransferBuffer(&tbci, false);
		if (tbuf.memoryMap == NULL)
			Error("Error creating screen index memory map!");

		memcpy(tbuf.memoryMap, inds.data(), sizeof(inds));
		
		bci = {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = sizeof(inds)
		};
		rendererState.screenIndBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);

		tbl.transfer_buffer = tbuf.buffer;
		br.buffer = rendererState.screenIndBuffer;
		br.size = sizeof(inds);

		SDL_UploadToGPUBuffer(cpass, &tbl, &br, false);

		FreeTransferBuffer(tbuf);

	}

	void InitWorldRendering(SDL_GPUCopyPass* cpass) {

		rendererState.worldPipeline = CreateGraphicsPipeline<1, 4>(rendererState.worldVert, rendererState.worldFrag, SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			std::array { SDL_GPUVertexBufferDescription {
				.slot = 0,
				.pitch = (uint32_t)sizeof(WorldVertex),
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
			} },
			
			std::array { SDL_GPUVertexAttribute {
				.location = 0,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = (uint32_t)offsetof(WorldVertex, pos),
			}, SDL_GPUVertexAttribute {
				.location = 1,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.offset = (uint32_t)offsetof(WorldVertex, uv),
			}, SDL_GPUVertexAttribute {
				.location = 2,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_INT4,
				.offset = (uint32_t)offsetof(WorldVertex, props),
			}, SDL_GPUVertexAttribute {
				.location = 3,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
				.offset = (uint32_t)offsetof(WorldVertex, colormod),
			} },
		true, false);

		if (rendererState.worldPipeline == NULL) {
			Error("Error creating world pipeline: %s", SDL_GetError());
		}

		Interp_point_list.resize(1000);
		point_list.resize(25);
		interp_color_table.resize(100);
	}

	int InitRenderAPI() {

		mprintf((0, "\nInitializing HRender\n"));

		rendererState.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, ENABLE_SDL_DEBUG, NULL);
		if (rendererState.device == NULL) {
			mprintf((1, "Could not create GPU device: %s\n", SDL_GetError()));
			return 2;
		}

		if (!SDL_ClaimWindowForGPUDevice(rendererState.device, gameWindow)) {
			mprintf((1, "Could not link GPU to window: %s\n", SDL_GetError()));
			return 3;
		}

#ifndef NDEBUG
		auto mask = SDL_GetGPUShaderFormats(rendererState.device);
		printf("Shader format mask: ");
		for (int i = sizeof(mask) * 8 - 1; i >= 0; i--) {
			printf("%d", mask & 1);
			mask >>= 1;
		} 
		printf("\n");
#endif

		bool good = true;

		good &= BuildShader(SHADER("screenv"), SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.screenVert, 0, 0, 0);
		good &= BuildShader(SHADER("screenf"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.screenFrag, 1, 0, 1);

		good &= BuildShader(SHADER("worldv"), SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.worldVert, 0, 4, 0);
		good &= BuildShader(SHADER("worldf"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.worldFrag, 2, 1, 1);

		if (!good)
			return 4;

		InitCommandBuffer();
		if (rendererState.mainCommandBuffer == NULL) {
			return 5;
		}
		
		ResizeWindow();

		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(rendererState.mainCommandBuffer);

		SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
			.size = (SDL_arraysize(gr_palette) / 3) * sizeof(float) * 4
		};
		
		rendererState.paletteBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
		if (rendererState.paletteBuffer == NULL) {
			mprintf((1, "Could not create palette buffer: %s\n", SDL_GetError()));
			return 6;
		}

		InitScreenRendering(cpass);
		InitWorldRendering(cpass);

		rendererState.defaultSampler = SDL_CreateGPUSampler(rendererState.device, &rendererState.defaultSamplerInfo);
		if (rendererState.defaultSampler == NULL) {
			Error("Error creating sampler for software-rendered canvas: %s", SDL_GetError());
		}

		SDL_EndGPUCopyPass(cpass);
		SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(rendererState.mainCommandBuffer);
		if (!fence) {
			mprintf((1, "Error submitting setup commands: %s\n", SDL_GetError()));
			return 7;
		}
		rendererState.mainCommandBuffer = NULL;

		memset(rendererState.mainProjectionMatrix, 0, sizeof(rendererState.mainProjectionMatrix));

		constexpr float depth = FAR_CLIP_F - NEAR_CLIP_F;

		rendererState.mainProjectionMatrix[1][1] = 1 / tanf(VFOV_RAD_F / 2);
		rendererState.mainProjectionMatrix[0][0] = rendererState.mainProjectionMatrix[1][1];
		rendererState.mainProjectionMatrix[2][3] = 1.f;
		rendererState.mainProjectionMatrix[3][2] = 1.f;
		
		memcpy(rendererState.subProjectionMatrix, rendererState.mainProjectionMatrix, sizeof(rendererState.subProjectionMatrix));

		worldVertices.reserve(5000);
		worldIndices.reserve(5000);

		SDL_WaitForGPUFences(rendererState.device, false, &fence, 1);
		SDL_ReleaseGPUFence(rendererState.device, fence);
		
		return 0;

	}

	SDL_GPUFence* SkipAsync() {
		return SDL_SubmitGPUCommandBufferAndAcquireFence(SDL_AcquireGPUCommandBuffer(rendererState.device));
	}

	void SkipGPUPortalList(const ViewTarget target) {
		switch (target) {

			case VT_MAIN:
				rendererState.mainPortalListBuilt = std::async(ASYNC_POLICY, SkipAsync);
			break;

			case VT_LEFT:
				rendererState.leftWindowPortalListBuilt = std::async(ASYNC_POLICY, SkipAsync);
			break;

			case VT_RIGHT:
				rendererState.rightWindowPortalListBuilt = std::async(ASYNC_POLICY, SkipAsync);
			break;

		}
	}

	SDL_GPUFence* BuildGPUPortalListThread(const std::vector<short> segments, const vms_vector pos, const vms_vector dir, const ViewTarget target, SDL_GPUBuffer** destBuffer);
	void BuildGPUPortalList(const std::vector<short>& segments, const vms_vector& pos, const vms_vector& dir, const ViewTarget target, const ViewTarget clone) {

		std::future<SDL_GPUFence*>* future;
		SDL_GPUBuffer** buffer;

		switch (target) {

			case VT_MAIN:
				future = &rendererState.mainPortalListBuilt;
				buffer = &rendererState.mainPortalBuffer;
			break;
		
			case VT_LEFT:
				future = &rendererState.leftWindowPortalListBuilt;
				buffer = &rendererState.leftPortalBuffer;
			break;

			case VT_RIGHT:
				future = &rendererState.rightWindowPortalListBuilt;
				buffer = &rendererState.rightPortalBuffer;
			break;

			default:
				Int3();

		}

		if (clone != target) {

			if (target == VT_LEFT && clone == VT_RIGHT)
				rendererState.cloneMode = VCM_CLONE_RL;
			else if (target == VT_RIGHT && clone == VT_LEFT)
				rendererState.cloneMode = VCM_CLONE_LR;
			else if (clone != VT_NONE)
				Error("Invalid subwindow clone mode! Target %d cloning %d", target, clone);

			*future = std::async(ASYNC_POLICY, SkipAsync);

		} else {

			*future = std::async(ASYNC_POLICY, BuildGPUPortalListThread, segments, pos, dir, target, buffer);

		}
	}

	void ClearTexturePages() {

		for (auto& tpage : rendererState.tpages) {
			delete[] tpage.bitmap.bm_data;
			delete[] tpage.convertedData;
		}

		rendererState.tpages.clear();
		rendererState.tpageLocations.clear();

	}

	void GenerateTexturePages() {
	
		ClearTexturePages();
		rendererState.tpages.reserve(activePiggyTable->gameBitmaps.size());

		piggy_bitmap_page_out_all();

		for (int i = 0; i < activePiggyTable->gameBitmaps.size(); i++) {
			
			PIGGY_PAGE_IN(BITMAP_INDEX(i));
			grs_bitmap* gbm = &activePiggyTable->gameBitmaps[i];
			int bmSize = gbm->bm_w * gbm->bm_h;

			TexturePage page;
			page.bitmap = *gbm;
			page.bitmap.bm_data = new uint8_t[bmSize];
			gr_bm_ubitblt(gbm->bm_w, gbm->bm_h, 0, 0, 0, 0, gbm, &page.bitmap);

			page.convertedData = new float[bmSize];
			for (int i = 0; i < bmSize; i++) {
				page.convertedData[i] = page.bitmap.bm_data[i];
			}

			rendererState.tpages.push_back(page);
			rendererState.tpageLocations[i] = std::pair(i, 0);
			
		}

		//8MiB probably already overkill, don't need 32 to account for floats
		rendererState.targetTextureTransferSize = 64 * 64 * activePiggyTable->gameBitmaps.size();
		
		SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(cbuf);

		UploadTexturePage(&rendererState.tpages[0], cpass, false);
		UploadTexturePage(&rendererState.tpages[0], cpass, true);

		SDL_EndGPUCopyPass(cpass);
		SDL_SubmitGPUCommandBuffer(cbuf);

	}

	void RenderScreenBitmap(grs_bitmap* bm) {

		InitCommandBuffer();

		int bmSize = bm->bm_w * bm->bm_h;

		SDL_GPUTextureCreateInfo tci {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.width = (uint32_t)bm->bm_w,
			.height = (uint32_t)bm->bm_h,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		SDL_GPUTexture* bmTex = SDL_CreateGPUTexture(rendererState.device, &tci);
		if (bmTex == NULL) {
			Error("Error creating GPU texture for grs_bitmap: %s", SDL_GetError());
		}

		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (uint32_t)(bmSize * sizeof(float))
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, true);
		if (tbuf.memoryMap == NULL) {
			Error("Could not create bitmap transfer buffer!");
		}

		float* floatMemMap = reinterpret_cast<float*>(tbuf.memoryMap);

		for (int i = 0; i < bmSize; i++) {
			floatMemMap[i] = bm->bm_data[i];
		}
		
		SDL_GPUCommandBuffer* copycmd = SDL_AcquireGPUCommandBuffer(rendererState.device);
		if (copycmd == NULL) {
			Error("Error acquiring bitmap transfer command buffer: %s", SDL_GetError());
		}

		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(copycmd);

		SDL_GPUTextureTransferInfo tti {
			.transfer_buffer = tbuf.buffer,
			.offset = 0,
			.pixels_per_row = (uint32_t)bm->bm_w
		};

		SDL_GPUTextureRegion tr {
			.texture = bmTex,
			.w = (uint32_t)bm->bm_w,
			.h = (uint32_t)bm->bm_h,
			.d = 1
		};

		SDL_UploadToGPUTexture(cpass, &tti, &tr, true);

		FreeTransferBuffer(tbuf);
		SDL_EndGPUCopyPass(cpass);

		SDL_SubmitGPUCommandBuffer(copycmd);

		//--End copy, begin render--

		SDL_GPURenderPass* rpass = BeginDefaultRenderPass();

		SDL_BindGPUGraphicsPipeline(rpass, rendererState.screenPipeline);

		SDL_GPUBufferBinding bb {
			.buffer = rendererState.screenVertBuffer,
			.offset = 0
		};
		SDL_BindGPUVertexBuffers(rpass, 0, &bb, 1);

		bb.buffer = rendererState.screenIndBuffer;
		SDL_BindGPUIndexBuffer(rpass, &bb, SDL_GPU_INDEXELEMENTSIZE_16BIT);

		SDL_GPUTextureSamplerBinding tsb {
			.texture = bmTex,
			.sampler = rendererState.defaultSampler
		};
		SDL_BindGPUFragmentSamplers(rpass, 0, &tsb, 1);

		SDL_BindGPUFragmentStorageBuffers(rpass, 0, &rendererState.paletteBuffer, 1);

		SDL_DrawGPUIndexedPrimitives(rpass, 4, 1, 0, 0, 0); 

		SDL_ReleaseGPUTexture(rendererState.device, bmTex);

		SDL_EndGPURenderPass(rpass);

	}

	void RenderScreenCanvas(grs_canvas* canvas) {
		RenderScreenBitmap(&canvas->cv_bitmap);
	}

	void PrepareMineRenderFrame() { // TODO: If in game, build and submit portal list. Also, determine if rear view mirrors need textures.
		
		sideDrawCalls.clear();
		modelDrawCalls.clear();
		objDrawCalls.clear();

		InitCommandBuffer();

		rendererState.cloneMode = VCM_NO_CLONE;

		if (mineRenderingReady) 
			mprintf((1, "Double prepared mine render!"));
		
		mineRenderingReady = true;

		rendererState.drawTransferOffset = rendererState.textureTransferOffset = 0;

	}

	void UpdateMainView(const float aspect) {
		rendererState.mainProjectionMatrix[0][0] = rendererState.mainProjectionMatrix[1][1] / aspect;
	}

	void SyncCockpit() {

		float aspect;

		switch (Cockpit_mode) {

			default:
			case CM_FULL_SCREEN:
			case CM_REAR_VIEW:
				aspect = (float)rendererState.renderWidth / rendererState.renderHeight;
				UpdateMainView(aspect);
			break;
			
			case CM_STATUS_BAR:
			case CM_LETTERBOX:
				aspect = 2;
				UpdateMainView(aspect);
			break;

			case CM_FULL_COCKPIT: // TODO scale by actual screen resolution
				aspect = 2;
				UpdateMainView(aspect);
			break;
		}

		if (rendererState.mainCTarget.texture) {
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.mainCTarget.texture);
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.mainDTarget.texture);
		}
		
		SDL_GPUTextureCreateInfo tci {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = TEXTURE_FORMAT,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.width = rendererState.renderWidth,
			.height = (uint32_t)(rendererState.renderWidth / aspect),
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		rendererState.mainCTarget.texture = SDL_CreateGPUTexture(rendererState.device, &tci);	
		
		tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
		tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

		rendererState.mainDTarget.texture = SDL_CreateGPUTexture(rendererState.device, &tci);

	}
	
	void UpdateViewMatrix(object* source, const ViewTarget target) {

		mat4f* matrix;
		
		switch (target) {
		
			case VT_MAIN:
				matrix = &rendererState.mainViewMatrix;
			break;

			case VT_LEFT:
				matrix = &rendererState.subViewMatrixL;
			break;

			case VT_RIGHT:
				matrix = &rendererState.subViewMatrixR;
			break;

			default:
				Int3();

		}

		memcpy(*matrix, M4_IDENTITY_MATRIX, sizeof(*matrix));

		static vms_matrix id = IDENTITY_MATRIX;
		
		for (int m = 0; m < 3; m++) {
			for (int n = 0; n < 3; n++) {
				(*matrix)[m][n] = f2fl(source->orient[n][m]);
			}

			//Cheat and encode pre-translation in matrix. Yay SDL only allowing 4 uniforms.
			(*matrix)[3][m] = -f2fl(source->pos[m]);
		}

	}

	void UploadVertexMatrix(SDL_GPUCommandBuffer* cbuf, const mat4f* matrix, const MatrixID id) {
		SDL_PushGPUVertexUniformData(cbuf, id, *matrix, sizeof(*matrix));
	}

	void UploadTexturePage(TexturePage* page, SDL_GPUCopyPass* cpass, const bool secondary) {
		
		Assert(page != NULL);

		grs_bitmap& bm = page->bitmap;

		SDL_GPUTexture** ptex;
		
		TexturePage* oldPage;

		if (secondary) {
			if (rendererState.secondaryPage == page)
				return;

			oldPage = rendererState.secondaryPage;
			rendererState.secondaryPage = page;
			ptex = &rendererState.secondaryTexture;
		} else {
			if (rendererState.primaryPage == page)
				return;

			oldPage = rendererState.primaryPage;
			rendererState.primaryPage = page;
			ptex = &rendererState.primaryTexture;
		}

		int bmSize = bm.bm_w * bm.bm_h;

		SDL_GPUTextureCreateInfo tci {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.width = (uint32_t)bm.bm_w,
			.height = (uint32_t)bm.bm_h,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};
		 
		if (*ptex) {
			
			if (!(oldPage->bitmap.bm_h == page->bitmap.bm_h && oldPage->bitmap.bm_w == page->bitmap.bm_w)) {
				textureFreeQueue.push_back(*ptex);
				*ptex = SDL_CreateGPUTexture(rendererState.device, &tci);
			}

		} else {
			*ptex = SDL_CreateGPUTexture(rendererState.device, &tci);
		}

		if (*ptex == NULL) {
			Error("Error creating GPU texture for texture page: %s", SDL_GetError());
		}

		if (rendererState.textureTransferBuffer.memoryMap == NULL || rendererState.textureTransferOffset + bmSize > rendererState.textureTransferBuffer.size) {

			if (rendererState.textureTransferBuffer.memoryMap)
				FreeTransferBuffer(rendererState.textureTransferBuffer);

			uint32_t msize = rendererState.targetTextureTransferSize; 
			if (msize < bmSize * sizeof(float)) {
				Int3(); //This shouldn't happen
				msize = bmSize * sizeof(float);
			}
			msize = std::bit_ceil(msize);

			SDL_GPUTransferBufferCreateInfo tbci {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = msize
			};

			rendererState.textureTransferBuffer = CreateTransferBuffer(&tbci, true);
			rendererState.textureTransferOffset = 0;

			mprintf((0, "Reallocated texture transfer buffer"));

		}

		size_t bmLen = bmSize * sizeof(*page->convertedData);

		memcpy(rendererState.textureTransferBuffer.memoryMap + rendererState.textureTransferOffset, page->convertedData, bmLen);

		SDL_GPUTextureTransferInfo tti {
			.transfer_buffer = rendererState.textureTransferBuffer.buffer,
			.offset = rendererState.textureTransferOffset,
			.pixels_per_row = (uint32_t)bm.bm_w
		};

		SDL_GPUTextureRegion tr {
			.texture = *ptex,
			.w = (uint32_t)bm.bm_w,
			.h = (uint32_t)bm.bm_h,
			.d = 1
		};

		SDL_UploadToGPUTexture(cpass, &tti, &tr, true);

		rendererState.textureTransferOffset += bmLen;
		
		if (secondary) {
			rendererState.numTexturesInPage.secondary[0] = 1;
			rendererState.numTexturesInPage.secondary[1] = 1;
		} else {
			rendererState.numTexturesInPage.primary[0] = 1;
			rendererState.numTexturesInPage.primary[1] = 1;
		}

	}

	void DrawBatch(SDL_GPURenderPass* currentRenderPass, SDL_GPUCopyPass* currentCopyPass) {

		uint32_t vsize = worldVertices.size() * sizeof(*worldVertices.data());
		uint32_t isize = worldIndices.size() * sizeof(*worldIndices.data());

		SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX,
			.size = vsize + isize
		};

		if (rendererState.drawTransferBuffer.memoryMap == NULL || rendererState.drawTransferOffset + vsize + isize > rendererState.drawTransferBuffer.size) {
		
			if (rendererState.drawTransferBuffer.memoryMap)
				FreeTransferBuffer(rendererState.drawTransferBuffer);

			uint32_t msize = Segments.size() * 8 * (sizeof(WorldVertex) + sizeof(uint32_t)) * 12;
			if (msize < vsize + isize) {
				Int3(); //Shouldn't ever happen, but just to be safe...
				msize = vsize + isize;
			}

			msize = std::bit_ceil(msize);

			SDL_GPUTransferBufferCreateInfo tbci {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = msize
			};

			rendererState.drawTransferBuffer = CreateTransferBuffer(&tbci, true);
			rendererState.drawTransferOffset = 0;

			mprintf((0, "Reallocated vertex transfer buffer"));

		}

		SDL_GPUBuffer* drawBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
		if (drawBuffer == NULL)
			Error("Error creating vertex buffer: %s", SDL_GetError());

		memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset, worldVertices.data(), vsize);
		memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset + vsize, worldIndices.data(), isize);

		SDL_GPUTransferBufferLocation tbl {
			.transfer_buffer = rendererState.drawTransferBuffer.buffer,
			.offset = rendererState.drawTransferOffset
		};

		SDL_GPUBufferRegion br {
			.buffer = drawBuffer,
			.offset = 0,
			.size = vsize + isize
		};

		SDL_UploadToGPUBuffer(currentCopyPass, &tbl, &br, true);

		SDL_GPUBufferBinding bb {
			.buffer = drawBuffer,
			.offset = 0
		};
		SDL_BindGPUVertexBuffers(currentRenderPass, 0, &bb, 1);

		//bb.buffer = indexBuffer;
		bb.offset = vsize;
		SDL_BindGPUIndexBuffer(currentRenderPass, &bb, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		SDL_GPUTextureSamplerBinding tsb[] { {
			.texture = rendererState.secondaryTexture,
			.sampler = rendererState.defaultSampler
		}, {
			.texture = rendererState.primaryTexture,
			.sampler = rendererState.defaultSampler
		} };
		SDL_BindGPUFragmentSamplers(currentRenderPass, 0, tsb, 2);

		SDL_GPUBuffer* storageBuffers[] { rendererState.paletteBuffer };// , rendererState.paletteBuffer}; //TODO: need portal buffer
		SDL_BindGPUFragmentStorageBuffers(currentRenderPass, 0, storageBuffers, SDL_arraysize(storageBuffers));

		SDL_DrawGPUIndexedPrimitives(currentRenderPass, worldIndices.size(), 1, 0, 0, 0);
		
		SDL_ReleaseGPUBuffer(rendererState.device, drawBuffer);

		rendererState.drawTransferOffset += vsize + isize;

		worldVertices.clear();
		worldIndices.clear();

	}

	void DispatchMineDrawCalls(SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {

		static int d1 = 30;

		ViewTarget view = VT_NONE;

		SDL_GPUColorTargetInfo* cct;
		SDL_GPUDepthStencilTargetInfo* cdt;

		//SDL_GPUBuffer* currentPortalBuffer;

		TexturePage* primaryPage;
		TexturePage* secondaryPage;
		
		SDL_BindGPUGraphicsPipeline(rpass, rendererState.worldPipeline);
		
		for (auto& cp : sideDrawCalls) {

			const auto& [newPrimary, newSecondary, newView] = cp.first;

			Assert(newView != VT_NONE);

			bool swap = false;

			if (newView != view) {
				swap = true;
			}

			if (newPrimary != rendererState.primaryPage) {
				swap = true;
			} 

			if (newSecondary != rendererState.secondaryPage && newSecondary != NULL) {
				swap = true;
			}

			if (swap) {// = SDL_CreateGPUTexture(rendererState.device, &tci);

				if (worldVertices.size() > 0) {
					DrawBatch(rpass, cpass);
				}

				UploadTexturePage(newPrimary, cpass, false);
				if (newSecondary)
					UploadTexturePage(newSecondary, cpass, true);

				SDL_PushGPUFragmentUniformData(combuf, 0, &rendererState.numTexturesInPage, sizeof(rendererState.numTexturesInPage));
				
				if (view != newView) {

					view = newView;

					if (view == VT_MAIN) {
						UploadVertexMatrix(combuf, &rendererState.mainProjectionMatrix, MID_PROJ);
						UploadVertexMatrix(combuf, &rendererState.mainViewMatrix, MID_VIEW);
						cct = &rendererState.mainCTarget;
						cdt = &rendererState.mainDTarget;
					} else {

						if (view == VT_LEFT) {
							UploadVertexMatrix(combuf, &rendererState.subViewMatrixL, MID_VIEW);
							cct = &rendererState.leftCTarget;
							cdt = &rendererState.leftDTarget;
						}
						else {
							UploadVertexMatrix(combuf, &rendererState.subViewMatrixR, MID_VIEW);
							cct = &rendererState.rightCTarget;
							cdt = &rendererState.rightDTarget;
						}

						UploadVertexMatrix(combuf, &rendererState.subProjectionMatrix, MID_PROJ);
						
					}

				}
			}

			for (auto& Draw : cp.second)
				Draw(combuf);
				
		}

		if (worldVertices.size() > 0) {
			DrawBatch(rpass, cpass);
		}

	}

	void EmplaceObjCall(const ViewTarget target, ObjDrawCall& call) {
		
		std::lock_guard g(objDrawCallMutex);

		if (objDrawCalls.count(target) == 0) {
			objDrawCalls[target] = std::vector<ObjDrawCall>();
		}

		std::vector<ObjDrawCall>& calls = objDrawCalls[target];
		calls.push_back(call);

	}

#pragma region Based on g3 model code
	void rotate_point_list(g3s_point* dest, vms_vector* src, int n)
	{
		while (n--)
			g3_rotate_point(dest++, src++);
	}

	/*bool CheckNormal(vms_vector* pt, vms_vector* norm) {
		


	}*/
	
	void RenderPolymodelSub(const ViewTarget target, const int segno, const void* model_ptr, const std::vector<grs_bitmap*>& model_bitmaps, const std::vector<short>& bitmapIDs, const vms_angvec anim_angles[], const fix model_light, const fix glow_values[], const vms_vector* origin, const vms_matrix* rotation) {
	
		uint8_t* p = (uint8_t*)model_ptr;
		int current_poly = 0;

		int glow_num = -1;		//glow off by default

		int loop = 0;

		while (w(p) != OP_EOF)

			switch (w(p))
			{

			case OP_DEFPOINTS:
			{
				int n = w(p + 2);
				if (n > Interp_point_list.size())
					Interp_point_list.resize(n);

				vms_vector* v = vp(p + 4);
				
				for (int i = 0; i < n; i++) {
					Interp_point_list[i] = {
						.p3_vec = *v
					};
					v++;
				}

				p += n * sizeof(struct vms_vector) + 4;
				break;
			}

			case OP_DEFP_START:
			{
				int n = w(p + 2);
				int s = w(p + 4);

				if (s + n > Interp_point_list.size())
					Interp_point_list.resize(s + n);
				
				vms_vector* v = vp(p + 8);

				for (int i = 0; i < n; i++) {
					Interp_point_list[i + s] = {
						.p3_vec = *v
					};
					v++;
				}

				p += n * sizeof(struct vms_vector) + 8;

				break;
			}

			case OP_FLATPOLY:
			{

				int light = 0;
				InterpColor color;
				int nv = w(p + 2); 

				//Assert(nv < MAX_POINTS_PER_POLY);
				if (nv > point_list.size())
					point_list.resize(nv);

				//if (g3_check_normal_facing(vp(p + 4), vp(p + 16)) > 0)
				{
					int i;
					if (currentGame == G_DESCENT_2) {
						color = interp_color_table[w(p + 28)];
						if (glow_num != -1)
						{
							light = glow_values[glow_num];
							glow_num = -1;
							if (light == -2)
								color = {
									.pal_entry = 255,
									.rgb15 = 0xffff
								};
						}
					}
					else {
						color = interp_color_table[w(p + 28)];
					}

					if (light != -3)
					{
						//gr_setcolor(drawindex);

						for (i = 0; i < nv; i++)
							point_list[i] = Interp_point_list.data() + wp(p + 30)[i];
						//g3_draw_poly(nv, point_list.data());
					}
				}

				p += 30 + ((nv & ~1) + 1) * 2;
				break;
			}

			case OP_TMAPPOLY:
			{
				int nv = w(p + 2);
				g3s_uvl* uvl_list;

				//Assert(nv < MAX_POINTS_PER_POLY);
				if (nv < point_list.size())
					point_list.resize(nv);

				//if (g3_check_normal_facing(vp(p + 4), vp(p + 16)) > 0)
				{
					int i;
					fix light;

					//calculate light from surface normal

					if (glow_num < 0) //no glow
					{
						light = -vm_vec_dot(&View_matrix.fvec, vp(p + 16));
						light = f1_0 / 4 + (light * 3) / 4;
						light = fixmul(light, model_light);
					}
					else //yes glow
					{
						light = glow_values[glow_num];
						glow_num = -1;
					}

					//now poke light into l values

					uvl_list = (g3s_uvl*)(p + 30 + ((nv & ~1) + 1) * 2);

					g3s_point* verts = new g3s_point[nv];

					for (i = 0; i < nv; i++) {
						verts[i] = Interp_point_list[wp(p + 30)[i]];
						//uvl_list[i].l = light;
						verts[i].p3_u = uvl_list[i].u;
						verts[i].p3_v = uvl_list[i].v;
						verts[i].p3_l = light;
					}

					short texind = w(p + 28);

					auto& tp = rendererState.tpageLocations[bitmapIDs[texind]];

					SideDrawKey k {
							&rendererState.tpages[tp.first],
							NULL,
							target
					};

					{

						std::lock_guard lock(modelDrawCallMutex);

						if (sideDrawCalls.count(k) == 0) {
							sideDrawCalls[k] = std::vector<SideDrawCall>();
						}

						std::vector<ObjDrawCall>& calls = modelDrawCalls[k];

						const vms_vector& pos = *origin;
						const vms_matrix& rot = *rotation;

						calls.emplace_back([tp, segno, verts, nv, light, pos, rot](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {

							/*if (rendererState.drawCallObjID != -1)*/ {

								mat4f matrix;

								memcpy(matrix, M4_IDENTITY_MATRIX, sizeof(matrix));

								for (int m = 0; m < 3; m++) {
									for (int n = 0; n < 3; n++) {
										matrix[m][n] = f2fl(rot[m][n]);
									}

									matrix[3][m] = f2fl(pos[m]);
								}

								UploadVertexMatrix(combuf, &M4_IDENTITY_MATRIX, MID_ANIM);
								UploadVertexMatrix(combuf, &matrix, MID_MODEL);

							}
							//rendererState.drawCallObjID = -1;

							//const auto& segment = Segments[segno];

							int vertStart = worldVertices.size();

							WorldVertex* wverts = new WorldVertex[nv];
							uint32_t* winds = new uint32_t[3 * (nv - 2)];

							for (int i = 0; i < nv; i++) {

								auto& vert = verts[i];

								float lightR, lightG, lightB;
								lightR = lightG = lightB = 1.f;// f2fl(light);

								if (cheatValues[CI_RAVE]) {

									SDL_srand((uint64_t)pos.x * 0xFFFF + (uint64_t)pos.y * 0x00FF + pos.z + GameTime);

									lightR = sqrtf(lightR);
									lightG = sqrtf(lightG);
									lightB = sqrtf(lightB);

									lightR *= SDL_randf() * 2.f;
									lightG *= SDL_randf() * 2.f;
									lightB *= SDL_randf() * 2.f;

								}

								/*lightR *= lightFactorR;
								lightG *= lightFactorG;
								lightB *= lightFactorB;*/
								
								wverts[i] = WorldVertex {
									.pos = {
										f2fl(vert.p3_vec.x),
										f2fl(vert.p3_vec.y),
										f2fl(vert.p3_vec.z)
									},
									.uv = {
										f2fl(vert.p3_u),
										f2fl(vert.p3_v),
									},
									.props = {
										segno,
										tp.second,
										-1,
										0 
									},
									.colormod = {
										lightR,
										lightG,
										lightB,
										1.f
									}
								};

							}

							for (int i = 0; i < nv - 2; i++) {
								winds[i * 3 + 0] = 0;
								winds[i * 3 + 1] = i + 1;
								winds[i * 3 + 2] = i + 2;
							}

							uint32_t vsize = nv * sizeof(*wverts);
							uint32_t isize = (nv - 2) * 3 * sizeof(*winds);

							SDL_GPUBufferCreateInfo bci {
								.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX,
								.size = vsize + isize
							};

							if (rendererState.drawTransferBuffer.memoryMap == NULL || rendererState.drawTransferOffset + vsize + isize > rendererState.drawTransferBuffer.size) {

								if (rendererState.drawTransferBuffer.memoryMap)
									FreeTransferBuffer(rendererState.drawTransferBuffer);

								uint32_t msize = Segments.size() * 8 * (sizeof(WorldVertex) + sizeof(uint32_t)) * 16;
								if (msize < vsize + isize) {
									Int3(); //Shouldn't ever happen, but just to be safe...
									msize = vsize + isize;
								}

								msize = std::bit_ceil(msize);

								SDL_GPUTransferBufferCreateInfo tbci {
									.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
									.size = msize
								};

								rendererState.drawTransferBuffer = CreateTransferBuffer(&tbci, true);
								rendererState.drawTransferOffset = 0;

								mprintf((0, "Reallocated vertex transfer buffer in polyobj tmap"));

							}

							SDL_GPUBuffer* drawBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
							if (drawBuffer == NULL)
								Error("Error creating vertex buffer: %s", SDL_GetError());

							memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset, wverts, vsize);
							memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset + vsize, winds, isize);

							SDL_GPUTransferBufferLocation tbl {
								.transfer_buffer = rendererState.drawTransferBuffer.buffer,
								.offset = rendererState.drawTransferOffset
							};

							SDL_GPUBufferRegion br {
								.buffer = drawBuffer,
								.offset = 0,
								.size = vsize + isize
							};

							SDL_UploadToGPUBuffer(cpass, &tbl, &br, true);

							SDL_GPUBufferBinding bb {
								.buffer = drawBuffer,
								.offset = 0
							};
							SDL_BindGPUVertexBuffers(rpass, 0, &bb, 1);

							//bb.buffer = indexBuffer;
							bb.offset = vsize;
							SDL_BindGPUIndexBuffer(rpass, &bb, SDL_GPU_INDEXELEMENTSIZE_32BIT);

							SDL_GPUTextureSamplerBinding tsb[] { {
								.texture = rendererState.secondaryTexture,
								.sampler = rendererState.defaultSampler
							}, {
								.texture = rendererState.primaryTexture,
								.sampler = rendererState.defaultSampler
							} };
							SDL_BindGPUFragmentSamplers(rpass, 0, tsb, 2);

							SDL_GPUBuffer* storageBuffers[] { rendererState.paletteBuffer };// , rendererState.paletteBuffer}; //TODO: need portal buffer
							SDL_BindGPUFragmentStorageBuffers(rpass, 0, storageBuffers, SDL_arraysize(storageBuffers));

							SDL_DrawGPUIndexedPrimitives(rpass, (nv - 2) * 3, 1, 0, 0, 0);

							SDL_ReleaseGPUBuffer(rendererState.device, drawBuffer);

							rendererState.drawTransferOffset += vsize + isize;

							delete[] verts;
							delete[] wverts;
							delete[] winds;

						});

						//g3_draw_tmap(nv, point_list.data(), uvl_list, model_bitmaps[w(p + 28)]);

					}
				}

				p += 30 + ((nv & ~1) + 1) * 2 + nv * 12;

				break;
			}

			case OP_SORTNORM:

				/*vms_matrix mat;
				vms_matrix rotated;

				if (anim_angles)
					vm_angles_2_matrix(&mat, const_cast<vms_angvec*>(anim_angles) + w(p + 2));
				else
					mat = IDENTITY_MATRIX;

				vms_vector pos = *vp(p + 4);
				vm_vec_add2(&pos, parentOrigin);*/

				//Hardware will handle sorting now

				//if (g3_check_normal_facing(vp(p + 16), vp(p + 4)) > 0) //facing
				//{
					//draw back then front
				RenderPolymodelSub(target, segno, p + w(p + 30), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, origin, rotation);
				RenderPolymodelSub(target, segno, p + w(p + 28), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, origin, rotation);
				/*}
				else //not facing.  draw front then back
				{
					RenderPolymodelSub(target, segno, p + w(p + 28), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, &pos, vm_matrix_x_matrix(&rotated, &mat, const_cast<vms_matrix*>(parentRotation)));
					RenderPolymodelSub(target, segno, p + w(p + 30), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, &pos, vm_matrix_x_matrix(&rotated, &mat, const_cast<vms_matrix*>(parentRotation)));
				}*/

				p += 32;
				break;

			case OP_RODBM:
			{

				g3s_point rod_bot_p, rod_top_p;

				g3_rotate_point(&rod_bot_p, vp(p + 20));
				g3_rotate_point(&rod_top_p, vp(p + 4));

				//g3_draw_rod_tmap(model_bitmaps[w(p + 2)], &rod_bot_p, w(p + 16), &rod_top_p, w(p + 32), f1_0);

				p += 36;
				break;
			}

			case OP_SUBCALL:
			{
				vms_matrix mat;
				vms_matrix rmat;

				if (anim_angles)
					vm_angles_2_matrix(&mat, const_cast<vms_angvec*>(anim_angles) + w(p + 2));
				else
					mat = IDENTITY_MATRIX;
				
				vms_vector* mpos = vp(p + 4);
				vms_vector rpos;
				vm_copy_transpose_matrix(&rmat, const_cast<vms_matrix*>(rotation));
				vm_vec_rotate(&rpos, mpos, &rmat);
				vm_vec_add2(&rpos, origin);

				RenderPolymodelSub(target, segno, p + w(p + 16), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, &rpos, vm_matrix_x_matrix(&rmat, &mat, const_cast<vms_matrix*>(rotation)));
				
				p += 20;
				break;
			}

			case OP_GLOW:

				if (glow_values)
					glow_num = w(p + 2);
				p += 4;
				break;

			default:
				Int3();
			}

	}
#pragma endregion

	void RenderPolymodel(const ViewTarget target, const int segno, const object& object, const vms_angvec anim_angles[], const int model_num, const int flags, const fix light, const short textureOverride, float visibility) {
	
		const vms_vector& pos = object.pos;
		const vms_matrix& orient = object.orient;

		polymodel& model = activeBMTable->models[model_num];

		thread_local std::vector<short> bitmapIDs;
		thread_local std::vector<grs_bitmap*> bitmaps;
		bitmapIDs.reserve(100);
		bitmapIDs.clear();
		bitmaps.reserve(100);
		bitmaps.clear();


		if (textureOverride >= 0) {

			for (int i = 0; i < model.n_textures; i++) {
				bitmapIDs.push_back(textureOverride);
				bitmaps.push_back(&activePiggyTable->gameBitmaps[textureOverride]);
			}

		} else {

			for (int i = 0; i < model.n_textures; i++) {
				short bmpID = activeBMTable->objectBitmaps[activeBMTable->objectBitmapPointers[model.first_texture + i]].index;
				bitmapIDs.push_back(bmpID);
				bitmaps.push_back(&activePiggyTable->gameBitmaps[bmpID]);
			}

		}

		fix glow[2] {
			f1_0 / 5,
			0
		};

		if (object.movement_type == MT_PHYSICS)
		{
			if (object.mtype.phys_info.flags & PF_USES_THRUST && object.type == OBJ_PLAYER && object.id == Player_num)
			{
				fix thrust_mag = vm_vec_mag_quick(&object.mtype.phys_info.thrust);
				glow[0] += (fixdiv(thrust_mag, Player_ship->max_thrust) * 4) / 5;
			}
			else
			{
				fix speed = vm_vec_mag_quick(&object.mtype.phys_info.velocity);
				glow[0] += (fixdiv(speed, MAX_VELOCITY) * 3) / 5;
			}
		}

		//set value for player headlight
		if (object.type == OBJ_PLAYER)
		{
			if (Players[object.id].flags & PLAYER_FLAGS_HEADLIGHT && !Endlevel_sequence)
				if (Players[object.id].flags & PLAYER_FLAGS_HEADLIGHT_ON)
					glow[1] = -2;		//draw white!
				else
					glow[1] = -1;		//draw normal color (grey)
			else
				glow[1] = -3;			//don't draw
		}

		RenderPolymodelSub(target, segno, model.model_data, bitmaps, bitmapIDs, anim_angles, light, glow, &object.pos, &object.orient);

		bitmaps.clear();
	
	}

	void RenderPolyObj(const ViewTarget target, const object& object, const int segno, const float visibility) {
		
		const polyobj_info& pinf = object.rtype.pobj_info; 

		short override = -1;
		if (pinf.tmap_override >= 0) {
			override = activeBMTable->textures[pinf.tmap_override].index;
		}

		RenderPolymodel(target, segno, object, pinf.anim_angles, pinf.model_num, 0, F1_0, override, visibility);

	}
	
	void RenderObject(const ViewTarget target, const int segno, const int objno) {

		const object& object = Objects[objno];
		uint8_t rtypeid = object.render_type;

		ObjDrawCall call;

		switch (rtypeid) {

			case RT_POLYOBJ: {

				float visibility = (object.type == OBJ_PLAYER && Players[object.id].flags & PLAYER_FLAGS_CLOAKED) ? 0.f : 1.f;

				if (object.type == OBJ_ROBOT) {
					
					const ai_static& ais = object.ctype.ai_info;
					const ai_local& ail = Ai_local_info[objno];
				
					if (ais.CLOAKED == RI_CLOAKED_ALWAYS)
						visibility = 0.f;
					else if (ais.CLOAKED == RI_CLOAKED_EXCEPT_FIRING)
						visibility = 0.f; //TODO: scale by fire time
					
				}

				RenderPolyObj(target, object, segno, visibility); //Will emplace its own calls
				call = [](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {};
				break;

			}

			case RT_POWERUP: {

				call = [objno, rtypeid](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {



				};

				break;

			}

			case RT_FIREBALL: {

				call = [objno, rtypeid](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {



				};

				break;

			}

			case RT_HOSTAGE: {

				call = [objno, rtypeid](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {



				};

				break;

			}

			case RT_LASER: {

				call = [objno, rtypeid](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {



				};

				break;

			}

			case RT_MORPH: {

				call = [objno, rtypeid](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {



				};

				break;

			}

			case RT_WEAPON_VCLIP: {

				call = [objno, rtypeid](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {



				};

				break;

			}

		}

		EmplaceObjCall(target, call);

	}

	void RenderSide(const ViewTarget target, const int segno, const int sideno) {

		const segment& segment = Segments[segno];
		const side& side = segment.sides[sideno];

		if (side.tmap_num >= activeBMTable->textures.size()) {
			mprintf((1, "Invalid texture! Segment %d side %d, skipping...\n", segno, sideno));
			return;
		}

		short texind1 = activeBMTable->textures[side.tmap_num].index;
		short texind2 = activeBMTable->textures[side.tmap_num2 & 0x3FFF].index;

		auto& tp1 = rendererState.tpageLocations[texind1];
		auto& tp2 = rendererState.tpageLocations[texind2];

		float timeWithinSecond = f2fl(GameTime % F1_0);
		
		float lightFactorR = 1.f;
		float lightFactorG = 1.f;
		float lightFactorB = 1.f;

		if (Control_center_destroyed) {
			
			lightFactorB = timeWithinSecond;
			if (lightFactorB > 0.5f)
				lightFactorB = 1 - lightFactorB;

			lightFactorG = lightFactorB = sinf(lightFactorB * 3.1415926536f);
			lightFactorR = sqrtf(lightFactorB) * 0.75f + 0.25f;

		}

		SideDrawKey k {
			&rendererState.tpages[tp1.first],
			&rendererState.tpages[tp2.first],
			target
		};

		{
			std::lock_guard g(sideDrawCallMutex);

			if (sideDrawCalls.count(k) == 0) {
				sideDrawCalls[k] = std::vector<SideDrawCall>();
			}

			std::vector<SideDrawCall>& calls = sideDrawCalls[k];

			//TODO lock the vector
			calls.emplace_back([tp1, tp2, segno, sideno, lightFactorR, lightFactorG, lightFactorB](SDL_GPUCommandBuffer* combuf) {

				if (rendererState.drawCallObjID != -1) {
					UploadVertexMatrix(combuf, &M4_IDENTITY_MATRIX, MID_ANIM);
					UploadVertexMatrix(combuf, &M4_IDENTITY_MATRIX, MID_MODEL);
				}
				rendererState.drawCallObjID = -1;

				const auto& segment = Segments[segno];
				const auto& side = segment.sides[sideno];
				const auto& sideverts = Side_to_verts[sideno];

				int vertStart = worldVertices.size();
				
				for (int i = 0; i < MAX_VERTICES_PER_POLY; i++) {

					auto& vert = Vertices[segment.verts[sideverts[i]]];

					float lightR, lightG, lightB;
					lightR = lightG = lightB = f2fl(side.uvls[i].l);

					if (cheatValues[CI_RAVE]) {

						SDL_srand((uint64_t)vert.x * 0xFFFF + (uint64_t)vert.y * 0x00FF + vert.z + GameTime);

						lightR = sqrtf(lightR);
						lightG = sqrtf(lightG);
						lightB = sqrtf(lightB);

						lightR *= SDL_randf() * 2.f;
						lightG *= SDL_randf() * 2.f;
						lightB *= SDL_randf() * 2.f;

					} 
					
					lightR *= lightFactorR;
					lightG *= lightFactorG;
					lightB *= lightFactorB;

					worldVertices.emplace_back(WorldVertex {
						.pos = {
							f2fl(vert.x),
							f2fl(vert.y),
							f2fl(vert.z),
						},
						.uv = {
							f2fl(side.uvls[i].u),
							f2fl(side.uvls[i].v),
						},
						.props = {
							segno,
							tp1.second,
							(side.tmap_num2 & 0x3ff) != 0 ? tp2.second : -1,
							((side.tmap_num2 & 0xC000) >> 14) & 3
						},
						.colormod = {
							lightR,
							lightG,
							lightB,
							1.f
						}
					});

				}

				if (side.type == SIDE_IS_TRI_13) {
					worldIndices.push_back(vertStart + 0);
					worldIndices.push_back(vertStart + 1);
					worldIndices.push_back(vertStart + 3);
					worldIndices.push_back(vertStart + 1);
					worldIndices.push_back(vertStart + 2);
					worldIndices.push_back(vertStart + 3);
				}
				else {
					worldIndices.push_back(vertStart + 0);
					worldIndices.push_back(vertStart + 1);
					worldIndices.push_back(vertStart + 2);
					worldIndices.push_back(vertStart + 0);
					worldIndices.push_back(vertStart + 2);
					worldIndices.push_back(vertStart + 3);
				}

			});

		}

	}

	void EndRenderFrame() {

		if (rendererState.mainCommandBuffer == NULL) {
			mprintf((1, "Tried to complete rendering that never started!"));
			return;
		}

		SDL_GPUBlitInfo bi {
			.source = {
				.texture = rendererState.mainCTarget.texture,
				.x = 0,
				.y = 0,
				.w = rendererState.renderWidth,
				.h = rendererState.renderHeight,
			},
			.destination = {
				.texture = rendererState.windowCTarget.texture,
				.x = 0,
				.y = 0,
				.w = rendererState.renderWidth,
				.h = rendererState.renderHeight,
			},
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.cycle = true
		};

		/*if (rendererState.activeDrawFence) {
			SDL_WaitForGPUFences(rendererState.device, false, &rendererState.activeDrawFence, 1);
			SDL_ReleaseGPUFence(rendererState.device, rendererState.activeDrawFence);
			rendererState.activeDrawFence = NULL;
		}*/

		if (ExtGameStatus == GAMESTAT_RUNNING && mineRenderingReady) {

			mineRenderingReady = false;

			rendererState.drawCallObjID = -2;

			SDL_GPUFence* portalFences[] = {
				rendererState.leftWindowPortalListBuilt.get(),
				rendererState.rightWindowPortalListBuilt.get(),
				rendererState.mainPortalListBuilt.get(),
			};

			if (!SDL_WaitForGPUFences(rendererState.device, true, portalFences, 3)) {
				mprintf((1, "Error waiting for portal lists!\n"));
			}

			SDL_ReleaseGPUFence(rendererState.device, portalFences[0]);
			SDL_ReleaseGPUFence(rendererState.device, portalFences[1]);
			SDL_ReleaseGPUFence(rendererState.device, portalFences[2]);
			
			SDL_GPUCommandBuffer* mainCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);
			SDL_GPURenderPass* mainRenderPass = SDL_BeginGPURenderPass(mainCommandBuffer, &rendererState.mainCTarget, 1, &rendererState.mainDTarget);

			SDL_GPUCommandBuffer* mainCopyBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);
			SDL_GPUCopyPass* mainCopyPass = SDL_BeginGPUCopyPass(mainCopyBuffer);

			DispatchMineDrawCalls(mainCommandBuffer, mainRenderPass, mainCopyPass);

			for (auto& call : modelDrawCalls) {
				auto& [tp, _, __] = call.first;
				UploadTexturePage(tp, mainCopyPass);
				SDL_PushGPUFragmentUniformData(mainCommandBuffer, 0, &rendererState.numTexturesInPage, sizeof(rendererState.numTexturesInPage));
				for (auto& Draw : call.second)
					Draw(mainCommandBuffer, mainRenderPass, mainCopyPass);
			}

			SDL_EndGPUCopyPass(mainCopyPass);
			SDL_SubmitGPUCommandBuffer(mainCopyBuffer);

			SDL_EndGPURenderPass(mainRenderPass);
			SDL_SubmitGPUCommandBuffer(mainCommandBuffer);

			SDL_BlitGPUTexture(rendererState.mainCommandBuffer, &bi);

		}

		SDL_SubmitGPUCommandBuffer(rendererState.mainCommandBuffer);
		rendererState.mainCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);

		uint32_t windowWidth, windowHeight;
		
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(rendererState.mainCommandBuffer, gameWindow, &rendererState.windowTexture, &windowWidth, &windowHeight))
			mprintf((1, "Error acquiring swapchain texture: %s", SDL_GetError()));

		if (rendererState.windowTexture) {

			bi.source.texture = rendererState.windowCTarget.texture;
			bi.destination.texture = rendererState.windowTexture;
			bi.destination.w = windowWidth;
			bi.destination.h = windowHeight;

			SDL_BlitGPUTexture(rendererState.mainCommandBuffer, &bi);

		}

		//rendererState.activeDrawFence = SDL_SubmitGPUCommandBufferAndAcquireFence(rendererState.mainCommandBuffer);
		SDL_SubmitGPUCommandBuffer(rendererState.mainCommandBuffer);
		rendererState.mainCommandBuffer = NULL;

		for (auto& tex : textureFreeQueue) {
			SDL_ReleaseGPUTexture(rendererState.device, tex);
		}

		textureFreeQueue.clear();

	}

	void ShutdownRenderAPI() {

		if (rendererState.activeDrawFence)
			SDL_ReleaseGPUFence(rendererState.device, rendererState.activeDrawFence);

		SDL_ReleaseGPUGraphicsPipeline(rendererState.device, rendererState.screenPipeline);
		SDL_ReleaseGPUGraphicsPipeline(rendererState.device, rendererState.worldPipeline);

		SDL_ReleaseGPUTexture(rendererState.device, rendererState.windowDTarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.windowCTarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.mainCTarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.mainDTarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.leftCTarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.leftDTarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.rightCTarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.rightDTarget.texture);

		SDL_ReleaseGPUTexture(rendererState.device, rendererState.primaryTexture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.secondaryTexture);

		SDL_ReleaseGPUShader(rendererState.device, rendererState.screenVert);
		SDL_ReleaseGPUShader(rendererState.device, rendererState.screenFrag);
		SDL_ReleaseGPUShader(rendererState.device, rendererState.worldVert);
		SDL_ReleaseGPUShader(rendererState.device, rendererState.worldFrag);

		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.paletteBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.mainPortalBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.leftPortalBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.rightPortalBuffer);

		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.screenIndBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.screenVertBuffer);

		SDL_ReleaseGPUSampler(rendererState.device, rendererState.defaultSampler);

		FreeTransferBuffer(rendererState.drawTransferBuffer);
		FreeTransferBuffer(rendererState.textureTransferBuffer);

		SDL_ReleaseWindowFromGPUDevice(rendererState.device, gameWindow);
		SDL_DestroyGPUDevice(rendererState.device);

	}

	SDL_GPUFence* BuildGPUPortalListThread(const std::vector<short> segments, const vms_vector pos, const vms_vector dir, const ViewTarget target, SDL_GPUBuffer** destBuffer) {

		bool rearActive = (Cockpit_3d_view[0] == CV_REAR || Cockpit_3d_view[1] == CV_REAR);

		SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
		
		//mprintf((1, "Portal list building not ready!"));

		//SDL_GPUStorageBufferReadWriteBinding* verts = 

		//SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cbuf, NULL, 0, , 1);

		return SDL_SubmitGPUCommandBufferAndAcquireFence(cbuf);

	}

}