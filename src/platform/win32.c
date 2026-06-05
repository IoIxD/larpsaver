#include <larpsaver.h>
#include <stdio.h>
#include <string.h>

/* This is horrible. We should just be using SetWindowLongPtr. Unfortunately
 * uhhh Windows hates you and when we run on Win11 that fails :)  */
static larpsaver_ctx *ctx = NULL;

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

LRESULT ScreenSaverProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

/*
  Register screen saver window class and call user
  supplied hook.
 */
static BOOL RegisterClasses(larpsaver_platform *platform) {
  WNDCLASS cls;

  cls.hCursor = NULL;
  cls.hIcon = LoadIcon(platform->hMainInstance, MAKEINTATOM(ID_APP));
  cls.lpszMenuName = NULL;
  cls.lpszClassName = CLASS_SCRNSAVE;
  cls.hbrBackground = GetStockObject(BLACK_BRUSH);
  cls.hInstance = platform->hMainInstance;
  cls.style = CS_VREDRAW | CS_HREDRAW | CS_SAVEBITS | CS_PARENTDC;
  cls.lpfnWndProc = (WNDPROC)ScreenSaverProc;
  cls.cbWndExtra = 0;
  cls.cbClsExtra = 0;

  if (!RegisterClass(&cls))
    return FALSE;
  return RegisterDialogClasses(platform->hMainInstance);
}

static void LaunchScreenSaver(HWND hParent, larpsaver_ctx *ctx,
                              larpsaver_platform *platform) {
  UINT style;
  RECT rc;
  MSG msg;
  BOOL foo;

  /* don't allow other tasks to get into the foreground */
  if (platform->w95 && !platform->fChildPreview)
    SystemParametersInfo(SPI_SCREENSAVERRUNNING, TRUE, &foo, 0);

  msg.wParam = 0;
  /* register classes, both user defined and classes used by screen saver
     library */
  if (!RegisterClasses(platform)) {
    MessageBox(NULL, TEXT("RegisterClasses() failed"), NULL, MB_ICONHAND);
    return;
  }
  /* a slightly different approach needs to be used when displaying
     in a preview window */
  if (hParent) {
    style = WS_CHILD;
    GetClientRect(hParent, &rc);
  } else {
    style = WS_POPUP | WS_VISIBLE;
    GetClientRect(GetDesktopWindow(), &rc);
  }

  /* create main screen saver window */
  platform->hwnd =
      CreateWindowEx(hParent ? 0 : WS_EX_TOPMOST, CLASS_SCRNSAVE,
                     TEXT("SCREENSAVER"), style, rc.left, rc.top, rc.right,
                     rc.bottom, hParent, NULL, platform->hMainInstance, ctx);

  UpdateWindow(platform->hwnd);
  ShowWindow(platform->hwnd, 1);
}
static void TerminateScreenSaver(HWND hWnd, larpsaver_ctx *ctx) {
  /* don't allow recursion */
  if (ctx->platform->checking_pwd || ctx->platform->closing)
    return;
  /* verify password */
  if (ctx->platform->VerifyScreenSavePwd) {
    ctx->platform->checking_pwd = TRUE;
    ctx->platform->closing = (BOOL)SendMessage(hWnd, SCRM_VERIFYPW, 0, 0);
    ctx->platform->checking_pwd = FALSE;
  } else
    ctx->platform->closing = TRUE;
  /* are we closing? */
  if (ctx->platform->closing) {
    DestroyWindow(hWnd);
  } else
    GetCursorPos(&ctx->platform->pt_orig); /* if not: get new mouse position */
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

  if (!ctx && !(message == WM_NCCREATE || message == WM_CREATE ||
                message == WM_NCCALCSIZE)) {
    return DefWindowProc(hwnd, message, wParam, lParam);
  }

  if (ctx) {
    /* don't do any special processing when in preview mode */
    if (ctx->platform->fChildPreview || ctx->platform->closing)
      return DefWindowProc(hwnd, message, wParam, lParam);
  }

  switch (message) {
  case WM_NCCREATE:
    return TRUE;
  case WM_CREATE:
    ctx = ((LPCREATESTRUCT)lParam)->lpCreateParams;
    SetWindowLongPtr(hwnd, -21, (LONG)ctx);

    ctx->platform->hwnd = hwnd;

    GetClientRect(hwnd, &ctx->platform->rect);
    ctx->platform->width = ctx->platform->rect.right;
    ctx->platform->height = ctx->platform->rect.bottom;

    if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
      printf("setting up opengl\n");
      ctx->platform->pfd.nSize = sizeof ctx->platform->pfd;
      ctx->platform->pfd.nVersion = 1;
      ctx->platform->pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
      ctx->platform->pfd.iPixelType = PFD_TYPE_RGBA;
      ctx->platform->pfd.cColorBits = 32;

      ctx->platform->hdc = GetDC(hwnd);

      SetPixelFormat(ctx->platform->hdc,
                     ChoosePixelFormat(ctx->platform->hdc, &ctx->platform->pfd),
                     &ctx->platform->pfd);

      ctx->platform->hrc = ctx->platform->wglCreateContext(ctx->platform->hdc);
      ctx->platform->wglMakeCurrent(ctx->platform->hdc, ctx->platform->hrc);
    }

    GetSystemTime(&ctx->platform->clock1);
    break;
  case WM_CLOSE:
    TerminateScreenSaver(hwnd, ctx);
    /* do NOT pass this to DefWindowProc; it will terminate even if
       an invalid password was given.
     */
    return 0;
  case SCRM_VERIFYPW:
    /* verify password or return TRUE if password checking is turned off */
    if (ctx->platform->VerifyScreenSavePwd)
      return ctx->platform->VerifyScreenSavePwd(hwnd);
    else
      return TRUE;
  case WM_NCHITTEST:
    return DefWindowProc(hwnd, message, wParam, lParam);
  case WM_SETCURSOR:
    if (ctx->platform->checking_pwd)
      break;
    SetCursor(NULL);
    break;
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
    else
      GetCursorPos(&ctx->platform->pt_orig);
  }
    /* try to terminate screen saver */
    if (!ctx->platform->checking_pwd) {
      PostQuitMessage(0);
      if (ctx->running > 0)
        --ctx->running;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    /* try to terminate screen saver */
    if (!ctx->platform->checking_pwd) {
      PostQuitMessage(0);
      ctx->running = 0;
    }
    return 0;
  case WM_ERASEBKGND: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HBRUSH brush = GetStockObject(BLACK_BRUSH);

    FillRect(hdc, &ps.rcPaint, brush);
    DeleteObject(brush);

    EndPaint(hwnd, &ps);
    return 1;
  }
  case WM_PAINT:
    if (ctx) {
      if (ctx->draw_func) {
        ctx->draw_func(ctx);
      }

      SwapBuffers(ctx->platform->hdc);
    }
    return 0;
  case WM_NCDESTROY:
    return 0;
  case WM_DESTROY:
    if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
      ctx->platform->wglMakeCurrent(NULL, NULL);
      ctx->platform->wglDeleteContext(ctx->platform->hrc);
    }
    ReleaseDC(hwnd, ctx->platform->hdc);
    break;
  default:
    break;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

