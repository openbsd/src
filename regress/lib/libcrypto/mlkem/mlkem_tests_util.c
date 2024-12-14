/*	$OpenBSD: mlkem_tests_util.c,v 1.2 2024/12/14 19:16:24 tb Exp $ */
/*
 * Copyright (c) 2024, Google Inc.
 * Copyright (c) 2024, Bob Beck <beck@obtuse.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mlkem_tests_util.h"

int failure;
int test_number;

void
hexdump(const uint8_t *buf, size_t len, const uint8_t *compare)
{
	const char *mark = "";
	size_t i;

	for (i = 1; i <= len; i++) {
		if (compare != NULL)
			mark = (buf[i - 1] != compare[i - 1]) ? "*" : " ";
		fprintf(stderr, " %s0x%02hhx,%s", mark, buf[i - 1],
		    i % 8 && i != len ? "" : "\n");
	}
	fprintf(stderr, "\n");
}

int
hex_decode(char *buf, size_t len, uint8_t **out_buf, size_t *out_len)
{
	size_t i;
	if (*out_buf != NULL)
		abort(); /* Du hast einin rotweinflarsche... */

	MALLOC(*out_buf, len);
	*out_len = 0;

	for (i = 0; i < len; i += 2) {
		if (sscanf(buf + i, "%2hhx", *out_buf + *out_len) != 1)
			err(1, "FAIL- hex decode failed for %d\n",
			    (int)*out_len);
		(*out_len)++;
	}
	return 1;
}

void
grab_data(CBS *cbs, char *buf, size_t offset)
{
	char *start = buf + offset;
	size_t len = strlen(start);
	uint8_t *new = NULL;
	size_t new_len = 0;
	/* This is hex encoded - decode it. */
	TEST(!hex_decode(start, len - 1, &new, &new_len), "hex decode failed");
	CBS_init(cbs, new, new_len);
}
