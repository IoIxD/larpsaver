#include "larpsaver.h"
#include <stdlib.h>
#include <string.h>

larpsaver_ctx *larpsaver_ctx_new(int argc, char **argv) {
  larpsaver_ctx *ctx = malloc(sizeof(struct larpsaver_ctx_t));

  if (ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->platform = larpsaver_platform_init(&ctx->supported_apis, argc, argv);
  }

  return ctx;
}

void larpsaver_ctx_free(larpsaver_ctx *ctx) {
  larpsaver_platform_free(ctx->platform);
  free(ctx);
}
