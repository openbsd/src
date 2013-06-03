/*	$OpenBSD: cerror.S,v 1.2 2013/06/03 04:41:31 miod Exp $	*/

/*
 * Copyright (c) 2011 Mark Kettenis
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

#include "SYS.h"

#define ERRNOPTR_OFFSET	32

ENTRY(__cerror,0)
	mfctl	%cr27, %r1
	ldw	4(%r1), %r1
	ldw	ERRNOPTR_OFFSET(%r1), %r1
	stw	%t1, 0(%r1)
	ldi	-1, %ret0
	bv	%r0(%rp)
	ldi	-1, %ret1
EXIT(__cerror)

	.end
