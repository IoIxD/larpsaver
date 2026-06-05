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

int main(int argc, char **argv) {
  larpsaver_ctx *ctx = larpsaver_ctx_new(argc, argv);
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
