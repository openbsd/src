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

/* $Id: bind9.h,v 1.2 2019/12/17 01:46:35 sthen Exp $ */

#ifndef ISC_BIND9_H
#define ISC_BIND9_H 1

#include <isc/boolean.h>
#include <isc/platform.h>

/*
 * This determines whether we are using the libisc/libdns libraries
 * in BIND9 or in some other application.  For BIND9 (named and related
 * tools) it must be set to ISC_TRUE at runtime.  Export library clients
 * will call isc_lib_register(), which will set it to ISC_FALSE.
 */
LIBISC_EXTERNAL_DATA extern isc_boolean_t isc_bind9;

#endif /* ISC_BIND9_H */
