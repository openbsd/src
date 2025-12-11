/*	$OpenBSD: bitmap_test.c,v 1.1 2025/12/11 12:19:59 claudio Exp $ */

/*
 * Copyright (c) 2025 Claudio Jeker <claudio@openbsd.org>
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
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "bgpd.h"

static const uint32_t foobar[] = { 1, 2, 61, 62, 63, 64, 65, 66,
	250, 251, 252, 253, 254, 255, 256, 257, 258, 259,
	1020, 1021, 1022, 1023, 1024, 1025 };
static const uint32_t foobaz[] = { 17, 18, 67, 222, 512 };


int
main(int argc, char **argv)
{
	struct bitmap map;
	struct bitmap set = { 0 };
	uint32_t id, i;

	bitmap_init(&map);

	printf("testing bitmap_id_get: "); fflush(stdout);
	for (i = 1; i < 1024; i++) {
		if (bitmap_id_get(&map, &id) == -1)
			err(1, NULL);
		if (id != i)
			errx(1, "unexpected result %u != %u", id, i);
	}
	printf("OK\n");

	printf("testing bitmap_id_put: "); fflush(stdout);
	for (i = 0; i < nitems(foobar); i++) {
		bitmap_id_put(&map, foobar[i]);
	}
	for (i = 0; i < nitems(foobar); i++) {
		if (bitmap_id_get(&map, &id) == -1)
			err(1, NULL);
		if (id != foobar[i])
			errx(1, "unexpected result %u != %u", id, foobar[i]);
	}
	printf("OK\n");

	printf("testing put / get: "); fflush(stdout);
	for (i = 0; i < nitems(foobar); i++) {
		bitmap_id_put(&map, foobar[i]);
		if (bitmap_id_get(&map, &id) == -1)
			err(1, NULL);
		if (id != foobar[i])
			errx(1, "unexpected result %u != %u", id, foobar[i]);
	}
	printf("OK\n");

	printf("testing bitmap_set: "); fflush(stdout);
	for (i = 0; i < nitems(foobar); i++) {
		bitmap_set(&set, foobar[i]);
		if (bitmap_test(&set, foobar[i]) == 0)
			errx(1, "%u not set", foobar[i]);
	}
	for (i = 0; i < nitems(foobaz); i++) {
		if (bitmap_test(&set, foobaz[i]) != 0)
			errx(1, "%u set", foobaz[i]);
	}
	printf("OK\n");

	printf("testing bitmap_clear: "); fflush(stdout);
	for (i = 0; i < nitems(foobaz); i++) {
		bitmap_set(&set, foobaz[i]);
	}
	for (i = 0; i < nitems(foobar); i++) {
		bitmap_clear(&set, foobar[i]);
		if (bitmap_test(&set, foobar[i]) != 0)
			errx(1, "%u set", foobar[i]);
	}
	for (i = 0; i < nitems(foobaz); i++) {
		if (bitmap_test(&set, foobaz[i]) == 0)
			errx(1, "%u not set", foobaz[i]);
	}
	printf("OK\n");

	printf("testing bitmap_reset: "); fflush(stdout);
	bitmap_reset(&set);
	for (i = 0; i < nitems(foobar); i++) {
		if (bitmap_test(&set, foobar[i]) != 0)
			errx(1, "%u set", foobar[i]);
	}
	for (i = 0; i < nitems(foobaz); i++) {
		if (bitmap_test(&set, foobaz[i]) != 0)
			errx(1, "%u set", foobaz[i]);
	}
	printf("OK\n");

	printf("re-testing bitmap_set: "); fflush(stdout);
	for (i = 0; i < nitems(foobar); i++) {
		bitmap_set(&set, foobar[i]);
		if (bitmap_test(&set, foobar[i]) == 0)
			errx(1, "%u not set", foobar[i]);
	}
	for (i = 0; i < nitems(foobaz); i++) {
		if (bitmap_test(&set, foobaz[i]) != 0)
			errx(1, "%u set", foobaz[i]);
	}
	printf("OK\n");

	bitmap_reset(&map);

	printf("OK\n");
	return 0;
}
