/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: app_api.c,v 1.6 2020/01/22 13:02:09 florian Exp $ */



#include <unistd.h>

#include <isc/app.h>
#include <isc/magic.h>
#include <isc/util.h>

static isc_appctxcreatefunc_t appctx_createfunc = NULL;
static isc_boolean_t is_running = ISC_FALSE;

#define ISCAPI_APPMETHODS_VALID(m) ISC_MAGIC_VALID(m, ISCAPI_APPMETHODS_MAGIC)

isc_result_t
isc_app_register(isc_appctxcreatefunc_t createfunc) {
	isc_result_t result = ISC_R_SUCCESS;

	if (appctx_createfunc == NULL)
		appctx_createfunc = createfunc;
	else
		result = ISC_R_EXISTS;

	return (result);
}

isc_result_t
isc_appctx_create(isc_appctx_t **ctxp) {
	return (isc__appctx_create(ctxp));
}

void
isc_appctx_destroy(isc_appctx_t **ctxp) {
	REQUIRE(ctxp != NULL && ISCAPI_APPCTX_VALID(*ctxp));

	isc__appctx_destroy(ctxp);

	ENSURE(*ctxp == NULL);
}

isc_result_t
isc_app_ctxstart(isc_appctx_t *ctx) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));

	return (isc__app_ctxstart(ctx));
}

isc_result_t
isc_app_ctxrun(isc_appctx_t *ctx) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));

	return (isc__app_ctxrun(ctx));
}

isc_result_t
isc_app_ctxonrun(isc_appctx_t *ctx,
		 isc_task_t *task, isc_taskaction_t action,
		 void *arg)
{
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));

	return (isc__app_ctxonrun(ctx, task, action, arg));
}

isc_result_t
isc_app_ctxsuspend(isc_appctx_t *ctx) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));

	return (isc__app_ctxsuspend(ctx));
}

isc_result_t
isc_app_ctxshutdown(isc_appctx_t *ctx) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));

	return (isc__app_ctxshutdown(ctx));
}

void
isc_app_ctxfinish(isc_appctx_t *ctx) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));

	isc__app_ctxfinish(ctx);

	ctx->methods->ctxfinish(ctx);
}

void
isc_appctx_settaskmgr(isc_appctx_t *ctx, isc_taskmgr_t *taskmgr) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));
	REQUIRE(taskmgr != NULL);

	isc__appctx_settaskmgr(ctx, taskmgr);

	ctx->methods->settaskmgr(ctx, taskmgr);
}

void
isc_appctx_setsocketmgr(isc_appctx_t *ctx, isc_socketmgr_t *socketmgr) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));
	REQUIRE(socketmgr != NULL);

	isc__appctx_setsocketmgr(ctx, socketmgr);

	ctx->methods->setsocketmgr(ctx, socketmgr);
}

void
isc_appctx_settimermgr(isc_appctx_t *ctx, isc_timermgr_t *timermgr) {
	REQUIRE(ISCAPI_APPCTX_VALID(ctx));
	REQUIRE(timermgr != NULL);

	isc__appctx_settimermgr(ctx, timermgr);

	ctx->methods->settimermgr(ctx, timermgr);
}

isc_result_t
isc_app_start(void) {
	return (isc__app_start());
}

isc_result_t
isc_app_onrun(isc_task_t *task,
	       isc_taskaction_t action, void *arg)
{
	return (isc__app_onrun(task, action, arg));
}

isc_result_t
isc_app_run() {
	isc_result_t result;

	is_running = ISC_TRUE;
	result = isc__app_run();
	is_running = ISC_FALSE;
	return (result);
}

isc_boolean_t
isc_app_isrunning() {
	return (is_running);
}

isc_result_t
isc_app_shutdown(void) {
	return (isc__app_shutdown());
}

isc_result_t
isc_app_reload(void) {
	return (isc__app_reload());
}

void
isc_app_finish(void) {
	isc__app_finish();
}

void
isc_app_block(void) {
	isc__app_block();
}

void
isc_app_unblock(void) {
	isc__app_unblock();
}
