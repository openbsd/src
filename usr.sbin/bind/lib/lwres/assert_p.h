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

/* $ISC: assert_p.h,v 1.9 2001/01/09 21:59:12 bwelling Exp $ */

#ifndef LWRES_ASSERT_P_H
#define LWRES_ASSERT_P_H 1

#include <assert.h>		/* Required for assert() prototype. */

#define REQUIRE(x)		assert(x)
#define INSIST(x)		assert(x)

#define UNUSED(x)		((void)(x))

#define SPACE_OK(b, s)		(LWRES_BUFFER_AVAILABLECOUNT(b) >= (s))
#define SPACE_REMAINING(b, s)	(LWRES_BUFFER_REMAINING(b) >= (s))

#endif /* LWRES_ASSERT_P_H */
