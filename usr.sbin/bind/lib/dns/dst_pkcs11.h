/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef DST_PKCS11_H
#define DST_PKCS11_H 1

#include <isc/lang.h>
#include <isc/log.h>
#include <isc/result.h>

ISC_LANG_BEGINDECLS

isc_result_t
dst__pkcs11_toresult(const char *funcname, const char *file, int line,
		     isc_result_t fallback, CK_RV rv);

#define PK11_CALL(func, args, fallback) \
	((void) (((rv = (func) args) == CKR_OK) || \
		 ((ret = dst__pkcs11_toresult(#func, __FILE__, __LINE__, \
					      fallback, rv)), 0)))

#define PK11_RET(func, args, fallback) \
	((void) (((rv = (func) args) == CKR_OK) || \
		 ((ret = dst__pkcs11_toresult(#func, __FILE__, __LINE__, \
					      fallback, rv)), 0)));	\
	if (rv != CKR_OK) goto err;

ISC_LANG_ENDDECLS

#endif /* DST_PKCS11_H */
