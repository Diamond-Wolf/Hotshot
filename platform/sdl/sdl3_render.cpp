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

#include "sdl3_render.h"
#include "platform/renderapi.h"

#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <tuple>
#include <bit>
#include <mutex>
#include <cmath>
#include <bit>

#include <SDL_gpu.h>

#include "2d/gr.h"
#include "2d/grdef.h"
#include "2d/rle.h"

#include "3d/globvars.h"

#include "cfile/cfile.h"

#include "main/ai.h"
#include "main/bm.h"
#include "main/game.h"
#include "main/gamestat.h"
#include "main/gauges.h"
#include "main/inferno.h"
#include "main/jobs.h"
#include "main/kconfig.h"
#include "main/laser.h"
#include "main/newcheat.h"
#include "main/player.h"
#include "main/polyobj.h"

#include "misc/error.h"
#include "misc/rand.h"
#include "misc/types.h"

#include "platform/mono.h"

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

	bool mineRenderingReady = false;

	const std::launch ASYNC_POLICY = std::launch::deferred;
	const SDL_GPUTextureFormat TEXTURE_FORMAT = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;

	void UploadTexturePage(TexturePage* page, SDL_GPUCopyPass* cpass, const bool secondary = false);

	bool dkeyCompare::operator()(const SideDrawKey a, const SideDrawKey b) const {
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

	constexpr size_t VERTEX_TRANSFER_FACTOR = 8 * (sizeof(WorldVertex) + sizeof(uint32_t)) * 24;

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
			.blend_state = {
				.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
				.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.color_blend_op = SDL_GPU_BLENDOP_ADD,
				.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
				.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
				.enable_blend = true,
			}
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
		//mprintf((0, "%p %p %p\n", rendererState.mainCommandBuffer, rendererState.windowCTarget.texture, rendererState.windowDTarget.texture));
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

	void SetMainPalette(uint8_t* palette) {

		rendererState.activePalette = palette;

		/*const int NUM_COLORS = SDL_arraysize(gr_palette) / 3;

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

		for (int i = 0; i < NUM_COLORS; i++) { //std430 packs nicely, but SDL thought it would be a good idea to upload std140 data anyway
			expandedPalette[i * 4] = palette[i * 3] / PALETTE_DIV;
			expandedPalette[i * 4 + 1] = palette[i * 3 + 1] / PALETTE_DIV;
			expandedPalette[i * 4 + 2] = palette[i * 3 + 2] / PALETTE_DIV;
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
		SDL_SubmitGPUCommandBuffer(upbuf);*/

	}

	void UploadPalette() {
		SetMainPalette(gr_palette);
	}

	// Small hack. SDL doesn't let you clear a screen normally, so fake it.
	void ClearScreen() {

		auto cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);

		SDL_GPURenderPass* rpass = SDL_BeginGPURenderPass(cbuf, &rendererState.windowCTarget, 1, &rendererState.windowDTarget);

		SDL_BindGPUGraphicsPipeline(rpass, rendererState.clearPipeline);

		SDL_GPUBufferBinding bb {
			.buffer = rendererState.screenVertBuffer,
			.offset = 0
		};
		SDL_BindGPUVertexBuffers(rpass, 0, &bb, 1);

		bb.buffer = rendererState.screenIndBuffer;
		SDL_BindGPUIndexBuffer(rpass, &bb, SDL_GPU_INDEXELEMENTSIZE_16BIT);

		SDL_DrawGPUIndexedPrimitives(rpass, 4, 1, 0, 0, 0); 

		SDL_EndGPURenderPass(rpass);

		SDL_SubmitGPUCommandBuffer(cbuf);

	}

#ifdef __APPLE__
	#define MaybeFlushScreenGarbage() ClearScreen()
#else
	#define MaybeFlushScreenGarbage()
