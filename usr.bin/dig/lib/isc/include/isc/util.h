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

/* $Id: util.h,v 1.3 2020/02/25 05:00:43 jsg Exp $ */

#ifndef ISC_UTIL_H
#define ISC_UTIL_H 1

/*! \file isc/util.h
 * NOTE:
 *
 * This file is not to be included from any <isc/???.h> (or other) library
 * files.
 *
 * \brief
 * Including this file puts several macros in your name space that are
 * not protected (as all the other ISC functions/macros do) by prepending
 * ISC_ or isc_ to the name.
 */

/***
 *** General Macros.
 ***/

/*%
 * Use this to hide unused function arguments.
 * \code
 * int
 * foo(char *bar)
 * {
 *	UNUSED(bar);
 * }
 * \endcode
 */
#define UNUSED(x)      (void)(x)

/*%
 * The opposite: silent warnings about stored values which are never read.
 */
#define POST(x)        (void)(x)

#define ISC_MAX(a, b)  ((a) > (b) ? (a) : (b))
#define ISC_MIN(a, b)  ((a) < (b) ? (a) : (b))

#define ISC_CLAMP(v, x, y) ((v) < (x) ? (x) : ((v) > (y) ? (y) : (v)))

/*%
 * Use this to remove the const qualifier of a variable to assign it to
 * a non-const variable or pass it as a non-const function argument ...
 * but only when you are sure it won't then be changed!
 * This is necessary to sometimes shut up some compilers
 * (as with gcc -Wcast-qual) when there is just no other good way to avoid the
 * situation.
 */
#define DE_CONST(konst, var) \
	do { \
		union { const void *k; void *v; } _u; \
		_u.k = konst; \
		var = _u.v; \
	} while (0)

/*%
 * Use this in translation units that would otherwise be empty, to
 * suppress compiler warnings.
 */
#define EMPTY_TRANSLATION_UNIT static void isc__empty(void) { isc__empty(); }

/*%
 * We use macros instead of calling the routines directly because
 * the capital letters make the locking stand out.
 * We RUNTIME_CHECK for success since in general there's no way
 * for us to continue if they fail.
 */

#include <isc/result.h>		/* Contractual promise. */
/*
 * List Macros.
 */
#include <isc/list.h>		/* Contractual promise. */

#define LIST(type)			ISC_LIST(type)
#define INIT_LIST(type)			ISC_LIST_INIT(type)
#define LINK(type)			ISC_LINK(type)
#define INIT_LINK(elt, link)		ISC_LINK_INIT(elt, link)
#define HEAD(list)			ISC_LIST_HEAD(list)
#define TAIL(list)			ISC_LIST_TAIL(list)
#define EMPTY(list)			ISC_LIST_EMPTY(list)
#define PREV(elt, link)			ISC_LIST_PREV(elt, link)
#define NEXT(elt, link)			ISC_LIST_NEXT(elt, link)
#define APPEND(list, elt, link)		ISC_LIST_APPEND(list, elt, link)
#define PREPEND(list, elt, link)	ISC_LIST_PREPEND(list, elt, link)
#define UNLINK(list, elt, link)		ISC_LIST_UNLINK(list, elt, link)
#define ENQUEUE(list, elt, link)	ISC_LIST_APPEND(list, elt, link)
#define DEQUEUE(list, elt, link)	ISC_LIST_UNLINK(list, elt, link)
#define INSERTBEFORE(li, b, e, ln)	ISC_LIST_INSERTBEFORE(li, b, e, ln)
#define INSERTAFTER(li, a, e, ln)	ISC_LIST_INSERTAFTER(li, a, e, ln)
#define APPENDLIST(list1, list2, link)	ISC_LIST_APPENDLIST(list1, list2, link)

/*%
 * Performance
 */

/*
 * Assertions
 */
#include <isc/assertions.h>	/* Contractual promise. */

/*% Require Assertion */
#define REQUIRE(e)			ISC_REQUIRE(e)
/*% Ensure Assertion */
#define ENSURE(e)			ISC_ENSURE(e)
/*% Insist Assertion */
#define INSIST(e)			ISC_INSIST(e)
/*% Invariant Assertion */
#define INVARIANT(e)			ISC_INVARIANT(e)

/*
 * Errors
 */
#include <isc/error.h>		/* Contractual promise. */

/*% Unexpected Error */
#define UNEXPECTED_ERROR		isc_error_unexpected
/*% Fatal Error */
#define FATAL_ERROR			isc_error_fatal
/*% Runtime Check */
#define RUNTIME_CHECK(cond)		ISC_ERROR_RUNTIMECHECK(cond)

#endif /* ISC_UTIL_H */
