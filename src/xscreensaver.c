#include <assert.h>
#include <dlfcn.h>
#include <larpsaver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static EGLint egl_attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                             EGL_RED_SIZE,     8,
                             EGL_GREEN_SIZE,   8,
                             EGL_BLUE_SIZE,    8,
                             EGL_NONE};
static EGLint egl_context_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

void larpsaver_platform_init(larpsaver_ctx *ctx) {
  struct larpsaver_platform_t *plat =
      malloc(sizeof(struct larpsaver_platform_t));
  memset(plat, 0, sizeof(*plat));

  if (plat) {
    EGLint num_config;
    EGLint major, minor;

    plat->display = XOpenDisplay(NULL);
    if (getenv("WAYLAND_DISPLAY")) {
      plat->window =
          XCreateSimpleWindow(plat->display, DefaultRootWindow(plat->display),
                              0, 0, 800, 600, 0, 0, 0x0);
    } else {
      plat->window = XDefaultRootWindow(plat->display);
    }

    /* opengl setup */
    plat->eglLib = dlopen("libEGL.so", RTLD_LAZY | RTLD_GLOBAL);
    if (plat->eglLib) {
      ctx->supported_apis |= LARPSAVER_API_OPENGL;

      plat->eglGetDisplay = dlsym(plat->eglLib, "eglGetDisplay");
      plat->eglInitialize = dlsym(plat->eglLib, "eglInitialize");
      plat->eglCreateWindowSurface =
          dlsym(plat->eglLib, "eglCreateWindowSurface");
      plat->eglCreateContext = dlsym(plat->eglLib, "eglCreateContext");
      plat->eglGetError = dlsym(plat->eglLib, "eglGetError");
      plat->eglChooseConfig = dlsym(plat->eglLib, "eglChooseConfig");
      plat->eglMakeCurrent = dlsym(plat->eglLib, "eglMakeCurrent");
      plat->eglSwapBuffers = dlsym(plat->eglLib, "eglSwapBuffers");
      plat->eglGetProcAddress = dlsym(plat->eglLib, "eglGetProcAddress");

      plat->egl_display =
          plat->eglGetDisplay((EGLNativeDisplayType)plat->display);
      if (plat->egl_display == EGL_NO_DISPLAY) {
        printf("Error getting EGL display\n");
        return;
      }

      if (!plat->eglInitialize(plat->egl_display, &major, &minor)) {
        printf("Error initializing EGL\n");
        return;
      }

      printf("loaded EGL v%d.%d\n", major, minor);

      if (!plat->eglChooseConfig(plat->egl_display, egl_attrs, &plat->egl_conf,
                                 1, &num_config)) {
        printf("Failed to choose config (eglError: %x)\n", plat->eglGetError());
        return;
      }

      if (num_config != 1) {
        printf("zero config\n");
        return;
      }

      plat->egl_surface = plat->eglCreateWindowSurface(
          plat->egl_display, plat->egl_conf, plat->window, NULL);
      if (plat->egl_surface == EGL_NO_SURFACE) {
        printf("CreateWindowSurface, EGL eglError: %d\n", plat->eglGetError());
        return;
      }

      plat->egl_context = plat->eglCreateContext(
          plat->egl_display, plat->egl_conf, EGL_NO_CONTEXT, egl_context_attrs);
      if (plat->egl_context == EGL_NO_CONTEXT) {
        printf("CreateContext, EGL eglError: %d\n", plat->eglGetError());
        return;
      }
    }

    XMapWindow(plat->display, plat->window);
  }

  ctx->platform = plat;
};

void larpsaver_platform_free(larpsaver_ctx *ctx) {
  XCloseDisplay(ctx->platform->display);
};

void larpsaver_platform_loop(larpsaver_ctx *ctx) {
  int running = 1;
  XEvent ev = {0};
  time_t t;
  struct timespec t1, t2;

  clock_gettime(CLOCK_MONOTONIC, &t1);

  while (running) {
    double t1_ms, t2_m2;
    clock_gettime(CLOCK_MONOTONIC, &t2);
    t1_ms = ((double)t1.tv_nsec) * 1e-6 + (t1.tv_sec * 1000);
    t2_m2 = ((double)t2.tv_nsec) * 1e-6 + (t2.tv_sec * 1000);

    if ((t2_m2 - t1_ms) >= ctx->ms) {
      if (ctx->tick_func) {
        ctx->tick_func(ctx);
      }
      clock_gettime(CLOCK_MONOTONIC, &t1);
    }

    if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
      ctx->platform->eglMakeCurrent(
          ctx->platform->egl_display, ctx->platform->egl_surface,
          ctx->platform->egl_surface, ctx->platform->egl_context);
    }

    if (ctx->draw_func) {
      ctx->draw_func(ctx);
    }

    if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
      ctx->platform->eglSwapBuffers(ctx->platform->egl_display,
                                    ctx->platform->egl_surface);
    }
  }
};

void *larpsaver_get_proc_address(larpsaver_ctx *ctx, int api,
                                 const char *name) {
  if (api & LARPSAVER_API_OPENGL) {
    void *p = ctx->platform->eglGetProcAddress(name);
    // printf("%p\n", p);
    return p;
  }
  return NULL;
}

int main(int argc, char **argv) {
  larpsaver_ctx *ctx = malloc(sizeof(struct larpsaver_ctx_t));

  if (ctx) {
    memset(ctx, 0, sizeof(*ctx));
    larpsaver_platform_init(ctx);

    larpsaver_entry(ctx);

    larpsaver_platform_loop(ctx);

    larpsaver_platform_free(ctx);
    free(ctx);
  } else {
    printf("Unable to display screensaver; Out of Memory");
  }

  return 0;
}
