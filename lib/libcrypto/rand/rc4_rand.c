/*	$OpenBSD: rc4_rand.c,v 1.1 2014/04/15 16:52:50 miod Exp $	*/

/*
 * Copyright (c) 2014 Miodrag Vallat.
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

#include <stdlib.h>

#include <openssl/rand.h>

static int
arc4_rand_bytes(unsigned char *buf, int num)
{
	if (num > 0)
		arc4random_buf(buf, (size_t)num);

	return 1;
}

static RAND_METHOD rand_arc4_meth = {
	.seed = NULL,		/* no external seed allowed */
	.bytes = arc4_rand_bytes,
	.cleanup = NULL,	/* no cleanup necessary */
	.add = NULL,		/* no external feed allowed */
	.pseudorand = arc4_rand_bytes,
	.status = NULL		/* no possible error condition */
};

RAND_METHOD *RAND_SSLeay(void)
{
	return &rand_arc4_meth;
}
