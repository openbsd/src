/*	$OpenBSD: amd64_get_mtrr.c,v 1.1 2004/02/28 23:51:35 deraadt Exp $	*/

/*
 * Copyright (c) 2004 Theo de Raadt
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <machine/segments.h>
#include <machine/sysarch.h>

int
amd64_get_mtrr(struct mtrr *mtrr, int *n)
{
	struct amd64_get_mtrr_args a;

	a.mtrrp = mtrr;
	a.n = n;

	return sysarch(AMD64_GET_MTRR, (void *)&a);
}
