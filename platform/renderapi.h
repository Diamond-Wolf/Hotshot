/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#ifndef PLAT_RENDER_API_H
#define PLAT_RENDER_API_H

namespace HRender {

	struct TexturePage;

	int InitRenderAPI();

	void BeginRenderFrame();

	void UploadPalette();
	void UploadPalette(uint8_t* data);

	void ResizeWindow(const int w, const int h);
	void ResizeRenderTarget(const unsigned int w, const unsigned int h);

	void BuildGPUPortalList();

	void RenderScreenCanvas(grs_canvas* canvas);
	void RenderScreenBitmap(grs_bitmap* bitmap);

	void EndRenderFrame();

	void ShutdownRenderAPI();

}

#endif