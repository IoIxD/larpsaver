#include <larpsaver.h>
#include <string.h>



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
static BOOL RegisterClasses(larpsaver_platform* platform) {
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

static void LaunchScreenSaver(HWND hParent, larpsaver_ctx* ctx, larpsaver_platform* platform) {
  UINT style;
  RECT rc;
  MSG msg;
  BOOL foo;

  printf("LaunchScreenSaver called\n");

  /* don't allow other tasks to get into the foreground */
  if (platform->w95 && !platform->fChildPreview)
    SystemParametersInfo(SPI_SCREENSAVERRUNNING, TRUE, &foo, 0);

  msg.wParam = 0;
  /* register classes, both user defined and classes used by screen saver
     library */
  if (!RegisterClasses(platform)) {
    MessageBox(NULL, TEXT("RegisterClasses() failed"), NULL, MB_ICONHAND);
    return FALSE;
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
  CreateWindowEx(
      hParent ? 0 : WS_EX_TOPMOST, CLASS_SCRNSAVE, TEXT("SCREENSAVER"), style,
      rc.left, rc.top, rc.right, rc.bottom, hParent, NULL,
      platform->hMainInstance, ctx);

  ShowWindow(platform->hwnd, SW_SHOW);
  UpdateWindow(platform->hwnd);
}
static void TerminateScreenSaver(HWND hWnd, larpsaver_ctx* ctx) {
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
    larpsaver_ctx* ctx = GetWindowLongPtr(hwnd, -21);

    if (!ctx && !(message == WM_NCCREATE || message == WM_CREATE || message == WM_NCCALCSIZE)) {
        printf("ctx still null (message %d)...\n",message);
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    //printf("ctx %p\n", ctx);

    if(ctx)
    {
        /* don't do any special processing when in preview mode */
        if (ctx->platform->fChildPreview || ctx->platform->closing)
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_NCCREATE:
        return TRUE;
    case WM_CREATE:
        ctx = ((LPCREATESTRUCT)lParam)->lpCreateParams;
        SetWindowLongPtr(hwnd, -21, ctx);

        printf("WM_CREATE %p\n", ctx);

        if (!ctx->platform->hwnd)
        {
            printf("setting hwnd\n");
            ctx->platform->hwnd = hwnd;
        }

        GetClientRect(hwnd, &ctx->platform->rect);
        ctx->platform->width = ctx->platform->rect.right;
        ctx->platform->height = ctx->platform->rect.bottom;

        if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
            printf("setting up opengl\n");
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
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        /* try to terminate screen saver */
        if (!ctx->platform->checking_pwd)
        {
            PostQuitMessage(0);
            ctx->running = FALSE;
        }
        break;

   
    case WM_PAINT:
    case WM_ERASEBKGND:
            printf("draw\n");
        if (ctx->draw_func) {
            ctx->draw_func(ctx);
        }
        if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
            SwapBuffers(ctx->platform->hdc);
        }
        return (message == WM_ERASEBKGND);
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
        //printf("unhandled msg %04X\n", message);
        //break;
        return 0;
    }


  return DefWindowProc(hwnd, message, wParam, lParam);
}

BOOL ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  /* TODO: how do we do a configure dialog? */
  return FALSE;
}
BOOL RegisterDialogClasses(HANDLE hInst) { return TRUE; }

static void LaunchConfig(larpsaver_ctx* ctx) {
   if (ctx)
    {
            /* FIXME: should this be called */
            RegisterDialogClasses(ctx->platform->hMainInstance);
        /* display configure dialog */
        DialogBox(ctx->platform->hMainInstance,
            MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE), GetForegroundWindow(),
            (DLGPROC)ScreenSaverConfigureDialog);
    }
}

void larpsaver_platform_init(larpsaver_ctx* ctx,  int argc,
                                            char **argv) {
  larpsaver_platform *plat = malloc(sizeof(struct larpsaver_platform_t));
  LPSTR p;
  int i = 0;
  OSVERSIONINFO vi;

  if (plat) {
      memset(plat, 0, sizeof(*plat));

      plat->wglLib = LoadLibrary("opengl32.dll");
      if (plat->wglLib) {
          ctx->supported_apis |= LARPSAVER_API_OPENGL;
          plat->wglCreateContext =
              (void*)GetProcAddress(plat->wglLib, "wglCreateContext");
          plat->wglMakeCurrent =
              (void*)GetProcAddress(plat->wglLib, "wglMakeCurrent");
          plat->wglGetProcAddress =
              (void*)GetProcAddress(plat->wglLib, "wglGetProcAddress");
          plat->wglDeleteContext =
              (void*)GetProcAddress(plat->wglLib, "wglDeleteContext");
      }
  }


  ctx->platform = plat;

  /* initialize */
  plat->hMainInstance = GetModuleHandle(0);
  vi.dwOSVersionInfoSize = sizeof(vi);
  GetVersionEx(&vi);
  printf("%d args\n", argc);

  /* on debug builds, start screensaver if no args */
#ifdef _DEBUG
  if (argc == 1) {
      /* start screen saver */
              /* start screen saver */
      LaunchScreenSaver(NULL,ctx, plat);
  }
  else
#endif
  {
      /* parse arguments */
      for (i = 0; i < argc; i++) {
          p = argv[i];
          printf("arg %d: %s\n", i, p);
          if (strcmp(p, "\\s") == 0) {
              /* start screen saver */
              LaunchScreenSaver(NULL, ctx, plat);
              break;
          }
          if (strcmp(p, "\\p") == 0) {
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
          if (strcmp(p, "\\c") == 0) {
              /* display configure dialog */
              LaunchConfig(ctx);
              break;
          } if (strcmp(p, "\\a") == 0) {
              /* start screen saver */
              LaunchScreenSaver(NULL, ctx, plat);
              /* change screen saver password */
              HWND hParent;
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



  return plat;
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
      return ctx->platform->wglGetProcAddress(name);
    }
  }
  return NULL;
}

void larpsaver_loop(larpsaver_ctx* ctx) {
    MSG msg = { 0 };

    ctx->running = TRUE;
    InvalidateRect(ctx->platform->hwnd, NULL, TRUE);


     while (ctx->running)
    {
                while (PeekMessage(&msg, ctx->platform->hwnd, 0, 0, PM_REMOVE) == TRUE) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                InvalidateRect(ctx->platform->hwnd, NULL, FALSE);
                UpdateWindow(ctx->platform->hwnd);

            
                if (ctx->tick_func) {
                    GetSystemTime(&ctx->platform->clock2);
                    if ((ctx->platform->clock2.wMilliseconds -
                        ctx->platform->clock1.wMilliseconds) >= ctx->ms) {
                        ctx->tick_func(ctx);
                        GetSystemTime(&ctx->platform->clock1);
                    }
                }
    }
}