#include <larpsaver.h>
#include <string.h>

static larpsaver_ctx *ctx = NULL;

/* screen saver window class */

/* function names */

static int ISSPACE(char c) { return (c == ' ' || c == '\t'); }
#define ISNUM(c) ((c) >= '0' && c <= '9')
static unsigned long _toul(const char *s) {
  unsigned long res;
  unsigned long n;
  const char *p;
  for (p = s; *p; p++)
    if (!ISNUM(*p))
      break;
  p--;
  res = 0;
  for (n = 1; p >= s; p--, n *= 10)
    res += (*p - '0') * n;
  return res;
}

/* this function takes care of *must* do tasks, like terminating
   screen saver */
static LRESULT SysScreenSaverProc(HWND hWnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam) {
  switch (msg) {
  case WM_CREATE:
    if (!ctx->platform->fChildPreview)
      SetCursor(NULL);
    /* mouse is not supposed to move from this position */
    GetCursorPos(&ctx->platform->pt_orig);
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  case WM_TIMER:
    if (ctx->platform->closing)
      return 0;
    break;
  case WM_PAINT:
    if (ctx->platform->closing)
      return DefWindowProc(hWnd, msg, wParam, lParam);
    break;
  case WM_SYSCOMMAND:
    if (!ctx->platform->fChildPreview)
      switch (wParam) {
      case SC_CLOSE:
      case SC_SCREENSAVE:
      case SC_NEXTWINDOW:
      case SC_PREVWINDOW:
        return FALSE;
      }
    break;
  case WM_MOUSEMOVE:
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
  case WM_NCACTIVATE:
  case WM_ACTIVATE:
  case WM_ACTIVATEAPP:
    if (ctx->platform->closing)
      return DefWindowProc(hWnd, msg, wParam, lParam);
    break;
  }
  return ScreenSaverProc(hWnd, msg, wParam, lParam);
}
/*
  Register screen saver window class and call user
  supplied hook.
 */
static BOOL RegisterClasses(void) {
  WNDCLASS cls;

  cls.hCursor = NULL;
  cls.hIcon = LoadIcon(ctx->platform->hMainInstance, MAKEINTATOM(ID_APP));
  cls.lpszMenuName = NULL;
  cls.lpszClassName = CLASS_SCRNSAVE;
  cls.hbrBackground = GetStockObject(BLACK_BRUSH);
  cls.hInstance = ctx->platform->hMainInstance;
  cls.style = CS_VREDRAW | CS_HREDRAW | CS_SAVEBITS | CS_PARENTDC;
  cls.lpfnWndProc = (WNDPROC)SysScreenSaverProc;
  cls.cbWndExtra = 0;
  cls.cbClsExtra = 0;

  if (!RegisterClass(&cls))
    return FALSE;
  return RegisterDialogClasses(ctx->platform->hMainInstance);
}

