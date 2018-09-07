/*	$OpenBSD: rde_sets_test.c,v 1.1 2018/09/07 08:40:00 claudio Exp $ */

/*
 * Copyright (c) 2018 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/types.h>
#include <sys/queue.h>

#include <stdio.h>
#include <err.h>

#include "rde.h"

u_int32_t va[] = { 19, 14, 32, 76, 125 };
u_int32_t vaa[] = { 125, 14, 76, 32, 19 };
u_int32_t vb[] = { 256, 1024, 512, 4096, 2048, 512 };

int
main(int argc, char **argv)
{
	struct as_set *a, *aa, *b;
	size_t i;

	a = as_set_new("a", sizeof(va) / sizeof(va[0]));
	if (a == NULL)
		err(1, "as_set_new a");
	if (as_set_add(a, va, sizeof(va) / sizeof(va[0])) != 0)
		err(1, "as_set_add a");

	aa = as_set_new("aa", 0);
	if (aa == NULL)
		err(1, "as_set_new aa");
	if (as_set_add(aa, vaa, sizeof(vaa) / sizeof(vaa[0])) != 0)
		err(1, "as_set_add aa");

	b = as_set_new("b", 0);
	if (b == NULL)
		err(1, "as_set_new b");
	if (as_set_add(b, vb, sizeof(vb) / sizeof(vb[0])) != 0)
		err(1, "as_set_add b");

	as_set_prep(a);
	as_set_prep(aa);
	as_set_prep(b);

	if (!as_set_equal(a, aa))
		errx(1, "as_set_equal(a, aa) non equal");
	if (as_set_equal(a, b))
		errx(1, "as_set_equal(a, b) equal");

	for (i = 0; i < sizeof(va) / sizeof(va[0]); i++)
		if (!as_set_match(a, va[i]))
			errx(1, "as_set_match(a, %u) failed to match", va[i]);
	for (i = 0; i < sizeof(vb) / sizeof(vb[0]); i++)
		if (as_set_match(a, vb[i]))
			errx(1, "as_set_match(a, %u) matched but shouldn't",
			    vb[i]);
	
	printf("OK\n");
	return 0;
}