BOOL ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  /* TODO: how do we do a configure dialog? */
  return FALSE;
}
BOOL RegisterDialogClasses(HANDLE hInst) { return TRUE; }

static void LaunchConfig(larpsaver_ctx *ctx) {
  if (ctx) {
    /* FIXME: should this be called */
    RegisterDialogClasses(ctx->platform->hMainInstance);
    /* display configure dialog */
    DialogBox(ctx->platform->hMainInstance,
              MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE), GetForegroundWindow(),
              (DLGPROC)ScreenSaverConfigureDialog);
  }
}

void larpsaver_platform_init(larpsaver_ctx *_ctx, int argc, char **argv) {
  larpsaver_platform *plat = malloc(sizeof(struct larpsaver_platform_t));
  LPSTR p;
  int i = 0;
  OSVERSIONINFO vi;

  ctx = _ctx;

  if (plat) {
    memset(plat, 0, sizeof(*plat));

    plat->wglLib = LoadLibrary("opengl32.dll");
    if (plat->wglLib) {
      ctx->supported_apis |= LARPSAVER_API_OPENGL;
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

  ctx->platform = plat;

  /* initialize */
  plat->hMainInstance = GetModuleHandle(0);
  vi.dwOSVersionInfoSize = sizeof(vi);
  GetVersionEx(&vi);

  /* on debug builds, start screensaver if no args */
  if (argc <= 1) {
    /* start screen saver */
    LaunchScreenSaver(NULL, ctx, plat);
  } else {
    /* parse arguments */
    for (i = 0; i < argc; i++) {
      p = argv[i];
      if (strcmp(p, "s") == 0) {
        /* start screen saver */
        LaunchScreenSaver(NULL, ctx, plat);
        break;
      }
      if (strcmp(p, "p") == 0) {
        /* start screen saver in preview window */
        HWND hParent;
        plat->fChildPreview = TRUE;
        while (ISSPACE(*++p))
          ;
        hParent = (HWND)(unsigned long long)_toul(p);
        if (hParent && IsWindow(hParent))
          LaunchScreenSaver(hParent, ctx, plat);
        break;
      }
      if (strcmp(p, "c") == 0) {
        /* display configure dialog */
        LaunchConfig(ctx);
        break;
      }
      if (strcmp(p, "a") == 0) {
        HWND hParent;
        /* start screen saver */
        LaunchScreenSaver(NULL, ctx, plat);
        /* change screen saver password */
        while (ISSPACE(*++p))
          ;
        hParent = (HWND)(unsigned long long)_toul(p);
        if (!hParent || !IsWindow(hParent))
          hParent = GetForegroundWindow();
        ScreenSaverChangePassword(hParent);
        break;
      }
    }
  }

  return;
}

void larpsaver_platform_free(larpsaver_platform *plat) {
  BOOL foo;
  /* restore system */
  if (plat->w95 && !plat->fChildPreview)
    SystemParametersInfo(SPI_SCREENSAVERRUNNING, FALSE, &foo, 0);
  FreeLibrary(plat->hPwdLib);
  return;
}

void *larpsaver_get_proc_address(larpsaver_ctx *ctx, int api,
                                 const char *name) {
  if (ctx->supported_apis & api) {
    switch (api) {
    case LARPSAVER_API_OPENGL:
      printf("%s => %p\n", name, ctx->platform->wglGetProcAddress(name));
      return ctx->platform->wglGetProcAddress(name);
    }
  }
  return NULL;
}

void larpsaver_loop(larpsaver_ctx *ctx) {
  MSG msg = {0};

  ctx->running = 5;

  while (ctx->running) {
    if (PeekMessage(&msg, ctx->platform->hwnd, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (!ctx->running) {
        break;
      }
    }
    if (ctx->tick_func) {
      GetSystemTime(&ctx->platform->clock2);
      if ((ctx->platform->clock2.wMilliseconds -
           ctx->platform->clock1.wMilliseconds) >= ctx->ms) {
        ctx->tick_func(ctx);
        GetSystemTime(&ctx->platform->clock1);
      }
    }

    InvalidateRect(ctx->platform->hwnd, NULL, 0);
    UpdateWindow(ctx->platform->hwnd);
  }
}
