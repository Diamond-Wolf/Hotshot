/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include <array>
#include <future>
#include <unordered_map>

#include <SDL_gpu.h>

#include "2d/gr.h"
#include "cfile/cfile.h"
#include "main/inferno.h"
#include "misc/error.h"
#include "misc/types.h"
#include "platform/mono.h"
#include "platform/renderapi.h"

#ifdef NDEBUG
# define ENABLE_SDL_DEBUG false
#else
# define ENABLE_SDL_DEBUG true
#endif

extern SDL_Window* gameWindow;

namespace HRender {

	enum PortalDirection {
		PD_FORWARD,
		PD_REAR
	};

	static struct SDLRenderStruct {

		SDL_GPUDevice* device = NULL;

		SDL_GPUShader* screenFrag = NULL;
		SDL_GPUShader* screenVert = NULL;

		SDL_GPUCommandBuffer* mainCommandBuffer = NULL;

		uint32_t renderWidth, renderHeight;

		SDL_GPUTexture* windowTexture;

		SDL_GPUBuffer* portalBufferFront = NULL;
		SDL_GPUBuffer* portalBufferRear = NULL;
		std::future<SDL_GPUFence*> frontPortalListBuilt;
		std::future<SDL_GPUFence*> rearPortalListBuilt;

		SDL_GPUBuffer* paletteBuffer = NULL;

		SDL_GPUBuffer* screenVertBuffer = NULL;
		SDL_GPUBuffer* screenIndBuffer = NULL;
		SDL_GPUSampler* screenSampler = NULL;

		SDL_GPUGraphicsPipeline* screenPipeline = NULL;;

		SDL_GPUColorTargetInfo ctarget {
			.texture = NULL,
			.mip_level = 0,
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.store_op = SDL_GPU_STOREOP_DONT_CARE,
			.cycle = true
		};

		SDL_GPUDepthStencilTargetInfo dtarget {
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

	struct TexturePage {
		SDL_GPUTexture* texture;
	};

	struct TransferBuffer {
		SDL_GPUTransferBuffer* buffer;
		void* memoryMap;
	};

	typedef void(*drawcall)();

	std::unordered_map<TexturePage*, std::vector<drawcall>> texturedDrawCalls;

#pragma region SDLHelpers

	template <int bufN, int attrN> SDL_GPUGraphicsPipeline* CreateGraphicsPipeline(
		SDL_GPUShader* vertex, SDL_GPUShader* fragment, SDL_GPUPrimitiveType primitiveType,
		std::array<SDL_GPUVertexBufferDescription, bufN> buffers,
		std::array<SDL_GPUVertexAttribute, attrN> attributes
	) {

		SDL_GPUTextureFormat fmt = SDL_GetGPUSwapchainTextureFormat(rendererState.device, gameWindow);
		if (fmt == SDL_GPU_TEXTUREFORMAT_INVALID)
			fmt = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT; //Guess, I guess

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
				.color_target_descriptions = std::array { SDL_GPUColorTargetDescription {
					.format = fmt,
				}}.data(),
				.num_color_targets = 1
			},
		};

		return SDL_CreateGPUGraphicsPipeline(rendererState.device, &gpci);

	}

	void InitCommandBuffer() {
		if (rendererState.mainCommandBuffer == NULL)
			rendererState.mainCommandBuffer = SDL_AcquireGPUCommandBuffer(rendererState.device);
	}

	SDL_GPURenderPass* BeginDefaultRenderPass() {
		return SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.ctarget, 1, &rendererState.dtarget);
	}

	TransferBuffer CreateTransferBuffer(const SDL_GPUTransferBufferCreateInfo* tbci, const bool cycle) {

		TransferBuffer buf;

		buf.buffer = SDL_CreateGPUTransferBuffer(rendererState.device, tbci);
		buf.memoryMap = SDL_MapGPUTransferBuffer(rendererState.device, buf.buffer, cycle);

		return buf;

	}

	void FreeTransferBuffer(TransferBuffer buffer) {
		SDL_ReleaseGPUTransferBuffer(rendererState.device, buffer.buffer);
	}

