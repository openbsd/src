/*	$OpenBSD: lhash_test.c,v 1.1 2024/05/06 14:31:25 jsing Exp $	*/
/*
 * Copyright (c) 2024 Joel Sing <jsing@openbsd.org>
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <openssl/lhash.h>

static void
test_doall_fn(void *arg1, void *arg2)
{
}

static int
test_lhash_doall(void)
{
	_LHASH *lh;
	int i;
	int failed = 1;

	if ((lh = lh_new(NULL, NULL)) == NULL)
		goto failure;

	/* Call doall multiple times while linked hash is empty. */
	for (i = 0; i < 100; i++)
		lh_doall_arg(lh, test_doall_fn, NULL);

	lh_free(lh);

	failed = 0;

 failure:
	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= test_lhash_doall();

	return failed;
}
