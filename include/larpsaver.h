#ifndef __LARPSAVER_H
#define __LARPSAVER_H

typedef struct larpsaver_platform_t larpsaver_platform;

typedef struct larpsaver_ctx_t {
	/* Internal platform data. Define _LARPSAVER before including larpsaver.h to access*/
	larpsaver_platform* platform;
	/* Userdata */
	void* userdata;
	/* Interval at which larpsaver_tick should be called. Defaults to zero.*/
	unsigned long long ms;
	/* Supported APIs. See larpsaver_api */
	int supported_apis;
} larpsaver_ctx;

/* larpsaver entry point */
extern void larpsaver_init(larpsaver_ctx* ctx);
/* larpsaver tick function, called every (ctx->ms) ms */
extern void larpsaver_tick(larpsaver_ctx* ctx);
/* larpsaver draw function, called at the monitor's refresh rate */
extern void larpsaver_draw(larpsaver_ctx* ctx);

/* Supported APIs that are found on the host machine at runtime. */
typedef enum larpsaver_api_t {
	LARPSAVER_API_OPENGL = (1 << 0),
} larpsaver_api;

/* 
	Get address of passed function.
	api should be the API that you want to search in, see larpsaver_api.
	Returns NULL if the api isn't supported or the function wasn't found.
*/
void *larpsaver_get_proc_address(larpsaver_ctx* ctx, int api, const char * name);

#ifdef _LARPSAVER
larpsaver_ctx* larpsaver_ctx_new(void);
void larpsaver_ctx_free(larpsaver_ctx* ctx);
larpsaver_platform* larpsaver_platform_init(int * supported_apis);
void larpsaver_platform_free(larpsaver_platform* plat);
#endif

#ifdef _WIN32
#include <windows.h>
struct larpsaver_platform_t {
	HWND hwnd;
	HDC hdc;
	HGLRC hrc;
	RECT rect;
	int width;
	int height;
	PIXELFORMATDESCRIPTOR pfd;
	SYSTEMTIME clock1;
	SYSTEMTIME clock2;

	HMODULE wglLib;
	HGLRC (*wglCreateContext)(HDC);
	void (*wglMakeCurrent)(HDC, HGLRC);
	void *(*wglGetProcAddress)(LPCSTR);
	void (*wglDeleteContext)(HGLRC);
};
#endif

#endif