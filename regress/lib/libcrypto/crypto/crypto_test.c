/*	$OpenBSD: crypto_test.c,v 1.1 2024/04/25 14:27:29 jsing Exp $	*/
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

#include "crypto_internal.h"

static int
test_ct_u8(void)
{
	uint8_t i, j, mask;
	int failed = 1;

	i = 0;

	do {
		if ((i != 0) != crypto_ct_ne_zero_u8(i)) {
			fprintf(stderr, "FAIL: crypto_ct_ne_zero_u8(%d) = %d, "
			    "want %d\n", i, crypto_ct_ne_zero_u8(i), i != 0);
			goto failure;
		}
		mask = (i != 0) ? 0xff : 0x00;
		if (mask != crypto_ct_ne_zero_mask_u8(i)) {
			fprintf(stderr, "FAIL: crypto_ct_ne_zero_mask_u8(%d) = %x, "
			    "want %x\n", i, crypto_ct_ne_zero_mask_u8(i), mask);
			goto failure;
		}
		if ((i == 0) != crypto_ct_eq_zero_u8(i)) {
			fprintf(stderr, "FAIL: crypto_ct_eq_zero_u8(%d) = %d, "
			    "want %d\n", i, crypto_ct_ne_zero_u8(i), i != 0);
			goto failure;
		}
		mask = (i == 0) ? 0xff : 0x00;
		if (mask != crypto_ct_eq_zero_mask_u8(i)) {
			fprintf(stderr, "FAIL: crypto_ct_eq_zero_mask_u8(%d) = %x, "
			    "want %x\n", i, crypto_ct_ne_zero_mask_u8(i), mask);
			goto failure;
		}

		j = 0;

		do {
			if ((i != j) != crypto_ct_ne_u8(i, j)) {
				fprintf(stderr, "FAIL: crypto_ct_ne_u8(%d, %d) = %d, "
				    "want %d\n", i, j, crypto_ct_ne_u8(i, j), i != j);
				goto failure;
			}
			mask = (i != j) ? 0xff : 0x00;
			if (mask != crypto_ct_ne_mask_u8(i, j)) {
				fprintf(stderr, "FAIL: crypto_ct_ne_mask_u8(%d, %d) = %x, "
				    "want %x\n", i, j, crypto_ct_ne_mask_u8(i, j), mask);
				goto failure;
			}
			if ((i == j) != crypto_ct_eq_u8(i, j)) {
				fprintf(stderr, "FAIL: crypto_ct_eq_u8(%d, %d) = %d, "
				    "want %d\n", i, j, crypto_ct_eq_u8(i, j), i != j);
				goto failure;
			}
			mask = (i == j) ? 0xff : 0x00;
			if (mask != crypto_ct_eq_mask_u8(i, j)) {
				fprintf(stderr, "FAIL: crypto_ct_eq_mask_u8(%d, %d) = %x, "
				    "want %x\n", i, j, crypto_ct_eq_mask_u8(i, j), mask);
				goto failure;
			}
		} while (++j != 0);
	} while (++i != 0);

	failed = 0;

 failure:
	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= test_ct_u8();

	return failed;
}