#pragma endregion SDLHelpers

	void BuildShader(const char* name, const SDL_GPUShaderStage stage, SDL_GPUShader** shader, const uint32_t samplers = 0, const uint32_t uniforms = 0, const uint32_t buffers = 0) {

		mprintf((0, "Building shader %s\n", name));

		CFILE* shaderCode = cfopen(name, "rb");
		if (shaderCode == NULL) {
			Error("Could not open shader <%s>!", name);
		}
		uint8_t* codeBytes = new uint8_t[shaderCode->size];
		cfread(codeBytes, shaderCode->size, 1, shaderCode);
		
		SDL_GPUShaderCreateInfo shaderInfo {
			.code_size = (size_t)shaderCode->size,
			.code = codeBytes,
			.entrypoint = "main",
			.format = SDL_GPU_SHADERFORMAT_SPIRV,
			.stage = stage,
			.num_samplers = samplers,
			.num_storage_textures = 0,
			.num_storage_buffers = buffers,
			.num_uniform_buffers = uniforms,
			.props = 0
		};

		cfclose(shaderCode);

		*shader = SDL_CreateGPUShader(rendererState.device, &shaderInfo);

		delete[] codeBytes;
	}

	void UploadPalette(uint8_t* palette) {

		const int PALETTE_SIZE = sizeof(gr_palette);

		InitCommandBuffer();

		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = PALETTE_SIZE * sizeof(uint32_t)
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, false);

		static uint32_t expandedPalette[PALETTE_SIZE];
		for (int i = 0; i < PALETTE_SIZE; i++) {
			expandedPalette[i] = palette[i];
		}

		SDL_memcpy(tbuf.memoryMap, expandedPalette, PALETTE_SIZE * sizeof(uint32_t));
		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);

		SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(rendererState.mainCommandBuffer);

		SDL_GPUTransferBufferLocation tbloc {
			.transfer_buffer = tbuf.buffer,
			.offset = 0
		};

		SDL_GPUBufferRegion breg {
			.buffer = rendererState.paletteBuffer,
			.offset = 0,
			.size = PALETTE_SIZE * sizeof(uint32_t)
		};

		SDL_UploadToGPUBuffer(pass, &tbloc, &breg, false);

		SDL_EndGPUCopyPass(pass);

	}

	void UploadPalette() {
		UploadPalette(gr_palette);
	}

	void ResizeWindow(const int w, const int h) {
		InitCommandBuffer();
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(rendererState.mainCommandBuffer, gameWindow, &rendererState.windowTexture, NULL, NULL))
			Error("Error acquiring swapchain texture: %s", SDL_GetError());
	}

	void ResizeRenderTarget(const unsigned int w, const unsigned int h) {

		rendererState.renderWidth = w;
		rendererState.renderHeight = h;

		// TODO - Cache rear view size

		SDL_GPUTextureCreateInfo texCreateInfo{
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
			.width = w,
			.height = h,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		rendererState.ctarget.texture = SDL_CreateGPUTexture(rendererState.device, &texCreateInfo);

		texCreateInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
		texCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

		rendererState.dtarget.texture = SDL_CreateGPUTexture(rendererState.device, &texCreateInfo);

		rendererState.screenPipeline = CreateGraphicsPipeline(rendererState.screenVert, rendererState.screenFrag, SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
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

	}

	void InitScreenRendering(SDL_GPUCopyPass* cpass) {
	
		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = sizeof(float) * 4 * 4
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, false);

		std::array<float, 4 * 4> verts {
			-1.f, -1.f, 0.5f, 0.f,
			1.f, -1.f, 0.5f, 0.f,
			-1.f, 1.f, 0.5f, 0.f,
			1.f, 1.f, 0.5f, 0.f,
		};
		SDL_memcpy(tbuf.memoryMap, verts.data(), verts.size() * sizeof(float));
		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);

		SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(float) * 4 * 4
		};

		rendererState.screenVertBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);

		SDL_GPUTransferBufferLocation tbl {
			.transfer_buffer = tbuf.buffer,
			.offset = 0
		};

		SDL_GPUBufferRegion br {
			.buffer = rendererState.screenVertBuffer,
			.offset = 0,
			.size = sizeof(float) * 4 * 4
		};

		SDL_UploadToGPUBuffer(cpass, &tbl, &br, false);

		SDL_ReleaseGPUTransferBuffer(rendererState.device, tbuf.buffer);

		tbci.size = sizeof(uint16_t) * 4;
		tbuf = CreateTransferBuffer(&tbci, false);

		std::array<uint16_t, 4> inds {
			0, 1, 2, 3
		};
		memcpy(tbuf.memoryMap, inds.data(), inds.size() * sizeof(uint16_t));
		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);

		bci = {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = sizeof(uint16_t) * 4
		};
		rendererState.screenIndBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);

		tbl.transfer_buffer = tbuf.buffer;
		br.buffer = rendererState.screenIndBuffer;
		br.size = sizeof(uint16_t) * 4;

		SDL_UploadToGPUBuffer(cpass, &tbl, &br, false);

		rendererState.screenSampler = SDL_CreateGPUSampler(rendererState.device, &rendererState.defaultSamplerInfo);

	}

	int InitRenderAPI() {

		mprintf((0, "\nInitializing HRender\n"));

		texturedDrawCalls.reserve(16);

		rendererState.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, ENABLE_SDL_DEBUG, NULL);
		if (rendererState.device == NULL) {
			mprintf((1, "Could not create GPU device: %s", SDL_GetError()));
			return 2;
		}

		if (!SDL_ClaimWindowForGPUDevice(rendererState.device, gameWindow)) {
			mprintf((1, "Could not link GPU to window: %s", SDL_GetError()));
			return 3;
		}

		BuildShader("screenf.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.screenFrag, 1, 0, 1);
		BuildShader("screenv.spv", SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.screenVert);

		bool error = false;
		
		if (rendererState.screenFrag == NULL) {
			mprintf((1, "Could not create screen fragment shader from screenf.spv\n"));
			error = true;
		}

		if (rendererState.screenVert == NULL) {
			mprintf((1, "Could not create screen vertex shader from screenv.spv\n"));
			error = true;
		}

		if (error) {
			mprintf((1, "Last error: %s\n", SDL_GetError()));
			return 4;
		}

		SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);

		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(cbuf);

		SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
			.size = sizeof(gr_palette) * sizeof(uint32_t)
		};
		
		rendererState.paletteBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);

		InitScreenRendering(cpass);

		SDL_EndGPUCopyPass(cpass);
		SDL_SubmitGPUCommandBuffer(cbuf);

		return 0;

	}

	SDL_GPUFence* BuildGPUPortalListThread(PortalDirection dir);
	void BuildGPUPortalList() {
		rendererState.frontPortalListBuilt = std::async(std::launch::async, BuildGPUPortalListThread, PD_FORWARD);
		//if (rear enabled)
			rendererState.rearPortalListBuilt = std::async(std::launch::async, BuildGPUPortalListThread, PD_REAR);
	}

	void RenderScreenBitmap(grs_bitmap* bm) {

		int bmSize = bm->bm_w * bm->bm_h;

		SDL_GPUTextureCreateInfo tci {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8_UINT,
			.usage = SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ,
			.width = (uint32_t)bm->bm_w,
			.height = (uint32_t)bm->bm_h,
			.layer_count_or_depth = 1,
			.num_levels = 1,
		};

		SDL_GPUTexture* bmTex = SDL_CreateGPUTexture(rendererState.device, &tci);

		/*SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
			.size = bmSize
		};

		SDL_GPUBuffer* texbuf = SDL_CreateGPUBuffer(rendererState.device, &bci);*/

		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (uint32_t)bmSize
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, true);

		SDL_memcpy(tbuf.memoryMap, bm->bm_data, bmSize);
		SDL_UnmapGPUTransferBuffer(rendererState.device, tbuf.buffer);

		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(rendererState.mainCommandBuffer);

		SDL_GPUTextureTransferInfo tti {
			.transfer_buffer = tbuf.buffer,
			.offset = 0,
			.pixels_per_row = (uint32_t)bm->bm_w
		};

		SDL_GPUTextureRegion tr {
			.texture = bmTex,
			.w = (uint32_t)bm->bm_w,
			.h = (uint32_t)bm->bm_h
		};

		SDL_UploadToGPUTexture(cpass, &tti, &tr, true);

		SDL_ReleaseGPUTransferBuffer(rendererState.device, tbuf.buffer);
		SDL_EndGPUCopyPass(cpass);



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
			.sampler = rendererState.screenSampler
		};
		SDL_BindGPUFragmentSamplers(rpass, 0, &tsb, 1);

		SDL_BindGPUFragmentStorageBuffers(rpass, 0, &rendererState.paletteBuffer, 1);

		SDL_DrawGPUIndexedPrimitives(rpass, 4, 1, 0, 0, 0); 

		SDL_EndGPURenderPass(rpass);

	}

	void RenderScreenCanvas(grs_canvas* canvas) {
		RenderScreenBitmap(&canvas->cv_bitmap);
	}

	void BeginRenderFrame() { // TODO: If in game, build and submit portal list. Also, determine if rear view mirrors need textures.
		texturedDrawCalls.clear();
		InitCommandBuffer();
	}

	void EndRenderFrame() {

		//if (rendererState.currentRenderPass != NULL)
		//	SDL_EndGPURenderPass(rendererState.currentRenderPass);

		for (auto& cp : texturedDrawCalls) {
			TexturePage* page = cp.first;
			
			//Swap texture page, begin render pass

			for (auto& Draw : cp.second)
				Draw();

			//end render pass

		}

		SDL_SubmitGPUCommandBuffer(rendererState.mainCommandBuffer);
		rendererState.mainCommandBuffer = NULL;

	}

	void ShutdownRenderAPI() {

		SDL_ReleaseGPUGraphicsPipeline(rendererState.device, rendererState.screenPipeline);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.dtarget.texture);
		SDL_ReleaseGPUTexture(rendererState.device, rendererState.ctarget.texture);

		SDL_ReleaseGPUShader(rendererState.device, rendererState.screenVert);
		SDL_ReleaseGPUShader(rendererState.device, rendererState.screenFrag);

		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.paletteBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.portalBufferFront);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.portalBufferRear);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.screenIndBuffer);
		SDL_ReleaseGPUBuffer(rendererState.device, rendererState.screenVertBuffer);

		SDL_ReleaseWindowFromGPUDevice(rendererState.device, gameWindow);
		SDL_DestroyGPUDevice(rendererState.device);

	}

	SDL_GPUFence* BuildGPUPortalListThread(PortalDirection dir) {

		SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);

		mprintf((1, "Portal list building not ready!"));

		//SDL_GPUStorageBufferReadWriteBinding* verts = 

		//SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cbuf, NULL, 0, , 1);

		return SDL_SubmitGPUCommandBufferAndAcquireFence(cbuf);

	}

}