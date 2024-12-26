/*	$OpenBSD: mlkem_unittest.c,v 1.6 2024/12/26 12:35:25 tb Exp $ */
/*
 * Copyright (c) 2024 Google Inc.
 * Copyright (c) 2024 Bob Beck <beck@obtuse.com>
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

#include "bytestring.h"
#include "mlkem.h"

#include "mlkem_tests_util.h"

struct unittest_ctx {
	void *priv;
	void *pub;
	void *priv2;
	void *pub2;
	uint8_t *encoded_public_key;
	size_t encoded_public_key_len;
	uint8_t *ciphertext;
	size_t ciphertext_len;
	mlkem_decap_fn decap;
	mlkem_encap_fn encap;
	mlkem_generate_key_fn generate_key;
	mlkem_parse_private_key_fn parse_private_key;
	mlkem_parse_public_key_fn parse_public_key;
	mlkem_encode_private_key_fn encode_private_key;
	mlkem_encode_public_key_fn encode_public_key;
	mlkem_public_from_private_fn public_from_private;
};

static int
MlKemUnitTest(struct unittest_ctx *ctx)
{
	uint8_t shared_secret1[MLKEM_SHARED_SECRET_BYTES];
	uint8_t shared_secret2[MLKEM_SHARED_SECRET_BYTES];
	uint8_t first_two_bytes[2];
	uint8_t *encoded_private_key = NULL, *tmp_buf = NULL;
	size_t encoded_private_key_len, tmp_buf_len;
	CBS cbs;
	int failed = 0;

	ctx->generate_key(ctx->encoded_public_key, NULL, ctx->priv);

	memcpy(first_two_bytes, ctx->encoded_public_key, sizeof(first_two_bytes));
	memset(ctx->encoded_public_key, 0xff, sizeof(first_two_bytes));

	CBS_init(&cbs, ctx->encoded_public_key, ctx->encoded_public_key_len);

	/* Parsing should fail because the first coefficient is >= kPrime. */
	if (ctx->parse_public_key(ctx->pub, &cbs)) {
		warnx("parse_public_key should have failed");
		failed |= 1;
	}

	memcpy(ctx->encoded_public_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, ctx->encoded_public_key, ctx->encoded_public_key_len);
	if (!ctx->parse_public_key(ctx->pub, &cbs)) {
		warnx("MLKEM768_parse_public_key");
		failed |= 1;
	}

	if (CBS_len(&cbs) != 0u) {
		warnx("CBS_len must be 0");
		failed |= 1;
	}

	if (!ctx->encode_public_key(ctx->pub, &tmp_buf, &tmp_buf_len)) {
		warnx("encode_public_key");
		failed |= 1;
	}
	if (ctx->encoded_public_key_len != tmp_buf_len) {
		warnx("encoded public key lengths differ");
		failed |= 1;
	}

	if (compare_data(ctx->encoded_public_key, tmp_buf, tmp_buf_len,
	    "encoded public keys") != 0) {
		warnx("compare_data");
		failed |= 1;
	}
	free(tmp_buf);
	tmp_buf = NULL;

	ctx->public_from_private(ctx->pub2, ctx->priv);
	if (!ctx->encode_public_key(ctx->pub2, &tmp_buf, &tmp_buf_len)) {
		warnx("encode_public_key");
		failed |= 1;
	}
	if (ctx->encoded_public_key_len != tmp_buf_len) {
		warnx("encoded public key lengths differ");
		failed |= 1;
	}

	if (compare_data(ctx->encoded_public_key, tmp_buf, tmp_buf_len,
	    "encoded public keys") != 0) {
		warnx("compare_data");
		failed |= 1;
	}
	free(tmp_buf);
	tmp_buf = NULL;

	if (!ctx->encode_private_key(ctx->priv, &encoded_private_key,
	    &encoded_private_key_len)) {
		warnx("mlkem768_encode_private_key");
		failed |= 1;
	}

	memcpy(first_two_bytes, encoded_private_key, sizeof(first_two_bytes));
	memset(encoded_private_key, 0xff, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);

	/*  Parsing should fail because the first coefficient is >= kPrime. */
	if (ctx->parse_private_key(ctx->priv2, &cbs)) {
		warnx("MLKEM768_parse_private_key should have failed");
		failed |= 1;
	}

	memcpy(encoded_private_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);

	if (!ctx->parse_private_key(ctx->priv2, &cbs)) {
		warnx("MLKEM768_parse_private_key");
		failed |= 1;
	}

	if (!ctx->encode_private_key(ctx->priv2, &tmp_buf, &tmp_buf_len)) {
		warnx("encode_private_key");
		failed |= 1;
	}

	if (encoded_private_key_len != tmp_buf_len) {
		warnx("encode private key lengths differ");
		failed |= 1;
	}

	if (compare_data(encoded_private_key, tmp_buf, tmp_buf_len,
	    "encoded private key") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	free(tmp_buf);
	tmp_buf = NULL;

	ctx->encap(ctx->ciphertext, shared_secret1, ctx->pub);
	ctx->decap(shared_secret2, ctx->ciphertext, ctx->ciphertext_len,
	    ctx->priv);
	if (compare_data(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    "shared secrets with priv") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	ctx->decap(shared_secret2, ctx->ciphertext, ctx->ciphertext_len,
	    ctx->priv2);
	if (compare_data(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    "shared secrets with priv2") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	free(encoded_private_key);

	return failed;
}

static int
mlkem768_unittest(void)
{
	struct MLKEM768_private_key mlkem768_priv, mlkem768_priv2;
	struct MLKEM768_public_key mlkem768_pub, mlkem768_pub2;
	uint8_t mlkem768_encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
	uint8_t mlkem768_ciphertext[MLKEM768_CIPHERTEXT_BYTES];
	struct unittest_ctx mlkem768_test = {
		.priv = &mlkem768_priv,
		.pub = &mlkem768_pub,
		.priv2 = &mlkem768_priv2,
		.pub2 = &mlkem768_pub2,
		.encoded_public_key = mlkem768_encoded_public_key,
		.encoded_public_key_len = sizeof(mlkem768_encoded_public_key),
		.ciphertext = mlkem768_ciphertext,
		.ciphertext_len = sizeof(mlkem768_ciphertext),
		.decap = mlkem768_decap,
		.encap = mlkem768_encap,
		.generate_key = mlkem768_generate_key,
		.parse_private_key = mlkem768_parse_private_key,
		.parse_public_key = mlkem768_parse_public_key,
		.encode_private_key = mlkem768_encode_private_key,
		.encode_public_key = mlkem768_encode_public_key,
		.public_from_private = mlkem768_public_from_private,
	};

	return MlKemUnitTest(&mlkem768_test);
}

static int
mlkem1024_unittest(void)
{
	struct MLKEM1024_private_key mlkem1024_priv, mlkem1024_priv2;
	struct MLKEM1024_public_key mlkem1024_pub, mlkem1024_pub2;
	uint8_t mlkem1024_encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
	uint8_t mlkem1024_ciphertext[MLKEM1024_CIPHERTEXT_BYTES];
	struct unittest_ctx mlkem1024_test = {
		.priv = &mlkem1024_priv,
		.pub = &mlkem1024_pub,
		.priv2 = &mlkem1024_priv2,
		.pub2 = &mlkem1024_pub2,
		.encoded_public_key = mlkem1024_encoded_public_key,
		.encoded_public_key_len = sizeof(mlkem1024_encoded_public_key),
		.ciphertext = mlkem1024_ciphertext,
		.ciphertext_len = sizeof(mlkem1024_ciphertext),
		.decap = mlkem1024_decap,
		.encap = mlkem1024_encap,
		.generate_key = mlkem1024_generate_key,
		.parse_private_key = mlkem1024_parse_private_key,
		.parse_public_key = mlkem1024_parse_public_key,
		.encode_private_key = mlkem1024_encode_private_key,
		.encode_public_key = mlkem1024_encode_public_key,
		.public_from_private = mlkem1024_public_from_private,
	};

	return MlKemUnitTest(&mlkem1024_test);
}

int
main(void)
{
	int failed = 0;

	/*
	 * XXX - this is split into two helper functions since having a few
	 * ML-KEM key blobs on the stack makes Emscripten's stack explode,
	 * leading to inscrutable silent failures unles ASAN is enabled.
	 * Go figure.
	 */

	failed |= mlkem768_unittest();
	failed |= mlkem1024_unittest();

	return failed;
}
