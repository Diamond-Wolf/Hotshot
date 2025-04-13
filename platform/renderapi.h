/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#ifndef PLAT_RENDER_API_H
#define PLAT_RENDER_API_H

#include "vecmat/vecmat.h"
#include "main/segment.h"
#include "main/object.h"

constexpr float VFOV_DEG_F = 70.f;
constexpr float VFOV_RAD_F = VFOV_DEG_F * 3.1415926536f / 180.f;
constexpr float VFOV_F = VFOV_DEG_F / 360.f;
constexpr fix VFOV = fl2f(VFOV_F);

constexpr float NEAR_CLIP_F = 0.01f;
constexpr float FAR_CLIP_F = 1000.f;

constexpr fix NEAR_CLIP = fl2f(NEAR_CLIP_F);
constexpr fix FAR_CLIP = fl2f(FAR_CLIP_F);

namespace HRender {

	enum ViewTarget {
		VT_MAIN,
		VT_LEFT,
		VT_RIGHT,
		VT_NONE
	};

	struct TexturePage;

	int InitRenderAPI();

	void PrepareMineRenderFrame();

	void UploadPalette();
	void UploadPalette(uint8_t* data);

	void ResizeWindow();
	void ResizeRenderTarget(const unsigned int w, const unsigned int h);

	void SyncCockpit();

	//void UpdateProjectionMatrices(const fix mainAspect);
	//void UpdateProjectionMatrices(const float mainAspect);

	void UpdateViewMatrix(object* source, const ViewTarget target);

	//If clone != target, then clone target rather than rebuilding. Set clone = target to actually build.
	//Required to call even for inactive windows, so inactive windows can clone VT_NONE.
	void BuildGPUPortalList(const std::vector<short>& segments, const vms_vector& pos, const vms_vector& dir, const ViewTarget target, const ViewTarget clone);
	void SkipGPUPortalList(const ViewTarget target);
	//TexturePage* CreateTexturePage(grs_bitmap* bitmap);
	//void FreeTexturePage(TexturePage* page);
	void GenerateTexturePages();

	void RenderScreenCanvas(grs_canvas* canvas);
	void RenderScreenBitmap(grs_bitmap* bitmap);

	void RenderSide(const ViewTarget target, const int segno, const int sideno);

	void EndRenderFrame();

	void ShutdownRenderAPI();

}

#endif