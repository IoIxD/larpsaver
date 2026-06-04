#define _LARPSAVER
#include "larpsaver.h"
#include <stdlib.h>

larpsaver_ctx* larpsaver_ctx_new() {
	larpsaver_ctx *ctx = malloc(sizeof(struct larpsaver_ctx_t));

	if(ctx) {
		memset(ctx, 0, sizeof(*ctx));
		ctx->platform = larpsaver_platform_init(&ctx->supported_apis);
	}
	
	return ctx;
}

void larpsaver_ctx_free(larpsaver_ctx* ctx) {
	larpsaver_platform_free(ctx->platform);
	free(ctx);
}
