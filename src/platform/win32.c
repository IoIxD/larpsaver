#include <windows.h>
#include <scrnsave.h>
#include <stdio.h>

#define _LARPSAVER
#include <larpsaver.h>

static larpsaver_ctx* ctx = NULL;

larpsaver_platform* larpsaver_platform_init(int * supported_apis) {
	/* Pretty much all of the actual initialiation happens in ScreenSaverProc/WM_CREATE */

	larpsaver_platform * plat = malloc(sizeof(struct larpsaver_platform_t));
	if (plat) {
		memset(plat, 0, sizeof(*plat));
	}
	
	if(plat)
	{
		plat->wglLib = LoadLibrary("opengl32.dll");
		if (plat->wglLib)
		{
			*supported_apis = (*supported_apis | LARPSAVER_API_OPENGL);
			plat->wglCreateContext = GetProcAddress(plat->wglLib, "wglCreateContext");
			plat->wglMakeCurrent = GetProcAddress(plat->wglLib, "wglMakeCurrent");
			plat->wglGetProcAddress = GetProcAddress(plat->wglLib, "wglGetProcAddress");
			plat->wglDeleteContext = GetProcAddress(plat->wglLib, "wglDeleteContext");
		}
	}

	return plat;
}

void larpsaver_platform_free(larpsaver_platform* plat) {}
LRESULT WINAPI ScreenSaverProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_NCCREATE:
		return TRUE;
	case WM_NCDESTROY:
		larpsaver_ctx_free(ctx);
		return 0;
	case WM_CREATE:
		if (!ctx) {
			ctx = larpsaver_ctx_new();
		}

		ctx->platform->hwnd = hwnd;

		GetClientRect(hwnd, &ctx->platform->rect);
		ctx->platform->width = ctx->platform->rect.right;
		ctx->platform->height = ctx->platform->rect.bottom;

		if(ctx->supported_apis & LARPSAVER_API_OPENGL) {
			ctx->platform->pfd.nSize = sizeof ctx->platform->pfd;
			ctx->platform->pfd.nVersion = 1;
			ctx->platform->pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
			ctx->platform->pfd.iPixelType = PFD_TYPE_RGBA;
			ctx->platform->pfd.cColorBits = 24;

			ctx->platform->hdc = GetDC(hwnd);

			SetPixelFormat(ctx->platform->hdc, ChoosePixelFormat(ctx->platform->hdc, &ctx->platform->pfd), &ctx->platform->pfd);

			ctx->platform->hrc = ctx->platform->wglCreateContext(ctx->platform->hdc);
			ctx->platform->wglMakeCurrent(ctx->platform->hdc, ctx->platform->hrc);
		}

		GetSystemTime(&ctx->platform->clock1);

		/* this should always be the last function called */
		larpsaver_init(ctx);
		break;
	case WM_PAINT:
	case WM_ERASEBKGND:
		larpsaver_draw(ctx);
		if(ctx->supported_apis & LARPSAVER_API_OPENGL) {
			SwapBuffers(ctx->platform->hdc);
		}
		GetSystemTime(&ctx->platform->clock2);
		if ((ctx->platform->clock2.wMilliseconds - ctx->platform->clock1.wMilliseconds) >= ctx->ms) {
			larpsaver_tick(ctx);
			GetSystemTime(&ctx->platform->clock1);
		}

		return (message == WM_ERASEBKGND);
	case WM_SETCURSOR:
		ShowCursor(0);
		break;
	case WM_ACTIVATE:
		if (wParam != FALSE) {
			break;
		}
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_KEYDOWN:
	case WM_MOUSEMOVE:
		PostQuitMessage(0);
		break;
	case WM_DESTROY:
		if(ctx->supported_apis & LARPSAVER_API_OPENGL) {
			ctx->platform->wglMakeCurrent(NULL, NULL);
			ctx->platform->wglDeleteContext(ctx->platform->hrc);
		}
		ReleaseDC(hwnd, ctx->platform->hdc);
		break;
	default:
		return 0;
	}

	return DefScreenSaverProc(hwnd, message, wParam, lParam);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	/* TODO: how do we do a configure dialog? */
	return FALSE;
}

BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
	return TRUE;
}

void* larpsaver_get_proc_address(larpsaver_ctx* ctx, int api, const char * name) {
	if(ctx->supported_apis & api) {
		switch (api) {
			case LARPSAVER_API_OPENGL: 
				return ctx->platform->wglGetProcAddress(name);
		}
	}
	return NULL;
}

