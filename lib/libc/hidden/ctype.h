/*	$OpenBSD: ctype.h,v 1.2 2015/09/19 04:02:21 guenther Exp $	*/
/*
 * Copyright (c) 2015 Philip Guenther <guenther@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LIBC_CTYPE_H_
#define _LIBC_CTYPE_H_

#include_next <ctype.h>

#if 0
extern PROTO_NORMAL(_ctype_);
extern PROTO_NORMAL(_tolower_tab_);
extern PROTO_NORMAL(_toupper_tab_);
#endif

PROTO_NORMAL(isalnum);
PROTO_NORMAL(isalpha);
PROTO_NORMAL(iscntrl);
PROTO_NORMAL(isdigit);
PROTO_NORMAL(isgraph);
PROTO_NORMAL(islower);
PROTO_NORMAL(isprint);
PROTO_NORMAL(ispunct);
PROTO_NORMAL(isspace);
PROTO_NORMAL(isupper);
PROTO_NORMAL(isxdigit);
PROTO_NORMAL(tolower);
PROTO_NORMAL(toupper);
PROTO_NORMAL(isblank);
PROTO_NORMAL(isascii);
PROTO_DEPRECATED(toascii);
PROTO_STD_DEPRECATED(_tolower);
PROTO_STD_DEPRECATED(_toupper);

#endif /* !_LIBC_CTYPE_H_ */
