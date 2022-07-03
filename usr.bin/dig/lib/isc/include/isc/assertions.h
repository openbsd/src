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

/*
 * $Id: assertions.h,v 1.5 2022/07/03 16:00:11 florian Exp $
 */
/*! \file isc/assertions.h
 */

#ifndef ISC_ASSERTIONS_H
#define ISC_ASSERTIONS_H 1

#include <sys/types.h>

/*% isc assertion type */
typedef enum {
	isc_assertiontype_require,
	isc_assertiontype_ensure,
	isc_assertiontype_insist,
	isc_assertiontype_invariant
} isc_assertiontype_t;

typedef void (*isc_assertioncallback_t)(const char *, int, isc_assertiontype_t,
					const char *);

__dead void isc_assertion_failed(const char *, int, isc_assertiontype_t,
			  const char *);

const char *
isc_assertion_typetotext(isc_assertiontype_t type);

#define ISC_REQUIRE(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_require, \
					 #cond), 0)))
#define ISC_ENSURE(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_ensure, \
					 #cond), 0)))
#define ISC_INSIST(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_insist, \
					 #cond), 0)))

#define ISC_INVARIANT(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_invariant, \
					 #cond), 0)))

#endif /* ISC_ASSERTIONS_H */
