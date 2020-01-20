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

/* $Id: lib.c,v 1.7 2020/01/20 18:51:53 florian Exp $ */

/*! \file */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <isc/app.h>
#include <isc/lib.h>

#include <isc/msgs.h>
#include <isc/once.h>

#include <isc/socket.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

/***
 *** Globals
 ***/

/***
 *** Private
 ***/

/***
 *** Functions
 ***/

static isc_once_t		register_once = ISC_ONCE_INIT;

static void
do_register(void) {
	isc_bind9 = ISC_FALSE;

	RUNTIME_CHECK(isc__app_register() == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc__task_register() == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc__socket_register() == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc__timer_register() == ISC_R_SUCCESS);
}

void
isc_lib_register(void) {
	RUNTIME_CHECK(isc_once_do(&register_once, do_register)
		      == ISC_R_SUCCESS);
}