#endif

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

		mprintf((0, "Resize render target: %d %d", w, h));

		rendererState.renderWidth = w;
		rendererState.renderHeight = h;

		if (rendererState.windowCTarget.texture != NULL) {
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.windowCTarget.texture);
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.windowDTarget.texture);
			SDL_ReleaseGPUTexture(rendererState.device, rendererState.fadeScreenTexture);
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

		rendererState.fadeScreenTexture = SDL_CreateGPUTexture(rendererState.device, &texCreateInfo);
		if (rendererState.fadeScreenTexture == NULL) {
			Error("Error creating render texture: %s", SDL_GetError());
		}

		texCreateInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
		texCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

		rendererState.windowDTarget.texture = SDL_CreateGPUTexture(rendererState.device, &texCreateInfo);
		if (rendererState.windowDTarget.texture == NULL) {
			Error("Error creating render depth texture: %s", SDL_GetError());
		}

		MaybeFlushScreenGarbage();

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

		//All because SDL3 doesn't know how to render over unitialized data with user-specified alpha (even 1.0) on some platforms and doesn't want to provide any straightforward means to initialize said uninitialized data
		rendererState.clearPipeline = CreateGraphicsPipeline<1,1>(rendererState.clearVert, rendererState.clearFrag, SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
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

		if (rendererState.clearPipeline == NULL) {
			Error("Error creating clear pipeline: %s", SDL_GetError());
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

		rendererState.pastePipeline = CreateGraphicsPipeline<1, 2>(rendererState.pasteVert, rendererState.pasteFrag, SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
			std::array { SDL_GPUVertexBufferDescription {
				.slot = 0,
				.pitch = (uint32_t)sizeof(float) * 4,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
			} },
			
			std::array { SDL_GPUVertexAttribute {
				.location = 0,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.offset = 0,
			}, SDL_GPUVertexAttribute {
				.location = 1,
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.offset = 2 * sizeof(float),
			} },
		true, false);

		if (rendererState.pastePipeline == NULL) {
			Error("Error creating world pipeline: %s", SDL_GetError());
		}

		InitPolymodelInterpreter();
		
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
		good &= BuildShader(SHADER("screenf"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.screenFrag, 1, 0, 0);

		good &= BuildShader(SHADER("worldv"), SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.worldVert, 0, 4, 0);
		good &= BuildShader(SHADER("worldf"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.worldFrag, 2, 1, 1);

		good &= BuildShader(SHADER("pastev"), SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.pasteVert, 0, 0, 0);
		good &= BuildShader(SHADER("pastef"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.pasteFrag, 1, 0, 0);

		good &= BuildShader(SHADER("clearv"), SDL_GPU_SHADERSTAGE_VERTEX, &rendererState.clearVert, 0, 0, 0);
		good &= BuildShader(SHADER("clearf"), SDL_GPU_SHADERSTAGE_FRAGMENT, &rendererState.clearFrag, 0, 0, 0);

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

		mprintf((0, "HRender: Initializing render modes\n"));

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

		memset(gr_palette, 0, 768);
		SetMainPalette(gr_palette);
		
		return 0;

	}

	SDL_GPUFence* SkipAsync() {
		return SDL_SubmitGPUCommandBufferAndAcquireFence(SDL_AcquireGPUCommandBuffer(rendererState.device));
	}

	void SkipGPUPortalList(const ViewTarget target) {
		switch (target) {

			case VT_MAIN:
				rendererState.mainPortalListBuilt = StartJob(SkipAsync);
			break;

			case VT_LEFT:
				rendererState.leftWindowPortalListBuilt = StartJob(SkipAsync);
			break;

			case VT_RIGHT:
				rendererState.rightWindowPortalListBuilt = StartJob(SkipAsync);
			break;

			default: break;

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

			*future = StartJob(SkipAsync);

		} else {

			*future = StartJob(BuildGPUPortalListThread, segments, pos, dir, target, buffer);

		}
	}

	void ClearTexturePages() {

		for (auto& tpage : rendererState.tpages) {
			delete[] tpage.bitmap.bm_data;
			if (tpage.bitmap.bm_alpha)
				delete[] tpage.bitmap.bm_alpha;
			delete[] tpage.convertedData;
		}

		rendererState.tpages.clear();
		rendererState.tpageLocations.clear();

	}

	void GenerateTexturePages() {
	
		rendererState.primaryTexture = NULL;
		rendererState.primaryPage = NULL;
		rendererState.secondaryTexture = NULL;
		rendererState.secondaryPage = NULL;

		ClearTexturePages();
		rendererState.tpages.reserve(activePiggyTable->gameBitmaps.size());

		//piggy_bitmap_page_out_all();

		for (int i = 0; i < activePiggyTable->gameBitmaps.size(); i++) {
			
			PIGGY_PAGE_IN(BITMAP_INDEX(i));
			grs_bitmap* gbm = &activePiggyTable->gameBitmaps[i];
			int bmSize = gbm->bm_w * gbm->bm_h;

			TexturePage page;
			page.bitmap = *gbm;
			page.bitmap.bm_data = new uint8_t[bmSize];
			gr_bm_ubitblt(gbm->bm_w, gbm->bm_h, 0, 0, 0, 0, gbm, &page.bitmap);

			page.convertedData = new float[bmSize * 4];
			for (int i = 0; i < bmSize; i++) {
				
				auto pindex = page.bitmap.bm_data[i];
				
				page.convertedData[i * 4 + 0] = rendererState.activePalette[pindex * 3 + 0] / 63.f;
				page.convertedData[i * 4 + 1] = rendererState.activePalette[pindex * 3 + 1] / 63.f;
				page.convertedData[i * 4 + 2] = rendererState.activePalette[pindex * 3 + 2] / 63.f;

				if (pindex == 255)
					page.convertedData[i * 4 + 3] = 0;
				else if (pindex == 254)
					page.convertedData[i * 4 + 3] = 2;
				else
					page.convertedData[i * 4 + 3] = 1;

			}

			rendererState.tpages.push_back(page);
			rendererState.tpageLocations[i] = std::pair(i, 0);
			
		}

		rendererState.targetTextureTransferSize = 64 * 64 * 4 * activePiggyTable->gameBitmaps.size();
		
		SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
		SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(cbuf);

		UploadTexturePage(&rendererState.tpages[0], cpass, false);
		UploadTexturePage(&rendererState.tpages[0], cpass, true);

		SDL_EndGPUCopyPass(cpass);
		SDL_SubmitGPUCommandBuffer(cbuf);

	}

	void GeneratePiggyStructs() {
		GenerateTexturePages();
		GenerateModels();
	}

	void PasteTextureToWorldCanvas(SDL_GPUTexture* source, float x1, float y1, float x2, float y2) {

		//InitCommandBuffer();
		auto cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);
		
		auto cpass = SDL_BeginGPUCopyPass(rendererState.mainCommandBuffer);

		std::array<float, 4 * 4> verts {
			x1, y2, 0.f, 0.f,
			x2, y2, 1.f, 0.f,
			x1, y1, 0.f, 1.f,
			x2, y1, 1.f, 1.f,
		};

		SDL_GPUTransferBufferCreateInfo tbci {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = sizeof(verts)
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, false);
		if (tbuf.memoryMap == NULL)
			Error("Error creating paste vertex memory map!");

		memcpy(tbuf.memoryMap, verts.data(), sizeof(verts));
		
		SDL_GPUBufferCreateInfo bci {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(verts)
		};

		auto vertBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);

		SDL_GPUTransferBufferLocation tbl {
			.transfer_buffer = tbuf.buffer,
			.offset = 0
		};

		SDL_GPUBufferRegion br {
			.buffer = vertBuffer,
			.offset = 0,
			.size = sizeof(verts)
		};

		SDL_UploadToGPUBuffer(cpass, &tbl, &br, true);

		SDL_EndGPUCopyPass(cpass);

		FreeTransferBuffer(tbuf);

		SDL_GPURenderPass* rpass = BeginDefaultRenderPass();

		SDL_BindGPUGraphicsPipeline(rpass, rendererState.pastePipeline);

		SDL_GPUBufferBinding bb {
			.buffer = vertBuffer,
			.offset = 0
		};
		SDL_BindGPUVertexBuffers(rpass, 0, &bb, 1);

		bb.buffer = rendererState.screenIndBuffer;
		SDL_BindGPUIndexBuffer(rpass, &bb, SDL_GPU_INDEXELEMENTSIZE_16BIT);

		SDL_GPUTextureSamplerBinding tsb {
			.texture = source,
			.sampler = rendererState.defaultSampler
		};
		SDL_BindGPUFragmentSamplers(rpass, 0, &tsb, 1);

		SDL_DrawGPUIndexedPrimitives(rpass, 4, 1, 0, 0, 0); 

		SDL_EndGPURenderPass(rpass);

		SDL_SubmitGPUCommandBuffer(cbuf);

	}

	void RenderScreenBitmap(grs_bitmap* bm) {

		InitCommandBuffer();

		int bmSize = bm->bm_w * bm->bm_h;

		SDL_GPUTextureCreateInfo tci {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
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
			.size = (uint32_t)(bmSize * sizeof(float) * 4)
		};

		TransferBuffer tbuf = CreateTransferBuffer(&tbci, true);
		if (tbuf.memoryMap == NULL) {
			Error("Could not create bitmap transfer buffer!");
		}

		float* floatMemMap = reinterpret_cast<float*>(tbuf.memoryMap);

		for (int i = 0; i < bmSize; i++) {
			auto pindex = bm->bm_data[i];
			floatMemMap[i * 4] = rendererState.activePalette[pindex * 3] / 63.f;
			floatMemMap[i * 4 + 1] = rendererState.activePalette[pindex * 3 + 1] / 63.f;
			floatMemMap[i * 4 + 2] = rendererState.activePalette[pindex * 3 + 2] / 63.f;
			floatMemMap[i * 4 + 3] = bm->bm_alpha ? bm->bm_alpha[i] / 255.f : 1.f;
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

	void SaveScreen() {

		auto cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);

		SDL_GPUBlitInfo bi {
			.source = {
				.texture = rendererState.windowCTarget.texture,
				.x = 0,
				.y = 0,
				.w = rendererState.renderWidth,
				.h = rendererState.renderHeight,
			},
			.destination = {
				.texture = rendererState.fadeScreenTexture,
				.x = 0,
				.y = 0,
				.w = rendererState.renderWidth,
				.h = rendererState.renderHeight,
			},
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.cycle = true,
		};

		SDL_BlitGPUTexture(cbuf, &bi);

		SDL_SubmitGPUCommandBuffer(cbuf);

	}

	void RenderSavedScreenFaded(uint8_t fadeAmount) {
	
		auto cbuf = SDL_AcquireGPUCommandBuffer(rendererState.device);

		SDL_GPUBlitInfo bi {
			.source = {
				.texture = rendererState.fadeScreenTexture,
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
			.cycle = true,
		};

		SDL_BlitGPUTexture(cbuf, &bi);

		uint8_t zero = 0;

		grs_bitmap bm {
			.bm_w = 1,
			.bm_h = 1,
			.bm_data = &zero,
			.bm_alpha = &fadeAmount,
		};

		RenderScreenBitmap(&bm);

		SDL_SubmitGPUCommandBuffer(cbuf);

	}

	void ResetWorldColorTargets() {
		
		mprintf((0, "Resetting color targets"));

		rendererState.mainCTarget.load_op = SDL_GPU_LOADOP_CLEAR;
		rendererState.rightCTarget.load_op = SDL_GPU_LOADOP_CLEAR;
		rendererState.leftCTarget.load_op = SDL_GPU_LOADOP_CLEAR;
		
		auto clearcom = SDL_AcquireGPUCommandBuffer(rendererState.device);

		//render pass auto clears
		auto rpass = SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.mainCTarget, 1, &rendererState.mainDTarget);
		SDL_EndGPURenderPass(rpass);

		rpass = SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.rightCTarget, 1, &rendererState.rightDTarget);
		SDL_EndGPURenderPass(rpass);

		rpass = SDL_BeginGPURenderPass(rendererState.mainCommandBuffer, &rendererState.leftCTarget, 1, &rendererState.leftDTarget);
		SDL_EndGPURenderPass(rpass);

		SDL_SubmitGPUCommandBuffer(clearcom);
		
		rendererState.mainCTarget.load_op = WORLD_LOAD_OP;
		rendererState.rightCTarget.load_op = WORLD_LOAD_OP;
		rendererState.leftCTarget.load_op = WORLD_LOAD_OP;

	}

	void PrepareMineRenderFrame() { // TODO: If in game, build and submit portal list. Also, determine if rear view mirrors need textures.
		
		sideDrawCalls.clear();
		modelDrawCalls.clear();
		objDrawCalls.clear();

		InitCommandBuffer();

		rendererState.cloneMode = VCM_NO_CLONE;

		if (mineRenderingReady) 
			mprintf((1, "Double prepared mine render!"));
		
		memset(gr_video_alpha, 0, SOFTWARE_VIDEO_BUFFER_SIZE);

		rendererState.drawTransferOffset = rendererState.textureTransferOffset = 0;

		mineRenderingReady = true;

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
		//ResetWorldColorTargets();
		
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
			.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
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
			if (msize < bmSize * sizeof(float) * 4) {
				Int3(); //This shouldn't happen
				msize = bmSize * sizeof(float) * 4;
			}
			msize = std::bit_ceil(msize);

			SDL_GPUTransferBufferCreateInfo tbci {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = msize
			};

			rendererState.textureTransferBuffer = CreateTransferBuffer(&tbci, true);
			rendererState.textureTransferOffset = 0;

			mprintf((0, "Reallocated texture transfer buffer\n"));

		}

		size_t bmLen = bmSize * sizeof(*page->convertedData) * 4;

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

			uint32_t msize = Segments.size() * VERTEX_TRANSFER_FACTOR;
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

			mprintf((0, "Reallocated vertex transfer buffer\n"));

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

		for (auto& cp : modelDrawCalls) {

			const auto& [newPrimary, _, newView] = cp.first;

			Assert(newView != VT_NONE);

			bool swap = false;

			if (newView != view) {
				swap = true;
			}

			if (newPrimary && newPrimary != rendererState.primaryPage) {
				swap = true;
			} 

			if (swap) {// = SDL_CreateGPUTexture(rendererState.device, &tci);

				UploadTexturePage(newPrimary, cpass, false);

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
				Draw(combuf, rpass, cpass);
				
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

	static void EmplaceModelDrawCall(std::vector<ObjDrawCall>& drawCalls, const ViewTarget target, const ModelFaceBatch& batch, const int segno, const size_t objno, const vms_matrix& rotationMatrix, const vms_vector& animOffset, const float light, const short textureOverride, const float visibility) {

		mat4f model = M4_IDENTITY_MATRIX_MACRO;
		mat4f anim = M4_IDENTITY_MATRIX_MACRO;

		auto& object = Objects[objno];

		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) {
				anim[i][j]  = f2fl(rotationMatrix[i][j]);
				anim[3][i]  = f2fl(animOffset[i]);
				model[i][j] = f2fl(object.orient[i][j]);
				model[3][i] = f2fl(object.pos[i]);
			}

		drawCalls.emplace_back([batch, segno, objno, model, anim, light, textureOverride, visibility](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {

			UploadVertexMatrix(combuf, &anim, MID_ANIM);
			if (rendererState.drawCallObjID != objno) {
				UploadVertexMatrix(combuf, &model, MID_MODEL);
				rendererState.drawCallObjID = objno;
			}
			
			size_t nv = batch.verts.size();

			WorldVertex* wverts = new WorldVertex[nv];
			//uint32_t* winds = new uint32_t[fb.indices.size()];

			for (int i = 0; i < nv; i++) {

				wverts[i] = batch.verts[i];

				float lightR, lightG, lightB;
				if (cheatValues[CI_FULLBRIGHT])
					lightR = lightG = lightB = 1.f;
				else
					lightR = lightG = lightB = light;

				if (cheatValues[CI_RAVE]) {

					const uint64_t x = std::bit_cast<uint32_t, float>(wverts[i].pos[0]);
					const uint64_t y = std::bit_cast<uint32_t, float>(wverts[i].pos[1]);
					const uint64_t z = std::bit_cast<uint32_t, float>(wverts[i].pos[2]);
					SDL_srand(x * 0xFFFF + y * 0x00FF + z + GameTime);

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

				/*wverts[i] = WorldVertex{
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
				};*/

				wverts[i].props[0] = segno;

				wverts[i].colormod[0] *= lightR;
				wverts[i].colormod[1] *= lightG;
				wverts[i].colormod[2] *= lightB;
				wverts[i].colormod[3] = visibility;

				const auto& obj = Objects[objno];
				if (obj.control_type == CT_MORPH) {
					const auto& pinf = obj.rtype.pobj_info;
					float size = f2fl(fixdiv(pinf.max_morph_time - pinf.morph_time, pinf.max_morph_time));
					wverts[i].pos[0] *= size;
					wverts[i].pos[1] *= size;
					wverts[i].pos[2] *= size;
				}

			}

			uint32_t vsize = nv * sizeof(*wverts);
			uint32_t isize = batch.indices.size() * sizeof(batch.indices[0]);

			SDL_GPUBufferCreateInfo bci{
				.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX,
				.size = vsize + isize
			};

			if (rendererState.drawTransferBuffer.memoryMap == NULL || rendererState.drawTransferOffset + vsize + isize > rendererState.drawTransferBuffer.size) {

				if (rendererState.drawTransferBuffer.memoryMap)
					FreeTransferBuffer(rendererState.drawTransferBuffer);

				uint32_t msize = Segments.size() * VERTEX_TRANSFER_FACTOR;
				if (msize < vsize + isize) {
					Int3(); //Shouldn't ever happen, but just to be safe...
					msize = vsize + isize;
				}

				msize = std::bit_ceil(msize);

				SDL_GPUTransferBufferCreateInfo tbci{
					.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
					.size = msize
				};

				rendererState.drawTransferBuffer = CreateTransferBuffer(&tbci, true);
				rendererState.drawTransferOffset = 0;

				mprintf((0, "Reallocated vertex transfer buffer in polyobj"));

			}

			SDL_GPUBuffer* drawBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
			if (drawBuffer == NULL)
				Error("Error creating vertex buffer: %s", SDL_GetError());

			Assert(rendererState.drawTransferBuffer.memoryMap != NULL); //So VS would shut up
			memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset, wverts, vsize);
			memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset + vsize, batch.indices.data(), isize);

			SDL_GPUTransferBufferLocation tbl{
				.transfer_buffer = rendererState.drawTransferBuffer.buffer,
				.offset = rendererState.drawTransferOffset
			};

			SDL_GPUBufferRegion br{
				.buffer = drawBuffer,
				.offset = 0,
				.size = vsize + isize
			};

			SDL_UploadToGPUBuffer(cpass, &tbl, &br, true);

			SDL_GPUBufferBinding bb{
				.buffer = drawBuffer,
				.offset = 0
			};
			SDL_BindGPUVertexBuffers(rpass, 0, &bb, 1);

			//bb.buffer = indexBuffer;
			bb.offset = vsize;
			SDL_BindGPUIndexBuffer(rpass, &bb, SDL_GPU_INDEXELEMENTSIZE_32BIT);

			SDL_GPUTextureSamplerBinding tsb[]{ {
				.texture = rendererState.secondaryTexture,
				.sampler = rendererState.defaultSampler
			}, {
				.texture = rendererState.primaryTexture,
				.sampler = rendererState.defaultSampler
			} };
			SDL_BindGPUFragmentSamplers(rpass, 0, tsb, 2);

			SDL_GPUBuffer* storageBuffers[]{ rendererState.paletteBuffer };// , rendererState.paletteBuffer}; //TODO: need portal buffer
			SDL_BindGPUFragmentStorageBuffers(rpass, 0, storageBuffers, SDL_arraysize(storageBuffers));

			SDL_DrawGPUIndexedPrimitives(rpass, batch.indices.size(), 1, 0, 0, 0);

			SDL_ReleaseGPUBuffer(rendererState.device, drawBuffer);

			rendererState.drawTransferOffset += vsize + isize;

			//delete[] verts;
			delete[] wverts;
			//delete[] winds;

		});
	}


	static void RenderModelInstance() {
	
	}

	static void RenderEntirePolymodel(const ViewTarget target, const int segno, const size_t objno, const vms_angvec anim_angles[], vms_matrix& currentAngle, vms_vector& currentOffset, const int model_num, const float light, const short textureOverride, const float visibility, Polymodel* root, int& i) {
	
		Assert(i < MAX_SUBMODELS);

		for (auto& batch : root->batches) {

			vms_angvec angvec;
			if (anim_angles)
				angvec = anim_angles[root->angleID];
			else
				angvec = ZERO_VECTOR;

			SideDrawKey key(batch.first, NULL, target);

			{

				std::lock_guard lg(modelDrawCallMutex);

				if (modelDrawCalls.count(key) == 0)
					modelDrawCalls[key] = std::vector<ObjDrawCall>();

				std::vector<ObjDrawCall>& drawCalls = modelDrawCalls[key];

				//Texture page already set
				EmplaceModelDrawCall(drawCalls, target, batch.second, segno, objno, currentAngle, currentOffset, light, textureOverride, visibility);

			}

		}

		vms_matrix modelAnimation = currentAngle;
		vms_vector modelOffset = currentOffset;

		for (auto& sub : root->submodelIndices) {

			Polymodel* model = &models[sub];
			
			if (model->submodelID >= 0) {

				i++;
				Assert(i < MAX_SUBMODELS);

				vms_matrix subAnimation;
				vm_angles_2_matrix(&subAnimation, const_cast<vms_angvec*>(anim_angles + model->angleID));

				vms_matrix inverseRotation;
				vm_copy_transpose_matrix(&inverseRotation, &currentAngle);

				vm_vec_rotate(&modelOffset, &model->offset, &inverseRotation);
				vm_vec_add2(&modelOffset, &currentOffset);

				if (Objects[objno].control_type == CT_MORPH) {

					const auto& pinf = Objects[objno].rtype.pobj_info;
					fix morphTime = pinf.morph_time;
					fix maxMorphTime = pinf.max_morph_time;
					fix size = fixdiv(maxMorphTime - morphTime, maxMorphTime);

					vm_vec_scale2(&modelOffset, size, F1_0);

				}

				vm_matrix_x_matrix(&modelAnimation, &subAnimation, &currentAngle);
			}

			RenderEntirePolymodel(target, segno, objno, anim_angles, currentAngle, modelOffset, model_num, light, textureOverride, visibility, model, i);
		}

	}

	void RenderPolymodel(const ViewTarget target, const int segno, const size_t objno, const vms_angvec anim_angles[], const int model_num, const int flags, const float light, const short textureOverride, const float visibility) {
	
		size_t index = modelIDXlat[model_num * MAX_SUBMODELS];
		Assert(index >= 0 && index < models.size());

		if (flags != 0) {

			Polymodel* modelPtrs[MAX_SUBMODELS];
			for (int i = 0; i < MAX_SUBMODELS; i++) {
				modelPtrs[i] = NULL;
				size_t ind = modelIDXlat[model_num * MAX_SUBMODELS + i];
				if (ind >= 0 && ind < models.size())
					modelPtrs[i] = &models[ind];
			}

			for (int i = 0; i < MAX_SUBMODELS; i++) {
				if (flags == 0 || ((flags & (1 << i)) && modelPtrs[i])) {
					for (auto& batch : modelPtrs[i]->batches) {

						vms_angvec angvec = anim_angles[i];

						SideDrawKey key(batch.first, NULL, target);

						{

							std::lock_guard lg(modelDrawCallMutex);

							if (modelDrawCalls.count(key) == 0)
								modelDrawCalls[key] = std::vector<ObjDrawCall>();

							std::vector<ObjDrawCall>& drawCalls = modelDrawCalls[key];

							//Texture page already set
							EmplaceModelDrawCall(drawCalls, target, batch.second, segno, objno, IDENTITY_MATRIX_INST, vmd_zero_vector, light, textureOverride, visibility);

						}

					}
				}
			}

		} else {

			vms_matrix activeRotation = IDENTITY_MATRIX;
			vms_vector activeOffset = vmd_zero_vector;
			int i = 0;

			RenderEntirePolymodel(target, segno, objno, anim_angles, activeRotation, activeOffset, model_num, light, textureOverride, visibility, &models[index], i);

		}

		return;

	}

	constexpr float CLOAK_VISIBILITY = 0.15f;

	void RenderPolyObj(const ViewTarget target, const size_t objno, const int segno, const float visibility) {
		
		auto& object = Objects[objno];

		const polyobj_info& pinf = object.rtype.pobj_info; 

		short override = -1;
		if (pinf.tmap_override >= 0) {
			override = activeBMTable->textures[pinf.tmap_override].index;
		}

		float light = f2fl(currentGame == G_DESCENT_2 ? Segment2s[segno].static_light : Segments[segno].static_light);
		//light *= light;
		if (light > 1 || (object.type == OBJ_FIREBALL || object.type == OBJ_WEAPON || object.type == OBJ_FLARE || object.type == OBJ_MARKER))
			light = 1;

		float vmod = 1.f;
		if (visibility == CLOAK_VISIBILITY) {
			float vmRaw = (0.75f + (SDL_randf() / 2));
			vmod = light * vmRaw;
			light /= (1 + vmRaw);
		}

		bool hasInner = false;
		if (object.control_type == CT_WEAPON) {
			auto innerModel = activeBMTable->weapons[object.id].model_num_inner;
			if (innerModel >= 0) {
				RenderPolymodel(target, segno, objno, pinf.anim_angles, innerModel, pinf.subobj_flags, light, override, visibility * 10.f * vmod);
				hasInner = true;
			}
		}

		RenderPolymodel(target, segno, objno, pinf.anim_angles, (hasInner ? activeBMTable->weapons[object.id].model_num : pinf.model_num), pinf.subobj_flags, light, override, (hasInner ? visibility * 0.9f : visibility) * vmod);

	}
	
	void RenderObject(const ViewTarget target, const int segno, const int objno) {

		const object& obj = Objects[objno];
		uint8_t rtypeid = obj.render_type;

		ObjDrawCall call = [](SDL_GPUCommandBuffer*, SDL_GPURenderPass*, SDL_GPUCopyPass*) {};

		switch (rtypeid) {

			case RT_NONE:
				return;

			case RT_MORPH:
			case RT_POLYOBJ: {

				float visibility = (obj.type == OBJ_PLAYER && Players[obj.id].flags & PLAYER_FLAGS_CLOAKED) ? CLOAK_VISIBILITY : 1.f;

				if (obj.type == OBJ_ROBOT) {
					
					const ai_static& ais = obj.ctype.ai_info;
					const ai_local& ail = Ai_local_info[objno];
				
					if (ais.CLOAKED == RI_CLOAKED_ALWAYS)
						visibility = CLOAK_VISIBILITY;
					else if (ais.CLOAKED == RI_CLOAKED_EXCEPT_FIRING)
						visibility = CLOAK_VISIBILITY; //TODO: scale by fire time
					
				}

				if (obj.control_type == CT_MORPH) {
					//constexpr fix three = F1_0 * 3;
					const auto& pinf = obj.rtype.pobj_info;
					const fix extMaxMorph = pinf.max_morph_time * 4 / 3; //add a little time so it starts a little visible
					visibility *= f2fl(fixdiv(extMaxMorph - pinf.morph_time, extMaxMorph)); 
				}

				//Will emplace its own calls
				RenderPolyObj(target, objno, segno, visibility);
				return;

				//call = [](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {};
				//break;

			}

			case RT_HOSTAGE:
			case RT_LASER:
			case RT_POWERUP:
			case RT_FIREBALL:
			case RT_WEAPON_VCLIP: {

				//auto tp = rendererState.tpageLocations[0];
				int bm;

				if (rtypeid == RT_LASER) {

					bm = activeBMTable->weapons[obj.id].bitmap.index;
				
				} else {

					int vcid = obj.rtype.vclip_info.vclip_num;
					int frame = obj.rtype.vclip_info.framenum;

					if (rtypeid == RT_FIREBALL)
						vcid = obj.id;
					else if (rtypeid == RT_WEAPON_VCLIP)
						vcid = activeBMTable->weapons[obj.id].weapon_vclip;

					const auto& vclip = activeBMTable->vclips[vcid];

					int time = obj.lifeleft;

					if (rtypeid == RT_WEAPON_VCLIP || rtypeid == RT_FIREBALL) {

						if (rtypeid == RT_WEAPON_VCLIP) {

							fix play_time = vclip.play_time;

							//	Special values for modtime were causing enormous slowdown for omega blobs.
							if (time == IMMORTAL_TIME)
								time = play_time;

							//	Should cause Omega blobs (which live for one frame) to not always be the same.
							if (time == ONE_FRAME_TIME)
								time = P_Rand();

							if (obj.id == PROXIMITY_ID) //make prox bombs spin out of sync
							{
								time += (time * (objno & 7)) / 16;	//add variance to spin rate

								while (time > play_time)
									time -= play_time;

								if ((objno & 1) ^ ((objno >> 1) & 1))			//make some spin other way
									time = play_time - time;

							}
							else
							{
								while (time > play_time)
									time -= play_time;
							}

						}
					
						int nf = vclip.num_frames;
						frame = (nf - f2i(fixdiv((nf - 1) * time, vclip.play_time))) - 1;
						if (frame >= nf) {
							frame = nf - 1;
						}

					}

					bm = vclip.frames[frame].index;

				}

				const auto& tp = rendererState.tpageLocations[bm];

				mat4f model = M4_IDENTITY_MATRIX_MACRO;

				mat4f* viewMat;
				switch (target) {

					case VT_MAIN:
						viewMat = &rendererState.mainViewMatrix;
					break;

					case VT_LEFT:
						viewMat = &rendererState.subViewMatrixL;
					break;

					case VT_RIGHT:
						viewMat = &rendererState.subViewMatrixR;
					break;

					default:
						Int3();

				}

				if (rtypeid != RT_HOSTAGE) {

					for (int i = 0; i < 3; i++) {
						for (int j = 0; j < 3; j++) {
							model[i][j] = (*viewMat)[j][i];
						}
						model[3][i] = f2fl(obj.pos[i]);
					}

				} else {

					//rotate forward vector until it's in same plane as pos -> pos + up -> camera pos

					// [DW] WHY IS THE FIX LIB LIKE THIS
					object* objpNonConst = const_cast<object*>(&obj);

					vms_vector direction = obj.pos;
					vm_vec_sub2(&direction, &ConsoleObject->pos);

					vms_vector normal = vmd_zero_vector;
					
					vm_vec_cross(&normal, &direction, &objpNonConst->orient.uvec);
					
					if (labs(normal.x) < FIX_EPSILON && labs(normal.y) < FIX_EPSILON && labs(normal.z) < FIX_EPSILON) //can't see it anyway
						return;

					vm_vec_normalize(&normal);
					
					fixang angle = vm_vec_delta_ang_norm(&objpNonConst->orient.fvec, &normal, NULL);
					if (vm_vec_dot(&objpNonConst->orient.fvec, &direction) > 0)
						angle = -angle;

					angle += F1_0 / 4;

					vms_matrix rot;
					vms_matrix final;
					
					vms_angvec ra {
						.p = 0,
						.b = 0,
						.h = angle,
						//.h = 0,
					};
					vm_angles_2_matrix(&rot, &ra);
					
					vm_matrix_x_matrix(&final, &objpNonConst->orient, &rot);

					//final[0].x = -final[0].x;
					//final[1].x = -final[1].x;
					//final[2].x = -final[2].x;

					//vm_vec_negate(&final.rvec);
					//vm_vec_negate(&final.fvec);

					for (int i = 0; i < 3; i++) {
						for (int j = 0; j < 3; j++) {
							model[i][j] = f2fl(final[i][j]);
						}
						model[3][i] = f2fl(obj.pos[i]); 
					}

				}

				const float size = f2fl(obj.size);
				const float aspect = (float)activePiggyTable->gameBitmaps[bm].bm_w / activePiggyTable->gameBitmaps[bm].bm_h;

				float light = 1.f;

				if (!cheatValues[CI_FULLBRIGHT]) {

					if (rtypeid == RT_POWERUP) {
						if (!(obj.id == POW_ENERGY || obj.id == POW_SHIELD_BOOST || obj.id == POW_EXTRA_LIFE || obj.id == POW_INVULNERABILITY || obj.id == POW_CLOAK)) {
							light = f2fl(currentGame == G_DESCENT_2 ? Segment2s[segno].static_light : Segments[segno].static_light);
						}
					}

					if (light > 1)
						light = 1;
				
				}

				//float lightR, lightG, lightB;
				//lightR = lightG = lightB = segLight;

				WorldVertex wverts[4] = {
					WorldVertex {
						.pos = {
							-size, -size / aspect, 0
						},
						.uv = {
							0, 1
						},
						.props = {
							segno,
							tp.second,
							-1,
							0
						},
						.colormod = {
							light,
							light,
							light,
							1.f
						}
					},
					WorldVertex {
						.pos = {
							-size, size / aspect, 0
						},
						.uv = {
							0, 0
						},
						.props = {
							segno,
							tp.second,
							-1,
							0
						},
						.colormod = {
							light,
							light,
							light,
							1.f
						}
					},
					WorldVertex {
						.pos = {
							size, size / aspect, 0
						},
						.uv = {
							1, 0
						},
						.props = {
							segno,
							tp.second,
							-1,
							0
						},
						.colormod = {
							light,
							light,
							light,
							1.f
						}
					},
					WorldVertex {
						.pos = {
							size, -size / aspect, 0
						},
						.uv = {
							1, 1
						},
						.props = {
							segno,
							tp.second,
							-1,
							0
						},
						.colormod = {
							light,
							light,
							light,
							1.f
						}
					}
				};

				TexturePage* tpage = &rendererState.tpages[tp.first];

				call = [objno, segno, wverts, model, tpage, rtypeid](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {

					UploadTexturePage(tpage, cpass);

					UploadVertexMatrix(combuf, &model, MID_MODEL);

					constexpr uint32_t winds[6] = { 0, 1, 2, 0, 2, 3 };

					constexpr size_t vsize = 4 * sizeof(WorldVertex);
					constexpr size_t isize = 6 * sizeof(uint32_t);

					SDL_GPUBufferCreateInfo bci {
						.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX,
						.size = vsize + isize
					};

					if (rendererState.drawTransferBuffer.memoryMap == NULL || rendererState.drawTransferOffset + vsize + isize > rendererState.drawTransferBuffer.size) {

						if (rendererState.drawTransferBuffer.memoryMap)
							FreeTransferBuffer(rendererState.drawTransferBuffer);

						uint32_t msize = Segments.size() * VERTEX_TRANSFER_FACTOR;
						if (msize < vsize + isize) {
							Int3(); //Shouldn't ever happen, but just to be safe...
							msize = vsize + isize;
						}

						msize = std::bit_ceil(msize);

						SDL_GPUTransferBufferCreateInfo tbci{
							.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
							.size = msize
						};

						rendererState.drawTransferBuffer = CreateTransferBuffer(&tbci, true);
						rendererState.drawTransferOffset = 0;

						mprintf((0, "Reallocated vertex transfer buffer\n"));

					}

					SDL_GPUBuffer* drawBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
					if (drawBuffer == NULL)
						Error("Error creating vertex buffer: %s", SDL_GetError());

					memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset, wverts, vsize);
					memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset + vsize, winds, isize);

					SDL_GPUTransferBufferLocation tbl{
						.transfer_buffer = rendererState.drawTransferBuffer.buffer,
						.offset = rendererState.drawTransferOffset
					};

					SDL_GPUBufferRegion br{
						.buffer = drawBuffer,
						.offset = 0,
						.size = vsize + isize
					};

					SDL_UploadToGPUBuffer(cpass, &tbl, &br, true);

					SDL_GPUBufferBinding bb{
						.buffer = drawBuffer,
						.offset = 0
					};
					SDL_BindGPUVertexBuffers(rpass, 0, &bb, 1);

					//bb.buffer = indexBuffer;
					bb.offset = vsize;
					SDL_BindGPUIndexBuffer(rpass, &bb, SDL_GPU_INDEXELEMENTSIZE_32BIT);

					SDL_GPUTextureSamplerBinding tsb[]{ {
						.texture = rendererState.secondaryTexture,
						.sampler = rendererState.defaultSampler
					}, {
						.texture = rendererState.primaryTexture,
						.sampler = rendererState.defaultSampler
					} };
					SDL_BindGPUFragmentSamplers(rpass, 0, tsb, 2);

					SDL_GPUBuffer* storageBuffers[]{ rendererState.paletteBuffer };// , rendererState.paletteBuffer}; //TODO: need portal buffer
					SDL_BindGPUFragmentStorageBuffers(rpass, 0, storageBuffers, SDL_arraysize(storageBuffers));

					SDL_DrawGPUIndexedPrimitives(rpass, 6, 1, 0, 0, 0);

					SDL_ReleaseGPUBuffer(rendererState.device, drawBuffer);

					rendererState.drawTransferOffset += vsize + isize;

				};

				break;

			}

			default:
				mprintf((1, "\nUnrecognized render type! %d\n", rtypeid));

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
					if (cheatValues[CI_FULLBRIGHT])
						lightR = lightG = lightB = 1.f;
					else
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
				.texture = rendererState.windowCTarget.texture,
				.x = 0,
				.y = 0,
				.w = rendererState.renderWidth,
				.h = rendererState.renderHeight,
			},
			.destination = {
				.texture = NULL,
				.x = 0,
				.y = 0,
				.w = rendererState.renderWidth,
				.h = rendererState.renderHeight,
			},
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.cycle = true,
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

			UploadVertexMatrix(mainCommandBuffer, &M4_IDENTITY_MATRIX, MID_ANIM);

			if (objDrawCalls.count(VT_MAIN) > 0) {
				for (auto& call : objDrawCalls[VT_MAIN]) {
					call(mainCommandBuffer, mainRenderPass, mainCopyPass);
				}
			}

			SDL_EndGPUCopyPass(mainCopyPass);
			SDL_SubmitGPUCommandBuffer(mainCopyBuffer);

			SDL_EndGPURenderPass(mainRenderPass);
			SDL_SubmitGPUCommandBuffer(mainCommandBuffer);

			//SDL_BlitGPUTexture(rendererState.mainCommandBuffer, &bi);
			PasteTextureToWorldCanvas(rendererState.mainCTarget.texture, -1, -1, 1, 1);

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
		SDL_ReleaseGPUGraphicsPipeline(rendererState.device, rendererState.pastePipeline);

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

		SDL_ReleaseGPUTexture(rendererState.device, rendererState.fadeScreenTexture);

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