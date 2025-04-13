/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

//#define MOCK_FUTURE

#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <tuple>

#include <SDL_gpu.h>

#include "2d/gr.h"
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


#ifndef MOCK_FUTURE
#include <future>
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
		//SDL_GPUTexture* texture = NULL;
	};

	struct TransferBuffer {
		SDL_GPUTransferBuffer* buffer = NULL;
		void* memoryMap = NULL;
	};

	struct WorldVertex {
		float pos[3];
		float uvl[3];
		int32_t props[4];
	};

	typedef float (mat4f[4])[4];

#define M4_IDENTITY_MATRIX_MACRO {	\
		{1,0,0,0},					\
		{0,1,0,0},					\
		{0,0,1,0},					\
		{0,0,0,1},					\
}

	constexpr mat4f M4_IDENTITY_MATRIX M4_IDENTITY_MATRIX_MACRO;

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

		//SDL_GPUTextureFormat windowFormat;
		
		int drawCallObjID = -2;

		SDL_GPUColorTargetInfo windowCTarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUDepthStencilTargetInfo windowDTarget {
			.texture = NULL,
			.clear_depth = 0,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUColorTargetInfo mainCTarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = SDL_GPU_LOADOP_LOAD,
			.store_op = SDL_GPU_STOREOP_STORE,
			.cycle = false
		};

		SDL_GPUDepthStencilTargetInfo mainDTarget {
			.texture = NULL,
			.clear_depth = 0,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUColorTargetInfo leftCTarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = SDL_GPU_LOADOP_LOAD,
			.store_op = SDL_GPU_STOREOP_STORE,
			.cycle = false
		};

		SDL_GPUDepthStencilTargetInfo leftDTarget {
			.texture = NULL,
			.clear_depth = 0,
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUColorTargetInfo rightCTarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = SDL_GPU_LOADOP_LOAD,
			.store_op = SDL_GPU_STOREOP_STORE,
			.cycle = false
		};

		SDL_GPUDepthStencilTargetInfo rightDTarget {
			.texture = NULL,
			.clear_depth = 0,
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
	typedef std::function<void(SDL_GPUCommandBuffer*)> drawcall;
	typedef std::tuple<TexturePage*, TexturePage*, ViewTarget> drawkey;

	struct dkeyCompare {

		bool operator()(const drawkey a, const drawkey b) const {
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
	
	//union num32 { float f; int32_t i; };

	std::map<drawkey, std::vector<drawcall>, dkeyCompare> worldDrawCalls;
	std::vector<WorldVertex> worldVertices;
	std::vector<uint32_t> worldIndices;

	bool mineRenderingReady = false;

	const std::launch ASYNC_POLICY = std::launch::deferred;
	const SDL_GPUTextureFormat TEXTURE_FORMAT = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

#pragma region SDLHelpers

	template <int bufN, int attrN> SDL_GPUGraphicsPipeline* CreateGraphicsPipeline(
		SDL_GPUShader* vertex, SDL_GPUShader* fragment, SDL_GPUPrimitiveType primitiveType,
		std::array<SDL_GPUVertexBufferDescription, bufN> buffers,
		std::array<SDL_GPUVertexAttribute, attrN> attributes
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

		buf.memoryMap = SDL_MapGPUTransferBuffer(rendererState.device, buf.buffer, cycle);
		if (buf.memoryMap == NULL) {
			mprintf((1, "Error mapping transfer buffer: %s", SDL_GetError()));
			return buf;
		}

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

		//InitCommandBuffer();
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
			//expandedPalette[i * 4 + 3] = 1;
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

		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);
		SDL_ReleaseGPUTransferBuffer(rendererState.device, tbuf.buffer);

		SDL_EndGPUCopyPass(pass);
		SDL_SubmitGPUCommandBuffer(upbuf);
		/*SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(upbuf);
		SDL_WaitForGPUFences(rendererState.device, false, &fence, 1);
		SDL_ReleaseGPUFence(rendererState.device, fence);*/

	}

	void UploadPalette() {
		UploadPalette(gr_palette);
	}

	void ResizeWindow() {
		//InitCommandBuffer();
		//if (!SDL_WaitAndAcquireGPUSwapchainTexture(rendererState.mainCommandBuffer, gameWindow, &rendererState.windowTexture, NULL, NULL))
		//	Error("Error acquiring swapchain texture: %s", SDL_GetError());

		uint32_t size;

		switch (Cockpit_mode) {
			
			/*
			
		if (Cockpit_mode == CM_FULL_COCKPIT)
			boxnum = (COCKPIT_PRIMARY_BOX)+win;
		else if (Cockpit_mode == CM_STATUS_BAR)
			boxnum = (SB_PRIMARY_BOX)+win;
			*/

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

		static SDL_GPUTextureCreateInfo tciC {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = TEXTURE_FORMAT,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.width = size,
			.height = size,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		static SDL_GPUTextureCreateInfo tciD {
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

		//UpdateProjectionMatrices((float)rendererState.renderWidth / rendererState.renderHeight);
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
			} }
		);

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

		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);
		SDL_ReleaseGPUTransferBuffer(rendererState.device, tbuf.buffer);

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

		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);
		SDL_ReleaseGPUTransferBuffer(rendererState.device, tbuf.buffer);

	}

	void InitWorldRendering(SDL_GPUCopyPass* cpass) {

		rendererState.worldPipeline = CreateGraphicsPipeline<1, 3>(rendererState.worldVert, rendererState.worldFrag, SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
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
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = (uint32_t)offsetof(WorldVertex, uvl),
			}, SDL_GPUVertexAttribute {
				.location = 2,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_INT4,
				.offset = (uint32_t)offsetof(WorldVertex, props),
			} }
		);

		if (rendererState.worldPipeline == NULL) {
			Error("Error creating world pipeline: %s", SDL_GetError());
		}

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

		if (!good) {
			//mprintf((1, "Last error: %s\n", SDL_GetError()));
			return 4;
		}

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
		if (!SDL_SubmitGPUCommandBuffer(rendererState.mainCommandBuffer)) {
			mprintf((1, "Error submitting setup commands: %s\n", SDL_GetError()));
			return 7;
		}
		rendererState.mainCommandBuffer = NULL;

		//memset(rendererState.mainProjectionMatrix, 0, sizeof(rendererState.mainProjectionMatrix));
		memcpy(rendererState.mainProjectionMatrix, M4_IDENTITY_MATRIX, sizeof(rendererState.mainProjectionMatrix));

		constexpr float depth = FAR_CLIP_F - NEAR_CLIP_F;

		rendererState.mainProjectionMatrix[1][1] = 1/tanf(VFOV_RAD_F / 2);
		rendererState.mainProjectionMatrix[0][0] = rendererState.mainProjectionMatrix[1][1];
		rendererState.mainProjectionMatrix[2][2] = FAR_CLIP_F / depth;
		rendererState.mainProjectionMatrix[2][3] = -(FAR_CLIP_F * NEAR_CLIP_F) / depth;
		rendererState.mainProjectionMatrix[3][2] = 1.f;
		rendererState.mainProjectionMatrix[3][3] = 0.f;
		
		memcpy(rendererState.subProjectionMatrix, rendererState.mainProjectionMatrix, sizeof(rendererState.subProjectionMatrix));

		worldVertices.reserve(5000);
		worldIndices.reserve(5000);

		//SyncCockpit();

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
				//Error("Invalid subwindow target specified! %d", target);
				Int3();

		}

		if (clone != target) {

			if (target == VT_LEFT && clone == VT_RIGHT)
				rendererState.cloneMode = VCM_CLONE_RL;
			else if (target == VT_RIGHT && clone == VT_LEFT)
				rendererState.cloneMode = VCM_CLONE_LR;
			else if (clone != VT_NONE)
				Error("Invalid subwindow clone mode! Target %d cloning %d", target, clone);

			*future = std::async(ASYNC_POLICY, +[]() {
				SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
				return SDL_SubmitGPUCommandBufferAndAcquireFence(cbuf);
			});

		} else {

			*future = std::async(ASYNC_POLICY, BuildGPUPortalListThread, segments, pos, dir, target, buffer);

		}
	}

	TexturePage* CreateTexturePage(grs_bitmap* bm) {
		
		TexturePage* page = new TexturePage();
		page->bitmap = *bm;
		return page;

	}
	
	void FreeTexturePage(TexturePage* page) {
		delete page;
	}

	void GenerateTexturePages() {
	
		rendererState.tpages.clear();
		rendererState.tpages.reserve(activePiggyTable->gameBitmaps.size());

		for (int i = 0; i < activePiggyTable->gameBitmaps.size(); i++) {
			auto bm = activePiggyTable->gameBitmaps[i];
			if (bm.bm_data == NULL)
				Int3();
			rendererState.tpages.push_back(TexturePage { bm });
			rendererState.tpageLocations[i] = std::pair(i, 0);
		}
	
	}

	void RenderScreenBitmap(grs_bitmap* bm) {

		//mprintf((0, "Drawing screen bitmap\n"));

		//mprintf((0, "%hd %hd %hhd %hhd %hhd", bm->bm_w, bm->bm_h, bm->bm_data[100], bm->bm_data[200], bm->bm_data[200] - bm->bm_data[100]));

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

		//SDL_memcpy(tbuf.memoryMap, bm->bm_data, bmSize); //Need to expand texture
		for (int i = 0; i < bmSize; i++) {
			floatMemMap[i] = bm->bm_data[i];
		}
		//mprintf((0, "%f %f\n", floatMemMap[100], floatMemMap[200]));
		
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

		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);
		SDL_ReleaseGPUTransferBuffer(rendererState.device, tbuf.buffer);
		SDL_EndGPUCopyPass(cpass);
		//SDL_SubmitGPUCommandBuffer(copycmd);
		SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(copycmd);
		if (fence == NULL) {
			Error("Error submitting bitmap copy commands: %s", SDL_GetError());
		}

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

		if (!SDL_WaitForGPUFences(rendererState.device, false, &fence, 1)){
			Error("Error waiting for bitmap texture upload: %s", SDL_GetError());
		}
		SDL_ReleaseGPUFence(rendererState.device, fence);

		//mprintf((0, SDL_GetError()));

		SDL_DrawGPUIndexedPrimitives(rpass, 4, 1, 0, 0, 0); 

		SDL_ReleaseGPUTexture(rendererState.device, bmTex);

		SDL_EndGPURenderPass(rpass);

	}

	void RenderScreenCanvas(grs_canvas* canvas) {
		RenderScreenBitmap(&canvas->cv_bitmap);
	}

	void PrepareMineRenderFrame() { // TODO: If in game, build and submit portal list. Also, determine if rear view mirrors need textures.
		
		worldDrawCalls.clear();
		InitCommandBuffer();

		rendererState.cloneMode = VCM_NO_CLONE;

		mineRenderingReady = true;

	}

	void UpdateMainView(const float aspect) {
		rendererState.mainProjectionMatrix[0][0] = rendererState.mainProjectionMatrix[1][1] / aspect;
	}

	/*void UpdateProjectionMatrices(const fix aspect) {
		UpdateProjectionMatrices(f2fl(aspect));
	}*/

	void SyncCockpit() {

		float aspect;

		switch (Cockpit_mode) {

			default:
			case CM_FULL_SCREEN:
			case CM_REAR_VIEW:
				aspect = rendererState.renderWidth / rendererState.renderHeight;
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
			(*matrix)[m][3] = f2fl(source->pos[m]);
		}

	}

	void UploadVertexMatrix(SDL_GPUCommandBuffer* cbuf, const mat4f* matrix, const MatrixID id) {
		SDL_PushGPUVertexUniformData(cbuf, id, *matrix, sizeof(*matrix));
	}

	void UploadTexturePage(TexturePage* page, const bool secondary = false) {
		
		Assert(page != NULL);

		grs_bitmap& bm = page->bitmap;
		InitCommandBuffer();

		SDL_GPUTexture** ptex;
		//TransferBuffer* ptbuf;
		
		if (secondary) {
			rendererState.secondaryPage = page;
			ptex = &rendererState.secondaryTexture;
			//ptbuf = &rendererState.secondaryBuffer;
		} else {
			rendererState.primaryPage = page;
			ptex = &rendererState.primaryTexture;
			//ptbuf = &rendererState.primaryBuffer;
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

		if (*ptex)
			SDL_ReleaseGPUTexture(rendererState.device, *ptex);
		*ptex = SDL_CreateGPUTexture(rendererState.device, &tci);

		if (*ptex == NULL) {
			Error("Error creating GPU texture for texture page: %s", SDL_GetError());
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

		//SDL_memcpy(tbuf.memoryMap, bm->bm_data, bmSize); //Need to expand texture
		for (int i = 0; i < bmSize; i++) {
			floatMemMap[i] = bm.bm_data[i];
		}
		//mprintf((0, "%f %f\n", floatMemMap[100], floatMemMap[200]));

		SDL_GPUCommandBuffer* copycmd = SDL_AcquireGPUCommandBuffer(rendererState.device);
		if (copycmd == NULL) {
			Error("Error acquiring bitmap transfer command buffer: %s", SDL_GetError());
		}

		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(copycmd);

		SDL_GPUTextureTransferInfo tti {
			.transfer_buffer = tbuf.buffer,
			.offset = 0,
			.pixels_per_row = (uint32_t)bm.bm_w
		};

		SDL_GPUTextureRegion tr {
			.texture = *ptex,
			.w = (uint32_t)bm.bm_w,
			.h = (uint32_t)bm.bm_h,
			.d = 1
		};

		SDL_UploadToGPUTexture(cpass, &tti, &tr, true);

		FreeTransferBuffer(tbuf);
		SDL_EndGPUCopyPass(cpass);
		SDL_SubmitGPUCommandBuffer(copycmd);

		if (secondary) {
			rendererState.numTexturesInPage.secondary[0] = 1;
			rendererState.numTexturesInPage.secondary[1] = 1;
		} else {
			rendererState.numTexturesInPage.primary[0] = 1;
			rendererState.numTexturesInPage.primary[1] = 1;
		}
		
	}

	void RenderBatch(SDL_GPURenderPass** currentPass) {

		SDL_GPUTextureCreateInfo tci {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = TEXTURE_FORMAT,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
		if (!cbuf)
			Error("Error creating copy command buffer: %s", SDL_GetError());
		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(cbuf);

		SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = (uint32_t)(worldVertices.size() * sizeof(*worldVertices.data())),
		};
		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = bci.size
		};
		SDL_GPUTransferBufferLocation tbl {
			.offset = 0
		};
		SDL_GPUBufferRegion br {
			.offset = 0
		};

		SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
		if (vertexBuffer == NULL)
			Error("Error creating vertex buffer: %s", SDL_GetError());
		br.buffer = vertexBuffer;
		br.size = bci.size;

		TransferBuffer vertTB = CreateTransferBuffer(&tbci, true);
		tbl.transfer_buffer = vertTB.buffer;
		memcpy(vertTB.memoryMap, worldVertices.data(), bci.size);
		SDL_UnmapGPUTransferBuffer(rendererState.device, vertTB.buffer);
		SDL_UploadToGPUBuffer(cpass, &tbl, &br, true);

		bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
		tbci.size = bci.size = (uint32_t)(worldIndices.size() * sizeof(*worldIndices.data()));

		SDL_GPUBuffer* indexBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
		if (indexBuffer == NULL)
			Error("Error creating index buffer: %s", SDL_GetError());
		br.buffer = indexBuffer;
		br.size = bci.size;

		TransferBuffer indTB = CreateTransferBuffer(&tbci, true);
		tbl.transfer_buffer = indTB.buffer;
		memcpy(indTB.memoryMap, worldIndices.data(), bci.size);
		SDL_UnmapGPUTransferBuffer(rendererState.device, indTB.buffer);
		SDL_UploadToGPUBuffer(cpass, &tbl, &br, true);

		SDL_EndGPUCopyPass(cpass);
		SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cbuf);
		SDL_WaitForGPUFences(rendererState.device, false, &fence, 1);
		SDL_ReleaseGPUFence(rendererState.device, fence);

		SDL_GPUBufferBinding bb {
			.buffer = vertexBuffer,
			.offset = 0
		};
		SDL_BindGPUVertexBuffers(*currentPass, 0, &bb, 1);

		bb.buffer = indexBuffer;
		SDL_BindGPUIndexBuffer(*currentPass, &bb, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		tci.width = rendererState.primaryPage->bitmap.bm_w;
		tci.height = rendererState.primaryPage->bitmap.bm_h;
		SDL_GPUTexture* primaryTex = SDL_CreateGPUTexture(rendererState.device, &tci);

		tci.width = rendererState.secondaryPage->bitmap.bm_w;
		tci.height = rendererState.secondaryPage->bitmap.bm_h;
		SDL_GPUTexture* secondaryTex = SDL_CreateGPUTexture(rendererState.device, &tci);

		SDL_GPUTextureSamplerBinding tsb[] { {
			.texture = secondaryTex,
			.sampler = rendererState.defaultSampler
		}, {
			.texture = primaryTex,
			.sampler = rendererState.defaultSampler
		} };
		SDL_BindGPUFragmentSamplers(*currentPass, 0, tsb, 2);

		SDL_GPUBuffer* storageBuffers[] { rendererState.paletteBuffer };// , rendererState.paletteBuffer}; //TODO: need portal buffer
		SDL_BindGPUFragmentStorageBuffers(*currentPass, 0, storageBuffers, SDL_arraysize(storageBuffers));

		SDL_DrawGPUIndexedPrimitives(*currentPass, worldIndices.size(), 1, 0, 0, 0);

		SDL_ReleaseGPUTransferBuffer(rendererState.device, vertTB.buffer);
		SDL_ReleaseGPUTransferBuffer(rendererState.device, indTB.buffer);

		SDL_ReleaseGPUBuffer(rendererState.device, vertexBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, indexBuffer);

		SDL_ReleaseGPUTexture(rendererState.device, primaryTex);
		SDL_ReleaseGPUTexture(rendererState.device, secondaryTex);

		worldVertices.clear();
		worldIndices.clear();
	}

	void DispatchMineDrawCalls() {

		static int d1 = 30;

		ViewTarget view = VT_NONE;

		SDL_GPUCommandBuffer* mainCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);
		SDL_GPUCommandBuffer* leftCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);
		SDL_GPUCommandBuffer* rightCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);
		
		SDL_GPURenderPass* mainPass = SDL_BeginGPURenderPass(mainCommandBuffer, &rendererState.mainCTarget, 1, &rendererState.mainDTarget);
		SDL_GPURenderPass* leftPass = NULL;
		SDL_GPURenderPass* rightPass = NULL;

		if (rendererState.cloneMode != VCM_CLONE_RL)
			leftPass = SDL_BeginGPURenderPass(leftCommandBuffer, &rendererState.leftCTarget, 1, &rendererState.leftDTarget);
		if (rendererState.cloneMode != VCM_CLONE_LR)
			rightPass = SDL_BeginGPURenderPass(rightCommandBuffer, &rendererState.rightCTarget, 1, &rendererState.rightDTarget);

		SDL_GPUCommandBuffer** currentCommandBuffer;
		SDL_GPURenderPass** currentPass;
		SDL_GPUColorTargetInfo* cct;
		SDL_GPUDepthStencilTargetInfo* cdt;

		SDL_GPUBuffer* currentPoratlBuffer;

		TexturePage* primaryPage;
		TexturePage* secondaryPage;
		
		SDL_BindGPUGraphicsPipeline(mainPass, rendererState.worldPipeline);
		if (leftPass)
			SDL_BindGPUGraphicsPipeline(leftPass, rendererState.worldPipeline);
		if (rightPass)
			SDL_BindGPUGraphicsPipeline(rightPass, rendererState.worldPipeline);

		for (auto& cp : worldDrawCalls) {

			const auto& [newPrimary, newSecondary, newView] = cp.first;

			Assert(newView != VT_NONE);

			bool swap = false;

			if (newView != view) {
				swap = true;
			}

			if (newPrimary != rendererState.primaryPage) {
				rendererState.primaryPage = newPrimary;
				swap = true;
			} 

			if (newSecondary != rendererState.secondaryPage && newSecondary != NULL) {
				rendererState.secondaryPage = newSecondary;
				swap = true;
			}

			if (swap) {// = SDL_CreateGPUTexture(rendererState.device, &tci);

				if (worldVertices.size() > 0) {
					if (d1 == 0)
						mprintf((0, "Drew %ld verts with %ld inds | ", worldVertices.size(), worldIndices.size()));
					RenderBatch(currentPass);
				}

				UploadTexturePage(rendererState.primaryPage, false);
				if (newSecondary)
					UploadTexturePage(rendererState.secondaryPage, true);

				SDL_PushGPUFragmentUniformData(mainCommandBuffer, 0, &rendererState.numTexturesInPage, sizeof(rendererState.numTexturesInPage));
				SDL_PushGPUFragmentUniformData(leftCommandBuffer, 0, &rendererState.numTexturesInPage, sizeof(rendererState.numTexturesInPage));
				SDL_PushGPUFragmentUniformData(rightCommandBuffer, 0, &rendererState.numTexturesInPage, sizeof(rendererState.numTexturesInPage));

				if (view != newView) {

					view = newView;

					if (view == VT_MAIN) {
						currentCommandBuffer = &mainCommandBuffer;
						UploadVertexMatrix(*currentCommandBuffer, &rendererState.mainProjectionMatrix, MID_PROJ);
						UploadVertexMatrix(*currentCommandBuffer, &rendererState.mainViewMatrix, MID_VIEW);
						currentPass = &mainPass;
						currentPoratlBuffer = rendererState.mainPortalBuffer;
						cct = &rendererState.mainCTarget;
						cdt = &rendererState.mainDTarget;
					} else {

						if (view == VT_LEFT) {
							currentCommandBuffer = &leftCommandBuffer;
							UploadVertexMatrix(*currentCommandBuffer, &rendererState.subViewMatrixL, MID_VIEW);
							currentPass = &leftPass;
							cct = &rendererState.leftCTarget;
							cdt = &rendererState.leftDTarget;
						}
						else {
							currentCommandBuffer = &rightCommandBuffer;
							UploadVertexMatrix(*currentCommandBuffer, &rendererState.subViewMatrixR, MID_VIEW);
							currentPass = &rightPass;
							cct = &rendererState.rightCTarget;
							cdt = &rendererState.rightDTarget;
						}

						UploadVertexMatrix(*currentCommandBuffer, &rendererState.subProjectionMatrix, MID_PROJ);

					}

				}
			}

			for (auto& Draw : cp.second)
				Draw(*currentCommandBuffer);

		}

		if (worldVertices.size() > 0) {
			if (d1 == 0)
				mprintf((0, "Drew %ld verts with %ld inds | ", worldVertices.size(), worldIndices.size()));
			RenderBatch(currentPass);
		}

		SDL_EndGPURenderPass(mainPass);
		if (leftPass)
			SDL_EndGPURenderPass(leftPass);
		if (rightPass)
			SDL_EndGPURenderPass(rightPass);

		SDL_SubmitGPUCommandBuffer(mainCommandBuffer);
		SDL_SubmitGPUCommandBuffer(leftCommandBuffer);
		SDL_SubmitGPUCommandBuffer(rightCommandBuffer);

		if (d1 <= 0) {
			mprintf((0, "\n"));
			d1 = 30;
		}
		d1--;

	}

	void RenderSide(const ViewTarget target, const int segno, const int sideno) {
		
		const segment& segment = Segments[segno];
		const side& side = segment.sides[sideno];
		auto& tp1 = rendererState.tpageLocations[side.tmap_num];
		auto& tp2 = rendererState.tpageLocations[side.tmap_num2 & 0x3FFF];

		drawkey k {
			&rendererState.tpages[tp1.first],
			&rendererState.tpages[tp2.first],
			target
		};

		if (worldDrawCalls.count(k) == 0) {
			worldDrawCalls[k] = std::vector<drawcall>();
		}

		std::vector<drawcall>& calls = worldDrawCalls[k];

		//TODO lock the vector
		calls.emplace_back([tp1, tp2, segno, sideno](SDL_GPUCommandBuffer* combuf) {
			
			//if (rendererState.drawCallObjID != -1) {
				//constexpr static mat4f identityModelAnim[2] { M4_IDENTITY_MATRIX_MACRO, M4_IDENTITY_MATRIX_MACRO };
				//SDL_PushGPUVertexUniformData(combuf, 0, identityModelAnim, sizeof(identityModelAnim));
			UploadVertexMatrix(combuf, &M4_IDENTITY_MATRIX, MID_ANIM);
			UploadVertexMatrix(combuf, &M4_IDENTITY_MATRIX, MID_MODEL);
			//}
			//rendererState.drawCallObjID = -1;

			const auto& segment = Segments[segno];
			const auto& side = segment.sides[sideno];
			const auto& sideverts = Side_to_verts[sideno];

			int vertStart = worldVertices.size();

			for (int i = 0; i < MAX_VERTICES_PER_POLY; i++) {

				auto& vert = Vertices[segment.verts[sideverts[i]]];
				
				worldVertices.emplace_back( WorldVertex {
					.pos = {
						f2fl(vert.x), 
						f2fl(vert.y), 
						f2fl(vert.z),
					},
					.uvl = {
						f2fl(side.uvls[i].u),
						f2fl(side.uvls[i].v),
						f2fl(side.uvls[i].l),
					},
					.props = {
						segno,
						tp1.second,
						tp2.second,
						((side.tmap_num2 & 0xC000) >> 14) & 3
					},
				});

			}

			if (side.type == SIDE_IS_TRI_13) {
				worldIndices.push_back(vertStart + 0);
				worldIndices.push_back(vertStart + 1);
				worldIndices.push_back(vertStart + 3);
				worldIndices.push_back(vertStart + 1);
				worldIndices.push_back(vertStart + 3);
				worldIndices.push_back(vertStart + 2);
			} else {
				worldIndices.push_back(vertStart + 0);
				worldIndices.push_back(vertStart + 1);
				worldIndices.push_back(vertStart + 2);
				worldIndices.push_back(vertStart + 0);
				worldIndices.push_back(vertStart + 3);
				worldIndices.push_back(vertStart + 2);
			}
			
		});

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
				.h = rendererState.renderHeight
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
			
			DispatchMineDrawCalls();

			SDL_BlitGPUTexture(rendererState.mainCommandBuffer, &bi);

		}

		SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(rendererState.mainCommandBuffer);
		if (fence == NULL) {
			mprintf((1, "Error submitting stage 1 command buffer and acquiring fence: %s", SDL_GetError()));
		}
		
		rendererState.mainCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);

		uint32_t windowWidth, windowHeight;
		
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(rendererState.mainCommandBuffer, gameWindow, &rendererState.windowTexture, &windowWidth, &windowHeight))
			Error("Error acquiring swapchain texture: %s", SDL_GetError());

		bi.source.texture = rendererState.windowCTarget.texture;
		bi.destination.texture = rendererState.windowTexture;
		bi.destination.w = windowWidth;
		bi.destination.h = windowHeight;

		SDL_WaitForGPUFences(rendererState.device, false, &fence, 1);
		SDL_ReleaseGPUFence(rendererState.device, fence);

		SDL_BlitGPUTexture(rendererState.mainCommandBuffer, &bi);

		SDL_SubmitGPUCommandBuffer(rendererState.mainCommandBuffer);
		rendererState.mainCommandBuffer = NULL;

	}

	void ShutdownRenderAPI() {

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