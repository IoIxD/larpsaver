#include <larpsaver.h>
#include <stdio.h>
#include <stdlib.h>

#include "glad/glad.h"
#include <math.h>

typedef struct my_userdata_t {
	PFNGLCLEARCOLORPROC glClearColor;
	PFNGLCLEARPROC glClear;
	double r;
	double g;
	double b;
	int x;
} my_userdata;

void larpsaver_init(larpsaver_ctx* ctx) {
	my_userdata* userdata = malloc(sizeof(my_userdata));
	if (!userdata) {
		printf("userdata was null, oom\n");
		exit(0);
	}
	memset(userdata, 0, sizeof(my_userdata));

	ctx->ms = 250;

	if (ctx->supported_apis & LARPSAVER_API_OPENGL) {
		userdata->glClearColor = larpsaver_get_proc_address(ctx, LARPSAVER_API_OPENGL, "glClearColor");
		userdata->glClear = larpsaver_get_proc_address(ctx, LARPSAVER_API_OPENGL, "glClear");
	}
	else {
		/* requires opengl */
		exit(0);
	}

	ctx->userdata = userdata;
}

void larpsaver_draw(larpsaver_ctx* ctx) {
	my_userdata* userdata = ctx->userdata;

	userdata->glClearColor((GLfloat)userdata->r, (GLfloat)userdata->g, (GLfloat)userdata->b, 1);
	userdata->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void larpsaver_tick(larpsaver_ctx* ctx) {
	my_userdata* userdata = ctx->userdata;

	++userdata->x;

	userdata->r = sin(userdata->x);
	userdata->g = cos(userdata->x);
	userdata->b = sin(userdata->x) * cos(userdata->x);
}