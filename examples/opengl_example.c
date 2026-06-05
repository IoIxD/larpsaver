#include <larpsaver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

/* currently broken */
#define DYNLOAD 0

#if DYNLOAD
#include "glad/glad.h"
#else

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

typedef struct my_userdata_t {
#if DYNLOAD
  PFNGLCLEARCOLORPROC glClearColor;
  PFNGLCLEARPROC glClear;
  PFNGLFLUSHPROC glFlush;
#endif
  double r;
  double g;
  double b;
  int x;
} my_userdata;

static void draw(larpsaver_ctx *ctx) {
  my_userdata *userdata = ctx->userdata;

#if DYNLOAD
  if (userdata) {
    if (userdata->glClearColor)
      userdata->glClearColor((GLfloat)userdata->r, (GLfloat)userdata->g,
                             (GLfloat)userdata->b, 1);
    if (userdata->glClear)
      userdata->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (userdata->glFlush)
      userdata->glFlush();
  }
#else
  glClearColor((GLfloat)userdata->r, (GLfloat)userdata->g, (GLfloat)userdata->b,
               1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glFlush();
#endif
}

static void tick(larpsaver_ctx *ctx) {
  my_userdata *userdata = ctx->userdata;

  ++userdata->x;

  userdata->r = sin(userdata->x);
  userdata->g = cos(userdata->x);
  userdata->b = sin(userdata->x) * cos(userdata->x);
}

#ifdef _WIN32
PCHAR *CommandLineToArgvA(PCHAR CmdLine, int *_argc) {
  PCHAR *argv;
  PCHAR _argv;
  ULONG len;
  ULONG argc;
  CHAR a;
  ULONG i, j;

  BOOLEAN in_QM;
  BOOLEAN in_TEXT;
  BOOLEAN in_SPACE;

  len = strlen(CmdLine);
  i = ((len + 2) / 2) * sizeof(PVOID) + sizeof(PVOID);

  argv = (PCHAR *)GlobalAlloc(GMEM_FIXED, i + (len + 2) * sizeof(CHAR));

  _argv = (PCHAR)(((PUCHAR)argv) + i);

  argc = 0;
  argv[argc] = _argv;
  in_QM = FALSE;
  in_TEXT = FALSE;
  in_SPACE = TRUE;
  i = 0;
  j = 0;

  while ((a = CmdLine[i])) {
    if (in_QM) {
      if (a == '\"') {
        in_QM = FALSE;
      } else {
        _argv[j] = a;
        j++;
      }
    } else {
      switch (a) {
      case '\"':
        in_QM = TRUE;
        in_TEXT = TRUE;
        if (in_SPACE) {
          argv[argc] = _argv + j;
          argc++;
        }
        in_SPACE = FALSE;
        break;
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if (in_TEXT) {
          _argv[j] = '\0';
          j++;
        }
        in_TEXT = FALSE;
        in_SPACE = TRUE;
        break;
      default:
        in_TEXT = TRUE;
        if (in_SPACE) {
          argv[argc] = _argv + j;
          argc++;
        }
        _argv[j] = a;
        j++;
        in_SPACE = FALSE;
        break;
      }
    }
    i++;
  }
  _argv[j] = '\0';
  argv[argc] = NULL;

  (*_argc) = argc;
  return argv;
}
#endif

#ifdef _WIN32
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
            int nShowCmd) {
#else
int main(int argc, char **argv) {
#endif
  larpsaver_ctx *ctx = larpsaver_ctx_new();
  my_userdata *userdata = malloc(sizeof(my_userdata));
  if (!userdata) {
    printf("userdata was null, oom\n");
    exit(0);
  }
  memset(userdata, 0, sizeof(my_userdata));

  ctx->ms = 250;

  if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
#if DYNLOAD
    userdata->glClearColor =
        larpsaver_get_proc_address(ctx, LARPSAVER_API_OPENGL, "glClearColor");
    userdata->glClear =
        larpsaver_get_proc_address(ctx, LARPSAVER_API_OPENGL, "glClear");
    userdata->glFlush =
        larpsaver_get_proc_address(ctx, LARPSAVER_API_OPENGL, "glFlush");
#endif
  } else {
    /* requires opengl */
    exit(0);
  }

  ctx->userdata = userdata;

  ctx->draw_func = draw;
  ctx->tick_func = tick;

  larpsaver_loop(ctx);

  return 0;
}
