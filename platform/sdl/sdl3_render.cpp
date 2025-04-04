/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include <array>
#include <future>
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

	typedef float (mat4f[4])[4];

	constexpr mat4f M4_IDENTITY_MATRIX {
		{1,0,0,0},
		{0,1,0,0},
		{0,0,1,0},
		{0,0,0,1},
	};

	static struct SDLRenderStruct {

		SDL_GPUDevice* device = NULL;

		SDL_GPUShader* screenFrag = NULL;
		SDL_GPUShader* screenVert = NULL;

		SDL_GPUShader* worldFrag = NULL;
		SDL_GPUShader* worldVert = NULL;

		SDL_GPUCommandBuffer* mainCommandBuffer = NULL;

		uint32_t renderWidth, renderHeight;

		SDL_GPUTexture* windowTexture = NULL;

		SDL_GPUBuffer* portalBufferFront = NULL;
		SDL_GPUBuffer* portalBufferRear = NULL;

		std::future<SDL_GPUFence*> mainPortalListBuilt;
		std::future<SDL_GPUFence*> leftWindowPortalListBuilt;
		std::future<SDL_GPUFence*> rightWindowPortalListBuilt;

		TransferBuffer mainPortalBuffer;
		TransferBuffer leftPortalBuffer;
		TransferBuffer rightPortalBuffer;

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

		std::vector<TexturePage> tpages;
		std::unordered_map<int, std::pair<int, int>> tpageLocations; //bm index : (tpage, index in page)

		SDL_GPUTextureFormat windowFormat;

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
			.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
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
			.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
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
			.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
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
			.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
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

	typedef void(*drawcall)();
	typedef std::tuple<TexturePage*, TexturePage*, ViewTarget> drawkey;

	struct dkeyCompare {

		bool operator()(drawkey a, drawkey b) {
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
	
	union num32 { float f; int32_t i; };

	std::map<drawkey, std::vector<drawcall>, dkeyCompare> worldDrawCalls;
	std::vector<num32> worldVertices;
	std::vector<unsigned int> worldIndices;

	bool mineRenderingReady = false;

#pragma region SDLHelpers

	template <int bufN, int attrN> SDL_GPUGraphicsPipeline* CreateGraphicsPipeline(
		SDL_GPUShader* vertex, SDL_GPUShader* fragment, SDL_GPUPrimitiveType primitiveType,
		std::array<SDL_GPUVertexBufferDescription, bufN> buffers,
		std::array<SDL_GPUVertexAttribute, attrN> attributes
	) {

		rendererState.windowFormat = SDL_GetGPUSwapchainTextureFormat(rendererState.device, gameWindow);
		if (rendererState.windowFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
			rendererState.windowFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; //Guess, I guess

		SDL_GPUColorTargetDescription ctd[] {{ 
			.format = rendererState.windowFormat
		}};

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
				.color_target_descriptions = ctd,
				.num_color_targets = 1
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

	bool BuildShader(const char* name, const SDL_GPUShaderStage stage, SDL_GPUShader** shader, const uint32_t samplers = 0, const uint32_t uniforms = 0, const uint32_t buffers = 0) {

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
			.format = rendererState.windowFormat,
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

		UpdateProjectionMatrices((float)rendererState.renderWidth / rendererState.renderHeight);

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
				.pitch = sizeof(float) * 4,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
			} },
			
			std::array { SDL_GPUVertexAttribute {
				.location = 0,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = 0,
			}, SDL_GPUVertexAttribute {
				.location = 1,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.offset = 0,
			}, SDL_GPUVertexAttribute {
				.location = 2,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_INT3,
				.offset = 0,
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

		good &= BuildShader(SHADER("screenf"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.screenFrag, 1, 0, 1);
		good &= BuildShader(SHADER("screenv"), SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.screenVert);

		good &= BuildShader(SHADER("worldf"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.worldFrag, 1, 0, 1);
		good &= BuildShader(SHADER("worldv"), SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.worldVert, 0, 4, 0);

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

		rendererState.windowFormat = SDL_GetGPUSwapchainTextureFormat(rendererState.device, gameWindow);
		if (rendererState.windowFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
			rendererState.windowFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

		memset(rendererState.mainProjectionMatrix, 0, sizeof(rendererState.mainProjectionMatrix));

		constexpr float depth = f2fl(FAR_CLIP - NEAR_CLIP);
		constexpr float invDepth = 1 / depth;

		rendererState.mainProjectionMatrix[1][1] = 1.f / tanf(VFOV_RAD_F / 2);
		rendererState.mainProjectionMatrix[2][2] = f2fl(FAR_CLIP) * invDepth;
		rendererState.mainProjectionMatrix[3][2] = -rendererState.mainProjectionMatrix[2][2] * f2fl(NEAR_CLIP);
		rendererState.mainProjectionMatrix[2][3] = 1.f;
		
		memcpy(&rendererState.subProjectionMatrix, &rendererState.mainProjectionMatrix, sizeof(rendererState.subProjectionMatrix));

		rendererState.subProjectionMatrix[0][0] = rendererState.subProjectionMatrix[1][1];

		return 0;

	}

	SDL_GPUFence* BuildGPUPortalListThread(const std::vector<short> segments, const vms_vector pos, const vms_vector dir, const ViewTarget target, TransferBuffer* destBuffer);
	void BuildGPUPortalList(const std::vector<short>& segments, const vms_vector& pos, const vms_vector& dir, const ViewTarget target, const ViewTarget clone) {

		std::future<SDL_GPUFence*>* future;
		TransferBuffer* buffer;

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

			*future = std::async(std::launch::async, []() {
				SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
				return SDL_SubmitGPUCommandBufferAndAcquireFence(cbuf);
			});

		} else {

			*future = std::async(std::launch::async, BuildGPUPortalListThread, segments, pos, dir, target, buffer);

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
			auto& bm = activePiggyTable->gameBitmaps[i];
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
		switch (Cockpit_mode) {

			default:
			case CM_FULL_SCREEN:
			case CM_REAR_VIEW:
				UpdateMainView(rendererState.renderWidth / rendererState.renderHeight);
			break;
			
			case CM_STATUS_BAR:
			case CM_LETTERBOX:
				UpdateMainView(2);
			break;

			case CM_FULL_COCKPIT: //todo scale by actual screen resolution
				UpdateMainView(2);
			break;
		}
	}
	
	void UpdateViewMatrix(const object* source, const ViewTarget target) {

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

		//*matrix = M4_IDENTITY_MATRIX;
		memcpy(matrix, M4_IDENTITY_MATRIX, sizeof(*matrix));
		
		for (int m = 0; m < 3; m++) {
			for (int n = 0; n < 3; n++) {
				*matrix[m][n] = f2fl(source->orient[m][n]);
			}
			*matrix[m][3] = -f2fl(source->pos[m]);
		}

	}

	void UploadVertexMatrix(const mat4f* matrix, const MatrixID id) {
		SDL_PushGPUVertexUniformData(rendererState.mainCommandBuffer, id, matrix, sizeof(*matrix));
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
		
	}

	void DispatchMineDrawCalls() {

		ViewTarget view = VT_NONE;

		SDL_GPURenderPass* mainPass = SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.mainCTarget, 1, &rendererState.mainDTarget);
		SDL_GPURenderPass* leftPass = SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.leftCTarget, 1, &rendererState.leftDTarget);
		SDL_GPURenderPass* rightPass = SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.rightCTarget, 1, &rendererState.rightDTarget);

		for (auto& cp : worldDrawCalls) {

			const auto& [newPrimary, newSecondary, newView] = cp.first;

			SDL_GPUColorTargetDescription* cdt;
			SDL_GPUColorTargetDescription* ddt;
 
			Assert(newView != VT_NONE);

			bool swap = false;

			if (newPrimary != rendererState.primaryPage) {
				rendererState.primaryPage = newPrimary;
				swap = true;
			} 

			if (newSecondary != rendererState.secondaryPage && newSecondary != NULL) {
				rendererState.secondaryPage = newSecondary;
				swap = true;
			}

			if (swap) {
				UploadTexturePage(rendererState.primaryPage, false);
				if (newSecondary)
					UploadTexturePage(rendererState.secondaryPage, true);
			}

			if (newView != view) {
				view = newView;
				swap = true;
				
				if (view == VT_MAIN) {
					UploadVertexMatrix(&rendererState.mainProjectionMatrix, MID_PROJ);
					UploadVertexMatrix(&rendererState.mainViewMatrix, MID_VIEW);
					rpass = SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, a, 1, d);
				} else {
					UploadVertexMatrix(&rendererState.subProjectionMatrix, MID_PROJ);

					if (view == VT_LEFT)
						UploadVertexMatrix(&rendererState.subViewMatrixL, MID_VIEW);
					else
						UploadVertexMatrix(&rendererState.subViewMatrixR, MID_VIEW);
				}
			}

			

			for (auto& Draw : cp.second)
				Draw();

			//end render pass

		}

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
		calls.emplace_back([tp1, tp2, segno, sideno]() {
			
			const auto& segment = Segments[segno];
			const auto& side = segment.sides[sideno];
			const auto& sideverts = Side_to_verts[sideno];

			int vertStart = worldVertices.size() / 10;

			for (int i = 0; i < MAX_VERTICES_PER_POLY; i++) {

				auto& vert = Vertices[segment.verts[sideverts[i]]];
				worldVertices.push_back({ .f = f2fl(vert.x) });
				worldVertices.push_back({ .f = f2fl(vert.y) });
				worldVertices.push_back({ .f = f2fl(vert.z) });
				worldVertices.push_back({ .f = f2fl(side.uvls[i].u) });
				worldVertices.push_back({ .f = f2fl(side.uvls[i].v) });
				worldVertices.push_back({ .f = f2fl(side.uvls[i].l) });
				worldVertices.push_back({ .i = segno });
				worldVertices.push_back({ .i = tp1.second });
				worldVertices.push_back({ .i = tp2.second });
				worldVertices.push_back({ .i = ((side.tmap_num2 & 0xC000) >> 14) & 3 });

			}

		});

	}

	void EndRenderFrame() {

		if (rendererState.mainCommandBuffer == NULL) {
			mprintf((1, "Tried to complete rendering that never started!"));
			return;
		}

		//mprintf((0, "render\n"));

		if (ExtGameStatus == GAMESTAT_RUNNING && mineRenderingReady) {

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

			mineRenderingReady = false;

		}

		SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(rendererState.mainCommandBuffer);
		if (fence == NULL) {
			mprintf((1, "Error submitting stage 1 command buffer and acquiring fence: %s", SDL_GetError()));
		}
		
		rendererState.mainCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);

		uint32_t windowWidth, windowHeight;
		
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(rendererState.mainCommandBuffer, gameWindow, &rendererState.windowTexture, &windowWidth, &windowHeight))
			Error("Error acquiring swapchain texture: %s", SDL_GetError());

		SDL_GPUBlitInfo bi {
			.source = {
				.texture = rendererState.windowCTarget.texture,
				//.layer_or_depth_plane = 1,
				.x = 0,
				.y = 0,
				.w = rendererState.renderWidth,
				.h = rendererState.renderHeight
			},
			.destination = {
				.texture = rendererState.windowTexture,
				//.layer_or_depth_plane = 1,
				.x = 0,
				.y = 0,
				.w = windowWidth,
				.h = windowHeight,
			},
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.cycle = true
		};

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

		SDL_ReleaseGPUShader(rendererState.device, rendererState.screenVert);
		SDL_ReleaseGPUShader(rendererState.device, rendererState.screenFrag);
		SDL_ReleaseGPUShader(rendererState.device, rendererState.worldVert);
		SDL_ReleaseGPUShader(rendererState.device, rendererState.worldFrag);

		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.paletteBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.portalBufferFront);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.portalBufferRear);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.screenIndBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.screenVertBuffer);

		SDL_ReleaseWindowFromGPUDevice(rendererState.device, gameWindow);
		SDL_DestroyGPUDevice(rendererState.device);

	}

	SDL_GPUFence* BuildGPUPortalListThread(const std::vector<short> segments, const vms_vector pos, const vms_vector dir, const ViewTarget target, TransferBuffer* destBuffer) {

		bool rearActive = (Cockpit_3d_view[0] == CV_REAR || Cockpit_3d_view[1] == CV_REAR);

		SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
		
		//mprintf((1, "Portal list building not ready!"));

		//SDL_GPUStorageBufferReadWriteBinding* verts = 

		//SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cbuf, NULL, 0, , 1);

		return SDL_SubmitGPUCommandBufferAndAcquireFence(cbuf);

	}

}