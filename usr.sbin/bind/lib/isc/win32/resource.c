/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: resource.c,v 1.2 2001/07/06 19:40:33 gson Exp $ */

#include <config.h>

#include <isc/platform.h>
#include <isc/resource.h>
#include <isc/result.h>
#include <isc/util.h>

#include "errno2result.h"

isc_result_t
isc_resource_setlimit(isc_resource_t resource, isc_resourcevalue_t value) {
	return (ISC_R_NOTIMPLEMENTED);

}

isc_result_t
isc_resource_getlimit(isc_resource_t resource, isc_resourcevalue_t *value) {
	return (ISC_R_NOTIMPLEMENTED);
}
