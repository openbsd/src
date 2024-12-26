/*	$OpenBSD: mlkem_tests_util.c,v 1.5 2024/12/26 00:04:24 tb Exp $ */
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
compare_data(const uint8_t *want, const uint8_t *got, size_t len, const char *msg)
{
	if (memcmp(want, got, len) == 0)
		return 0;

	warnx("FAIL: %s differs", msg);
	fprintf(stderr, "want:\n");
	hexdump(want, len, got);
	fprintf(stderr, "got:\n");
	hexdump(got, len, want);
	fprintf(stderr, "\n");

	return 1;
}

int
mlkem768_encode_private_key(const void *private_key, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM768_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM768_marshal_private_key(&cbb, private_key))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mlkem768_encode_public_key(const void *public_key, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM768_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM768_marshal_public_key(&cbb, public_key))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mlkem1024_encode_private_key(const void *private_key, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM1024_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM1024_marshal_private_key(&cbb, private_key))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mlkem1024_encode_public_key(const void *public_key, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	int ret = 0;

	if (!CBB_init(&cbb, MLKEM1024_PUBLIC_KEY_BYTES))
		goto err;
	if (!MLKEM1024_marshal_public_key(&cbb, public_key))
		goto err;
	if (!CBB_finish(&cbb, out_buf, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mlkem768_decap(uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len, const void *private_key)
{
	return MLKEM768_decap(out_shared_secret, ciphertext, ciphertext_len,
	    private_key);
}

void
mlkem768_encap(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const void *public_key)
{
	MLKEM768_encap(out_ciphertext, out_shared_secret, public_key);
}

void
mlkem768_encap_external_entropy(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const void *public_key, const uint8_t entropy[MLKEM_ENCAP_ENTROPY])
{
	MLKEM768_encap_external_entropy(out_ciphertext, out_shared_secret,
	    public_key, entropy);
}

void
mlkem768_generate_key(uint8_t *out_encoded_public_key,
    uint8_t optional_out_seed[MLKEM_SEED_BYTES], void *out_private_key)
{
	MLKEM768_generate_key(out_encoded_public_key, optional_out_seed,
	    out_private_key);
}

void
mlkem768_generate_key_external_entropy(uint8_t *out_encoded_public_key,
    void *out_private_key, const uint8_t entropy[MLKEM_SEED_BYTES])
{
	MLKEM768_generate_key_external_entropy(out_encoded_public_key,
	    out_private_key, entropy);
}

int
mlkem768_parse_private_key(void *out_private_key, CBS *private_key_cbs)
{
	return MLKEM768_parse_private_key(out_private_key, private_key_cbs);
}

int
mlkem768_parse_public_key(void *out_public_key, CBS *public_key_cbs)
{
	return MLKEM768_parse_public_key(out_public_key, public_key_cbs);
}

void
mlkem768_public_from_private(void *out_public_key, const void *private_key)
{
	MLKEM768_public_from_private(out_public_key, private_key);
}

int
mlkem1024_decap(uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len, const void *private_key)
{
	return MLKEM1024_decap(out_shared_secret, ciphertext, ciphertext_len,
	    private_key);
}

void
mlkem1024_encap(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const void *public_key)
{
	MLKEM1024_encap(out_ciphertext, out_shared_secret, public_key);
}

void
mlkem1024_encap_external_entropy(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const void *public_key, const uint8_t entropy[MLKEM_ENCAP_ENTROPY])
{
	MLKEM1024_encap_external_entropy(out_ciphertext, out_shared_secret,
	    public_key, entropy);
}

void
mlkem1024_generate_key(uint8_t *out_encoded_public_key,
    uint8_t optional_out_seed[MLKEM_SEED_BYTES], void *out_private_key)
{
	MLKEM1024_generate_key(out_encoded_public_key, optional_out_seed,
	    out_private_key);
}

void
mlkem1024_generate_key_external_entropy(uint8_t *out_encoded_public_key,
    void *out_private_key, const uint8_t entropy[MLKEM_SEED_BYTES])
{
	MLKEM1024_generate_key_external_entropy(out_encoded_public_key,
	    out_private_key, entropy);
}

int
mlkem1024_parse_private_key(void *out_private_key, CBS *private_key_cbs)
{
	return MLKEM1024_parse_private_key(out_private_key, private_key_cbs);
}

void
mlkem1024_public_from_private(void *out_public_key, const void *private_key)
{
	MLKEM1024_public_from_private(out_public_key, private_key);
}

int
mlkem1024_parse_public_key(void *out_public_key, CBS *public_key_cbs)
{
	return MLKEM1024_parse_public_key(out_public_key, public_key_cbs);
}