static int LaunchScreenSaver(HWND hParent) {
  BOOL foo;
  UINT style;
  RECT rc;
  MSG msg;
  /* don't allow other tasks to get into the foreground */
  if (ctx->platform->w95 && !ctx->platform->fChildPreview)
    SystemParametersInfo(SPI_SCREENSAVERRUNNING, TRUE, &foo, 0);
  msg.wParam = 0;
  /* register classes, both user defined and classes used by screen saver
     library */
  if (!RegisterClasses()) {
    MessageBox(NULL, TEXT("RegisterClasses() failed"), NULL, MB_ICONHAND);
    goto restore;
  }
  /* a slightly different approach needs to be used when displaying
     in a preview window */
  if (hParent) {
    style = WS_CHILD;
    GetClientRect(hParent, &rc);
  } else {
    style = WS_POPUP;
    rc.left = 0;
    rc.top = 0;
    rc.right = SM_CXSCREEN;
    rc.bottom = SM_CYSCREEN;
    style |= WS_VISIBLE;
  }
  /* create main screen saver window */
  ctx->platform->hMainWindow = CreateWindowEx(
      hParent ? 0 : WS_EX_TOPMOST, CLASS_SCRNSAVE, TEXT("SCREENSAVER"), style,
      rc.left, rc.top, rc.right, rc.bottom, hParent, NULL,
      ctx->platform->hMainInstance, NULL);
  /* display window and start pumping messages */
  if (ctx->platform->hMainWindow) {
    UpdateWindow(ctx->platform->hMainWindow);
    ShowWindow(ctx->platform->hMainWindow, SW_SHOW);
    while (GetMessage(&msg, NULL, 0, 0) == TRUE) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
restore:
  /* restore system */
  if (ctx->platform->w95 && !ctx->platform->fChildPreview)
    SystemParametersInfo(SPI_SCREENSAVERRUNNING, FALSE, &foo, 0);
  FreeLibrary(ctx->platform->hPwdLib);
  return msg.wParam;
}
static void TerminateScreenSaver(HWND hWnd) {
  /* don't allow recursion */
  if (ctx->platform->checking_pwd || ctx->platform->closing)
    return;
  /* verify password */
  if (ctx->platform->VerifyScreenSavePwd) {
    ctx->platform->checking_pwd = TRUE;
    ctx->platform->closing = SendMessage(hWnd, SCRM_VERIFYPW, 0, 0);
    ctx->platform->checking_pwd = FALSE;
  } else
    ctx->platform->closing = TRUE;
  /* are we closing? */
  if (ctx->platform->closing) {
    DestroyWindow(hWnd);
  } else
    GetCursorPos(&ctx->platform->pt_orig); /* if not: get new mouse position */
}

LONG DefScreenSaverProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  /* don't do any special processing when in preview mode */
  if (ctx->platform->fChildPreview || ctx->platform->closing)
    return DefWindowProc(hWnd, msg, wParam, lParam);
  switch (msg) {
  case WM_CLOSE:
    TerminateScreenSaver(hWnd);
    /* do NOT pass this to DefWindowProc; it will terminate even if
       an invalid password was given.
     */
    return 0;
  case SCRM_VERIFYPW:
    /* verify password or return TRUE if password checking is turned off */
    if (ctx->platform->VerifyScreenSavePwd)
      return ctx->platform->VerifyScreenSavePwd(hWnd);
    else
      return TRUE;
  case WM_SETCURSOR:
    if (ctx->platform->checking_pwd)
      break;
    SetCursor(NULL);
    return TRUE;
  case WM_NCACTIVATE:
  case WM_ACTIVATE:
  case WM_ACTIVATEAPP:
    if (wParam != FALSE)
      break;
  case WM_MOUSEMOVE: {
    POINT pt;
    GetCursorPos(&pt);
    if (pt.x == ctx->platform->pt_orig.x && pt.y == ctx->platform->pt_orig.y)
      break;
  }
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    /* try to terminate screen saver */
    if (!ctx->platform->checking_pwd)
      PostMessage(hWnd, WM_CLOSE, 0, 0);
    break;
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ScreenSaverChangePassword(HWND hParent) {
  /* load Master Password Router (MPR) */
  HINSTANCE hMpr = LoadLibrary(TEXT("MPR.DLL"));

  if (hMpr) {
    CHPWDPROC ChangePassword;
    ChangePassword = (CHPWDPROC)GetProcAddress(hMpr, szPwdChangePassword);
    /* change password for screen saver provider */
    if (ChangePassword)
      ChangePassword(TEXT("SCRSAVE"), hParent, 0, NULL);
    FreeLibrary(hMpr);
  }
}

LRESULT ScreenSaverProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_NCCREATE:
    return TRUE;
  case WM_NCDESTROY:
    larpsaver_ctx_free(ctx);
    return 0;
  case WM_CREATE:
    ctx->platform->hwnd = hwnd;

    GetClientRect(hwnd, &ctx->platform->rect);
    ctx->platform->width = ctx->platform->rect.right;
    ctx->platform->height = ctx->platform->rect.bottom;

    if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
      ctx->platform->pfd.nSize = sizeof ctx->platform->pfd;
      ctx->platform->pfd.nVersion = 1;
      ctx->platform->pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
      ctx->platform->pfd.iPixelType = PFD_TYPE_RGBA;
      ctx->platform->pfd.cColorBits = 24;

      ctx->platform->hdc = GetDC(hwnd);

      SetPixelFormat(ctx->platform->hdc,
                     ChoosePixelFormat(ctx->platform->hdc, &ctx->platform->pfd),
                     &ctx->platform->pfd);

      ctx->platform->hrc = ctx->platform->wglCreateContext(ctx->platform->hdc);
      ctx->platform->wglMakeCurrent(ctx->platform->hdc, ctx->platform->hrc);
    }

    GetSystemTime(&ctx->platform->clock1);
    break;
  case WM_PAINT:
  case WM_ERASEBKGND:
    if (ctx->draw_func) {
      ctx->draw_func(ctx);
      if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
        SwapBuffers(ctx->platform->hdc);
      }
      if (ctx->tick_func) {
        GetSystemTime(&ctx->platform->clock2);
        if ((ctx->platform->clock2.wMilliseconds -
             ctx->platform->clock1.wMilliseconds) >= ctx->ms) {
          ctx->tick_func(ctx);
          GetSystemTime(&ctx->platform->clock1);
        }
      }
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
    if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
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

BOOL ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  /* TODO: how do we do a configure dialog? */
  return FALSE;
}
BOOL RegisterDialogClasses(HANDLE hInst) { return TRUE; }

