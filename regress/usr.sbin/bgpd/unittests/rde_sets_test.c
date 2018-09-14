/*	$OpenBSD: rde_sets_test.c,v 1.3 2018/09/14 10:23:42 claudio Exp $ */

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
u_int32_t vc[] = { 42 };

static struct as_set *
build_set(const char *name, u_int32_t *mem, size_t nmemb, size_t initial)
{
	struct as_set *a;

	a = as_set_new(name, initial, sizeof(*mem));
	if (a == NULL)
		err(1, "as_set_new %s", name);
	if (as_set_add(a, mem, nmemb) != 0)
		err(1, "as_set_add %s", name);
	as_set_prep(a);

	return a;
}

int
main(int argc, char **argv)
{
	struct as_set *a, *aa, *b, *c, *empty;
	size_t i;

	a = build_set("a", va, sizeof(va) / sizeof(va[0]),
	    sizeof(va) / sizeof(va[0]));
	aa = build_set("aa", vaa, sizeof(vaa) / sizeof(vaa[0]), 0);
	b = build_set("b", vb, sizeof(vb) / sizeof(vb[0]), 1);
	c = build_set("c", vc, sizeof(vc) / sizeof(vc[0]), 1);
	empty = build_set("empty", NULL, 0, 0);

	if (!as_set_equal(a, a))
		errx(1, "as_set_equal(a, a) non equal");
	if (!as_set_equal(a, aa))
		errx(1, "as_set_equal(a, aa) non equal");
	if (as_set_equal(a, b))
		errx(1, "as_set_equal(a, b) equal");

	for (i = 0; i < sizeof(va) / sizeof(va[0]); i++)
		if (!as_set_match(a, va[i]))
			errx(1, "as_set_match(a, %u) failed to match", va[i]);
	for (i = 0; i < sizeof(vb) / sizeof(vb[0]); i++)
		if (as_set_match(a, vb[i]))
			errx(1, "as_set_match(a, %u) matched but should not",
			    vb[i]);
	if (!as_set_match(c, 42))
		errx(1, "as_set_match(c, %u) failed to match", 42);
	if (as_set_match(c, 7))
		errx(1, "as_set_match(c, %u) matched but should not", 7);
	
	if (!as_set_equal(empty, empty))
		errx(1, "as_set_equal(empty, empty) non equal");
	if (as_set_match(empty, 42))
		errx(1, "as_set_match(empty, %u) matched but should not", 42);

	as_set_free(a);
	as_set_free(aa);
	as_set_free(b);
	as_set_free(c);
	as_set_free(empty);

	printf("OK\n");
	return 0;
}
