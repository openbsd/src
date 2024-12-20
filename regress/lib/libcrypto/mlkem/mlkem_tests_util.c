/*	$OpenBSD: mlkem_tests_util.c,v 1.3 2024/12/20 00:07:12 tb Exp $ */
/*
 * Copyright (c) 2024 Google Inc.
 * Copyright (c) 2024 Bob Beck <beck@obtuse.com>
 * Copyright (c) 2024 Theo Buehler <tb@openbsd.org>
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
#include <string.h>

#include "bytestring.h"
#include "mlkem.h"

#include "mlkem_internal.h"

#include "mlkem_tests_util.h"

int failure;
int test_number;

static void
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
compare_data(const uint8_t *want, const uint8_t *got, size_t len, size_t line,
    const char *msg)
{
	if (memcmp(want, got, len) == 0)
		return 0;

	warnx("FAIL: #%zu - %s differs", line, msg);
	fprintf(stderr, "want:\n");
	hexdump(want, len, got);
	fprintf(stderr, "got:\n");
	hexdump(got, len, want);
	fprintf(stderr, "\n");

	return 1;
}

int
compare_length(size_t want, size_t got, size_t line, const char *msg)
{
	if (want == got)
		return 1;

	warnx("#%zu: %s: want %zu, got %zu", line, msg, want, got);
	return 0;
}

static int
hex_get_nibble_cbs(CBS *cbs, uint8_t *out_nibble)
{
	uint8_t c;

	if (!CBS_get_u8(cbs, &c))
		return 0;

	if (c >= '0' && c <= '9') {
		*out_nibble = c - '0';
		return 1;
	}
	if (c >= 'a' && c <= 'f') {
		*out_nibble = c - 'a' + 10;
		return 1;
	}
	if (c >= 'A' && c <= 'F') {
		*out_nibble = c - 'A' + 10;
		return 1;
	}

	return 0;
}

void
hex_decode_cbs(CBS *cbs, CBB *cbb, size_t line, const char *msg)
{
	if (!CBB_init(cbb, 0))
		errx(1, "#%zu %s: %s CBB_init", line, msg, __func__);

	while (CBS_len(cbs) > 0) {
		uint8_t hi, lo;

		if (!hex_get_nibble_cbs(cbs, &hi))
			errx(1, "#%zu %s: %s nibble", line, msg, __func__);
		if (!hex_get_nibble_cbs(cbs, &lo))
			errx(1, "#%zu %s: %s nibble", line, msg, __func__);

		if (!CBB_add_u8(cbb, hi << 4 | lo))
			errx(1, "#%zu %s: %s CBB_add_u8", line, msg, __func__);
	}
}

int
get_string_cbs(CBS *cbs_in, const char *str, size_t line, const char *msg)
{
	CBS cbs;
	size_t len = strlen(str);

	if (!CBS_get_bytes(cbs_in, &cbs, len))
		errx(1, "#%zu %s: %s CBB_get_bytes", line, msg, __func__);

	return CBS_mem_equal(&cbs, str, len);
}

int
mlkem768_encode_private_key(const struct MLKEM768_private_key *priv,
    uint8_t **out_buf, size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM768_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM768_marshal_private_key(&cbb, priv))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mlkem768_encode_public_key(const struct MLKEM768_public_key *pub,
    uint8_t **out_buf, size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM768_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM768_marshal_public_key(&cbb, pub))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mlkem1024_encode_private_key(const struct MLKEM1024_private_key *priv,
    uint8_t **out_buf, size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM1024_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM1024_marshal_private_key(&cbb, priv))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mlkem1024_encode_public_key(const struct MLKEM1024_public_key *pub,
    uint8_t **out_buf, size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM1024_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM1024_marshal_public_key(&cbb, pub))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}
