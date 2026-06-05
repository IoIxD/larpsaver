#ifndef __LARPSAVER_H
#define __LARPSAVER_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct larpsaver_platform_t larpsaver_platform;

typedef struct larpsaver_ctx_t {
  /* Internal platform data. Define _LARPSAVER before including larpsaver.h to
   * access*/
  larpsaver_platform *platform;
  /* Userdata */
  void *userdata;
  /* Interval at which larpsaver_tick should be called. Defaults to zero.*/
  unsigned long long ms;
  /* Supported APIs. See larpsaver_api */
  int supported_apis;

  /* tick function, called every (ctx->ms) ms */
  void (*tick_func)(struct larpsaver_ctx_t *ctx);
  /* draw function, called at the monitor's refresh rate */
  void (*draw_func)(struct larpsaver_ctx_t *ctx);

  /* Whether or not the screensaver is running*/
  int running;
} larpsaver_ctx;

larpsaver_ctx *larpsaver_ctx_new(int argc, char **argv);
void larpsaver_ctx_free(larpsaver_ctx *ctx);
void larpsaver_loop(larpsaver_ctx *ctx);

/* Supported APIs that are found on the host machine at runtime. */
typedef enum larpsaver_api_t {
  LARPSAVER_API_OPENGL = (1 << 0),
} larpsaver_api;

/*
        Get address of passed function.
        api should be the API that you want to search in, see larpsaver_api.
        Returns NULL if the api isn't supported or the function wasn't found.
*/
void *larpsaver_get_proc_address(larpsaver_ctx *ctx, int api, const char *name);

#ifdef _LARPSAVER
void larpsaver_platform_init(larpsaver_ctx *ctx, int argc, char **argv);
void larpsaver_platform_free(larpsaver_platform *plat);

#ifdef _WIN32
#include <windows.h>

#include <regstr.h>

#define CLASS_SCRNSAVE TEXT("WindowsScreenSaverClass")

#define IDS_DESCRIPTION 1

#define ID_APP 100
#define DLG_SCRNSAVECONFIGURE 2003
#define WS_GT (WS_GROUP | WS_TABSTOP)

#define MAXFILELEN 13
#define TITLEBARNAMELEN 40
#define APPNAMEBUFFERLEN 40
#define BUFFLEN 255

#define idsIsPassword 1000
#define idsIniFile 1001
#define idsScreenSaver 1002
#define idsPassword 1003
#define idsDifferentPW 1004
#define idsChangePW 1005
#define idsBadOldPW 1006
#define idsAppName 1007
#define idsNoHelpMemory 1008
#define idsHelpFile 1009
#define idsDefKeyword 1010

#define szVerifyPassword "VerifyScreenSavePwd"
#ifdef UNICODE
#define szPwdChangePassword "PwdChangePasswordW"
#else
#define szPwdChangePassword "PwdChangePasswordA"
#endif

typedef BOOL(WINAPI *VERIFYPWDPROC)(HWND);
typedef DWORD(WINAPI *CHPWDPROC)(LPCTSTR, HWND, DWORD, PVOID);

struct larpsaver_platform_t {
  HDC hdc;
  HGLRC hrc;
  RECT rect;
  int width;
  int height;
  PIXELFORMATDESCRIPTOR pfd;
  SYSTEMTIME clock1;
  SYSTEMTIME clock2;

  HWND hwnd;
  BOOL fChildPreview;
  HINSTANCE hMainInstance;
  TCHAR szName[TITLEBARNAMELEN];
  TCHAR szAppName[APPNAMEBUFFERLEN];
  TCHAR szIniFile[MAXFILELEN];
  TCHAR szScreenSaver[22];
  TCHAR szHelpFile[MAXFILELEN];
  TCHAR szNoHelpMemory[BUFFLEN];
  UINT MyHelpMessage;
  HINSTANCE hPwdLib;
  POINT pt_orig;
  BOOL checking_pwd;
  BOOL closing;
  BOOL w95;
  VERIFYPWDPROC VerifyScreenSavePwd;

  HMODULE wglLib;
  HGLRC (*wglCreateContext)(HDC);
  void (*wglMakeCurrent)(HDC, HGLRC);
  void *(*wglGetProcAddress)(LPCSTR);
  void (*wglDeleteContext)(HGLRC);
};

LRESULT ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam,
                                LPARAM lParam);
BOOL RegisterDialogClasses(HANDLE hInst);

#define SCRM_VERIFYPW WM_APP

void ScreenSaverChangePassword(HWND hParent);
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
