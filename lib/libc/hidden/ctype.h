/*	$OpenBSD: ctype.h,v 1.3 2016/09/06 19:56:36 guenther Exp $	*/
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

/* sigh: predeclare and rename the functions which we'll declare inline */
__only_inline int	isalnum(int _c);
__only_inline int	isalpha(int _c);
__only_inline int	iscntrl(int _c);
__only_inline int	isdigit(int _c);
__only_inline int	isgraph(int _c);
__only_inline int	islower(int _c);
__only_inline int	isprint(int _c);
__only_inline int	ispunct(int _c);
__only_inline int	isspace(int _c);
__only_inline int	isupper(int _c);
__only_inline int	isxdigit(int _c);
__only_inline int	tolower(int _c);
__only_inline int	toupper(int _c);
__only_inline int	isblank(int _c);
__only_inline int	isascii(int _c);
__only_inline int	toascii(int _c);
__only_inline int	_tolower(int _c);
__only_inline int	_toupper(int _c);
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

#include_next <ctype.h>

#if 0
extern PROTO_NORMAL(_ctype_);
extern PROTO_NORMAL(_tolower_tab_);
extern PROTO_NORMAL(_toupper_tab_);
#endif

#endif /* !_LIBC_CTYPE_H_ */
