/* Copyright (c) 2024, Bob Beck <beck@obtuse.com>
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
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef MLKEM_TEST_UTIL_H
#define MLKEM_TEST_UTIL_H

#include "bytestring.h"

#define TEST(cond, msg) do {						\
	if ((cond)) {							\
		failure = 1;						\
		fprintf(stderr, "FAIL: %s:%d - Test %d: %s\n",		\
		    __FILE__, __LINE__, test_number, msg);		\
	}								\
} while(0)

#define MALLOC(A, B) do {						\
	if (((A) = malloc(B)) == NULL) {				\
		failure = 1;						\
		fprintf(stderr, "FAIL: %s:%d - Test %d: malloc\n",	\
		    __FILE__, __LINE__, test_number);			\
		exit(1);						\
	}								\
} while(0)

#define TEST_DATAEQ(values, expected, len, msg) do {			\
	if (memcmp((values), (expected), (len)) != 0) {			\
		failure = 1;						\
		fprintf(stderr, "FAIL: %s:%d - Test %d: %s differs\n",	\
		    __FILE__, __LINE__, test_number, msg);		\
		fprintf(stderr, "values:\n");				\
		hexdump(values, len, expected);				\
		fprintf(stderr, "expected:\n");				\
		hexdump(expected, len, values);				\
		fprintf(stderr, "\n");					\
	}								\
} while(0)

extern int failure, test_number;

void hexdump(const uint8_t *buf, size_t len, const uint8_t *compare);
int hex_decode(char *buf, size_t len, uint8_t **out_buf, size_t *out_len);
void grab_data(CBS *cbs, char *buf, size_t offset);

#endif
