/*
The code contained in this file
is not the property of Parallax Software, and is not under the terms of the
Parallax Software Source license.Instead, it is released under the terms
of the MIT License.
*/

#include "platform/renderapi.h"

#include <SDL.h>
#include <future>
#include <map>

#ifndef SDL3_RENDER_H
#define SDL3_RENDER_H

constexpr fix MAX_VELOCITY = i2f(50);

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

	inline struct SDLRenderStruct {

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
	typedef std::tuple<TexturePage*, ViewTarget, size_t> ModelDrawKey; //last is model ID

	typedef std::function<void(SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass)> ObjDrawCall;

	struct dkeyCompare {
		bool operator()(const SideDrawKey a, const SideDrawKey b) const;
	};

	inline std::map<SideDrawKey, std::vector<SideDrawCall>, dkeyCompare> sideDrawCalls;
	inline std::mutex sideDrawCallMutex;

	inline std::map<SideDrawKey, std::vector<ObjDrawCall>, dkeyCompare> modelDrawCalls;
	inline std::mutex modelDrawCallMutex;

	inline std::map<ViewTarget, std::vector<ObjDrawCall>> objDrawCalls;
	inline std::mutex objDrawCallMutex;

	inline std::vector<WorldVertex> worldVertices;
	inline std::vector<uint32_t> worldIndices;

	inline std::vector<SDL_GPUTexture*> textureFreeQueue;

	struct ModelFaceBatch {
		std::vector<WorldVertex> verts;
		std::vector<uint32_t> indices;
	};

	struct Polymodel {
		std::unordered_map<TexturePage*, ModelFaceBatch> batches;
		//std::vector<Polymodel*> submodels;
		std::vector<size_t> submodelIndices;
		int submodelID;

		int angleID;
		vms_vector offset;
	};

	inline std::vector<Polymodel> models;
	inline std::vector<size_t> modelIDXlat;

	constexpr float PALETTE_DIV = 63.f;

	void InitPolymodelInterpreter();
	void GenerateModels();

	void UploadVertexMatrix(SDL_GPUCommandBuffer* cbuf, const mat4f* matrix, const MatrixID id);
	void FreeTransferBuffer(TransferBuffer buffer);
	void RenderPolymodelSub(const ViewTarget target, const int segno, const void* model_ptr, const std::vector<grs_bitmap*>& model_bitmaps, const std::vector<short>& bitmapIDs, const vms_angvec anim_angles[], const fix model_light, const fix glow_values[], const vms_vector* origin, const vms_matrix* rotation);
	TransferBuffer CreateTransferBuffer(const SDL_GPUTransferBufferCreateInfo* tbci, const bool cycle);

	
}

#endif