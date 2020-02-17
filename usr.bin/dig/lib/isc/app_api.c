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

/* $Id: app_api.c,v 1.2 2020/02/17 18:58:39 jung Exp $ */



#include <unistd.h>

#include <isc/app.h>
#include <isc/magic.h>
#include <isc/util.h>

static isc_appctxcreatefunc_t appctx_createfunc = NULL;
static isc_boolean_t is_running = ISC_FALSE;

#define ISCAPI_APPMETHODS_VALID(m) ISC_MAGIC_VALID(m, ISCAPI_APPMETHODS_MAGIC)

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
