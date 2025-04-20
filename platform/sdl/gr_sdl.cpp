/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/
/*
*	Code for SDL integration of the GR library
*
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_SDL

#include "SDL.h"
#include "SDL_video.h"
#include "SDL_surface.h"
#include "SDL_pixels.h"
#include "SDL_mouse.h"
#include "SDL_render.h"

#include "2d/gr.h"
#include "misc/error.h"
#include "misc/types.h"

#include "platform/joy.h"
#include "platform/mono.h"
#include "platform/mouse.h"
#include "platform/platform.h"
#include "platform/renderapi.h"
#include "platform/key.h"
#include "platform/timer.h"

#define FITMODE_BEST 1
#define FITMODE_FILTERED 2

const char* titleMsg = "Hotshot (" __DATE__ ")";

int WindowWidth = 1600, WindowHeight = 900;
int CurWindowWidth, CurWindowHeight;
SDL_Window* gameWindow = NULL;
grs_canvas* screenBuffer;

int BestFit = 0;
int Fullscreen = 0;
int SwapInterval = 0;

SDL_Rect screenRectangle, sourceRectangle;
SDL_Surface* softwareSurf = nullptr;

uint32_t localPal[256];

int refreshDuration = US_70FPS;

SDL_ScaleMode scaleMode;

int plat_init()
{
	int res;

	res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
	if (!res)
	{
		Error("Error initalizing SDL: %s\n", SDL_GetError());
		return res;
	}

	plat_read_chocolate_cfg();
	return 0;
}

int plat_create_window()
{
	
	CurWindowWidth = WindowWidth;
	CurWindowHeight = WindowHeight;
	int flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;

//#ifdef __APPLE__ 
#if 0
	flags |= SDL_WINDOW_METAL;
#else
	flags |= SDL_WINDOW_VULKAN;
#endif
	
	if (Fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;

	//SDL is good, create a game window
	gameWindow = SDL_CreateWindow(titleMsg, WindowWidth, WindowHeight, flags);
	
	if (!gameWindow)
	{
		Error("Error creating game window: %s\n", SDL_GetError());
		return 1;
	}
	//where else do i do this...
	I_InitSDLJoysticks();

	int error = HRender::InitRenderAPI();
	if (error != 0) {

		Error("Init render API error: %d", error);

	}

	SDL_ShowWindow(gameWindow);

	if (Fullscreen)
		SDL_GetWindowSize(gameWindow, &CurWindowWidth, &CurWindowHeight);

	return 0;
}

void plat_close_window()
{
	
	if (gameWindow)
	{
		SDL_DestroyWindow(gameWindow);
		gameWindow = NULL;
	}
}

int plat_check_gr_mode(int mode)
{
	//For now, high color modes are rejected (were those ever well supported? or even used?)
	switch (mode)
	{
	case SM_320x200C:
	case SM_320x200U:
	case SM_320x240U:
	case SM_360x200U:
	case SM_360x240U:
	case SM_376x282U:
	case SM_320x400U:
	case SM_320x480U:
	case SM_360x400U:
	case SM_360x480U:
	case SM_360x360U:
	case SM_376x308U:
	case SM_376x564U:
	case SM_640x400V:
	case SM_640x480V:
	case SM_800x600V:
	case SM_1024x768V:
	case 19:
	case 21:
	case SM_1280x1024V: return 0;
	}
	return 11;
}

void I_SetScreenRect(int w, int h)
{
	//Create the destination rectangle for the game screen
	int bestWidth = CurWindowHeight * 4 / 3;
	if (CurWindowWidth < bestWidth) bestWidth = CurWindowWidth;
	sourceRectangle.x = sourceRectangle.y = 0;
	sourceRectangle.w = w; sourceRectangle.h = h;

	if (BestFit == FITMODE_FILTERED && h <= 400)
	{
		scaleMode = SDL_SCALEMODE_LINEAR;
		w *= 2; h *= 2;
	}
	else
		scaleMode = SDL_SCALEMODE_NEAREST;
	
	if (BestFit == FITMODE_BEST)
	{
		int numWidths = bestWidth / w;
		screenRectangle.w = numWidths * w;
		screenRectangle.h = (screenRectangle.w * 3 / 4);
		screenRectangle.x = (CurWindowWidth - screenRectangle.w) / 2;
		screenRectangle.y = (CurWindowHeight - screenRectangle.h) / 2;
	}
	else
	{
		screenRectangle.w = bestWidth;
		screenRectangle.h = (screenRectangle.w * 3 / 4);
		screenRectangle.x = screenRectangle.y = 0;
		screenRectangle.x = (CurWindowWidth - screenRectangle.w) / 2;
		screenRectangle.y = (CurWindowHeight - screenRectangle.h) / 2;
	}

	HRender::ResizeRenderTarget(w, h);
	
}

void plat_toggle_fullscreen()
{
	if (Fullscreen)
	{
		SDL_SetWindowFullscreen(gameWindow, SDL_WINDOW_FULLSCREEN);
		SDL_GetWindowSize(gameWindow, &CurWindowWidth, &CurWindowHeight);
	}
	else
	{
		SDL_SetWindowFullscreen(gameWindow, 0);
		//SDL_SetWindowSize(gameWindow, WindowWidth, WindowHeight);
		CurWindowWidth = WindowWidth; CurWindowHeight = WindowHeight;
		HRender::ResizeWindow();
	}

	I_SetScreenRect(grd_curscreen->sc_w, grd_curscreen->sc_h);
}

void plat_update_window()
{
	SDL_SetWindowSize(gameWindow, WindowWidth, WindowHeight);
	SDL_SetWindowPosition(gameWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	HRender::ResizeWindow();

	plat_toggle_fullscreen();
}

int plat_set_gr_mode(int mode)
{
	int w, h;

	refreshDuration = US_60FPS;
	switch (mode)
	{
	case SM_320x200C:
	case SM_320x200U:
		w = 320; h = 200; refreshDuration = US_70FPS;
		break;
	case SM_320x240U:
		w = 320; h = 240; refreshDuration = US_70FPS; //these need to be checked
		break;
	case SM_360x200U:
		w = 360; h = 200; refreshDuration = US_70FPS;
		break;
	case SM_360x240U:
		w = 360; h = 240;
		break;
	case SM_376x282U:
		w = 376; h = 282;
		break;
	case SM_320x400U:
		w = 320; h = 400;
		break;
	case SM_320x480U:
		w = 320; h = 480;
		break;
	case SM_360x400U:
		w = 360; h = 400;
		break;
	case SM_360x480U:
		w = 360; h = 480;
		break;
	case SM_376x308U:
		w = 376; h = 308;
		break;
	case SM_376x564U:
		w = 376; h = 564;
		break;
	case SM_640x400V:
		w = 640; h = 400;
		break;
	case SM_640x480V:
		w = 640; h = 480;
		break;
	case SM_800x600V:
		w = 800; h = 600;
		break;
	case SM_1024x768V:
		w = 1024; h = 768;
		break;
	case 19:
		w = 320; h = 100;
		break;
	case 21:
		w = 160; h = 100;
		break;
	case SM_1280x1024V:
		w = 1280; h = 1024;
		break;
	default:
		Error("plat_set_gr_mode: bad mode %d\n", mode);
		return 0;
	}

	//HRender::ResizeRenderTarget(w, h);

	//[ISB] this should hopefully fix all instances of the screen flashing white when changing modes
	plat_write_palette(0, 255, gr_palette);
	I_SetScreenRect(w, h);
	
	return 0;
}

void I_ScaleMouseToWindow(float* x, float* y)
{
	*x = (*x * screenRectangle.w / CurWindowWidth);
	*y = (*y * screenRectangle.h / CurWindowHeight);
	if (*x < 0) *x = 0; if (*x >= screenRectangle.w) *x = screenRectangle.w - 1;
	if (*y < 0) *y = 0; if (*y >= screenRectangle.h) *y = screenRectangle.h - 1;
}

void plat_do_events()
{
	SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		
		case SDL_EVENT_WINDOW_RESIZED: {
			SDL_WindowEvent winEv = ev.window;
			WindowWidth = CurWindowWidth = winEv.data1;
			WindowHeight = CurWindowHeight = winEv.data2;
			break;
		}
		case SDL_EVENT_WINDOW_FOCUS_GAINED: 
			SDL_FlushEvents(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP);
		break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			I_MouseHandler(ev.button.button, ev.button.down);
			break;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			if (ev.key.scancode == SDL_SCANCODE_RETURN && ev.key.down && ev.key.mod & SDL_KMOD_ALT)
			{
				Fullscreen ^= 1;
				plat_toggle_fullscreen();
			}
			else
				I_KeyHandler(ev.key.scancode, ev.key.down);
			break;
		}
	}

	I_JoystickHandler();
	I_ControllerHandler();
}

void plat_set_mouse_relative_mode(int state)
{
	bool formerState = SDL_GetWindowRelativeMouseMode(gameWindow);
	SDL_SetWindowRelativeMouseMode(gameWindow, (bool)state); 
	if (state && !formerState)
	{
		float bogusX, bogusY;
		SDL_GetRelativeMouseState(&bogusX, &bogusY);
	}
	if ((state || formerState) && (SDL_GetWindowFlags(gameWindow) & SDL_WINDOW_INPUT_FOCUS)) // [DW] Fix issue where mouse would be stuck within the window but would still try to get out, dropping input when it did
	{
		SDL_WarpMouseInWindow(gameWindow, CurWindowWidth / 2, CurWindowHeight / 2);
	}
}

uint8_t platPalette[SDL_arraysize(gr_palette)];

void plat_write_palette(int start, int end, uint8_t* data)
{
	
	int istart = start * 3;
	int iend = end * 3 + 2;

	for (int i = 0; i <= iend - istart; i++) {
		platPalette[istart + i] = data[i];
	}

	HRender::UploadPalette(platPalette);

}

void plat_blank_palette()
{
	uint8_t pal[768];
	memset(pal, 0, sizeof(pal));
	plat_write_palette(0, 255, pal);
}

void plat_read_palette(uint8_t* dest)
{
	memcpy(dest, platPalette, sizeof(platPalette));
}

void plat_wait_for_vbl()
{
	I_MarkEnd(refreshDuration);
	I_MarkStart();
}

extern uint8_t* gr_video_memory;
void I_SoftwareBlit()
{
	int x, y;
	int sourcePitch = grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize;
	int destPitch = softwareSurf->pitch;
	uint8_t* source = gr_video_memory;
	if (SDL_LockSurface(softwareSurf))
		Error("Failed to lock software surface for blitting");

	uint8_t* dest = (uint8_t*)softwareSurf->pixels;

	for (y = 0; y < softwareSurf->h; y++)
	{
		for (x = 0; x < softwareSurf->w; x++)
		{
			*(uint32_t*)&dest[x<<2] = localPal[source[x]];
		}
		source += sourcePitch;
		dest += destPitch;
	}

	SDL_UnlockSurface(softwareSurf);

	SDL_Surface* windowSurf = SDL_GetWindowSurface(gameWindow);
	SDL_BlitSurfaceScaled(softwareSurf, &sourceRectangle, windowSurf, &screenRectangle, scaleMode);
}

void plat_present_canvas(int sync)
{
	if (sync)
	{
		SDL_Delay(1000 / 70);
	}

	HRender::EndRenderFrame();

}

void plat_blit_canvas(grs_canvas *canv)
{
	HRender::RenderScreenCanvas(canv);
}

void plat_close()
{
	plat_close_window();
	SDL_Quit();
}

void plat_display_error(const char* msg)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Error", msg, gameWindow);
}

#endif
