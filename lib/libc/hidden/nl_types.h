/*	$OpenBSD: nl_types.h,v 1.1 2015/09/05 11:25:30 guenther Exp $	*/
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
/*	$OpenBSD: nl_types.h,v 1.1 2015/09/05 11:25:30 guenther Exp $	*/
/*	$NetBSD: nl_types.h,v 1.6 1996/05/13 23:11:15 jtc Exp $	*/

#ifndef _LIBC_NL_TYPES_H_
#define _LIBC_NL_TYPES_H_

#include_next <nl_types.h>
#include "namespace.h"

PROTO_NORMAL(catclose);
PROTO_NORMAL(catgets);
PROTO_NORMAL(catopen);

#endif	/* _LIBC_NL_TYPES_H_ */