static void LaunchConfig(void) {
  /* FIXME: should this be called */
  RegisterDialogClasses(ctx->platform->hMainInstance);
  /* display configure dialog */
  DialogBox(ctx->platform->hMainInstance,
            MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE), GetForegroundWindow(),
            (DLGPROC)ScreenSaverConfigureDialog);
}

larpsaver_platform *larpsaver_platform_init(int *supported_apis, int argc,
                                            char **argv) {
  larpsaver_platform *plat = malloc(sizeof(struct larpsaver_platform_t));
  LPSTR p;
  OSVERSIONINFO vi;
  /* initialize */
  ctx->platform->hMainInstance = GetModuleHandle(0);
  vi.dwOSVersionInfoSize = sizeof(vi);
  GetVersionEx(&vi);

  /* parse arguments */
  for (p = *argv; *p; p++) {
    switch (*p) {
    case 'S':
    case 's':
      /* start screen saver */
      LaunchScreenSaver(NULL);

    case 'P':
    case 'p': {
      /* start screen saver in preview window */
      HWND hParent;
      ctx->platform->fChildPreview = TRUE;
      while (ISSPACE(*++p))
        ;
      hParent = (HWND)(unsigned long long)_toul(p);
      if (hParent && IsWindow(hParent))
        LaunchScreenSaver(hParent);
    }
      return 0;
    case 'C':
    case 'c':
      /* display configure dialog */
      LaunchConfig();
      return 0;
    case 'A':
    case 'a': {
      /* change screen saver password */
      HWND hParent;
      while (ISSPACE(*++p))
        ;
      hParent = (HWND)(unsigned long long)_toul(p);
      if (!hParent || !IsWindow(hParent))
        hParent = GetForegroundWindow();
      ScreenSaverChangePassword(hParent);
    }
      return 0;
    case '-':
    case '/':
    case ' ':
    default:
      break;
    }
  }
  LaunchConfig();
  return 0;

  if (plat) {
    memset(plat, 0, sizeof(*plat));
  }

  if (plat) {
    plat->wglLib = LoadLibrary("opengl32.dll");
    if (plat->wglLib) {
      *supported_apis = (*supported_apis | LARPSAVER_API_OPENGL);
      plat->wglCreateContext =
          (void *)GetProcAddress(plat->wglLib, "wglCreateContext");
      plat->wglMakeCurrent =
          (void *)GetProcAddress(plat->wglLib, "wglMakeCurrent");
      plat->wglGetProcAddress =
          (void *)GetProcAddress(plat->wglLib, "wglGetProcAddress");
      plat->wglDeleteContext =
          (void *)GetProcAddress(plat->wglLib, "wglDeleteContext");
    }
  }

  return plat;
}

void larpsaver_platform_free(larpsaver_platform *plat) {}

void *larpsaver_get_proc_address(larpsaver_ctx *ctx, int api,
                                 const char *name) {
  if (ctx->supported_apis & api) {
    switch (api) {
    case LARPSAVER_API_OPENGL:
      return ctx->platform->wglGetProcAddress(name);
    }
  }
  return NULL;
}
