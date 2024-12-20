/*	$OpenBSD: mlkem_unittest.c,v 1.4 2024/12/20 00:07:12 tb Exp $ */
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

static int
MlKem768UnitTest(void)
{
	struct MLKEM768_private_key priv = { 0 }, priv2 = { 0 };
	struct MLKEM768_public_key pub = { 0 }, pub2 = { 0 };
	uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
	uint8_t ciphertext[MLKEM768_CIPHERTEXT_BYTES];
	uint8_t shared_secret1[MLKEM_SHARED_SECRET_BYTES];
	uint8_t shared_secret2[MLKEM_SHARED_SECRET_BYTES];
	uint8_t first_two_bytes[2];
	uint8_t *encoded_private_key = NULL, *tmp_buf = NULL;
	size_t encoded_private_key_len, tmp_buf_len;
	CBS cbs;
	int failed = 0;

	MLKEM768_generate_key(encoded_public_key, NULL, &priv);

	memcpy(first_two_bytes, encoded_public_key, sizeof(first_two_bytes));
	memset(encoded_public_key, 0xff, sizeof(first_two_bytes));

	CBS_init(&cbs, encoded_public_key, sizeof(encoded_public_key));

	/* Parsing should fail because the first coefficient is >= kPrime. */
	if (MLKEM768_parse_public_key(&pub, &cbs)) {
		warnx("MLKEM768_parse_public_key should have failed");
		failed |= 1;
	}

	memcpy(encoded_public_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_public_key, sizeof(encoded_public_key));
	if (!MLKEM768_parse_public_key(&pub, &cbs)) {
		warnx("MLKEM768_parse_public_key");
		failed |= 1;
	}

	if (CBS_len(&cbs) != 0u) {
		warnx("CBS_len must be 0");
		failed |= 1;
	}

	if (!mlkem768_encode_public_key(&pub, &tmp_buf, &tmp_buf_len)) {
		warnx("encode_public_key");
		failed |= 1;
	}
	if (sizeof(encoded_public_key) != tmp_buf_len) {
		warnx("mlkem768 encoded public key lengths differ");
		failed |= 1;
	}

	if (compare_data(encoded_public_key, tmp_buf, tmp_buf_len, 768,
	    "encoded public keys") != 0) {
		warnx("compare_data");
		failed |= 1;
	}
	free(tmp_buf);
	tmp_buf = NULL;

	MLKEM768_public_from_private(&pub2, &priv);
	if (!mlkem768_encode_public_key(&pub2, &tmp_buf, &tmp_buf_len)) {
		warnx("mlkem768_encode_public_key");
		failed |= 1;
	}
	if (sizeof(encoded_public_key) != tmp_buf_len) {
		warnx("mlkem768 encoded public key lengths differ");
		failed |= 1;
	}

	if (compare_data(encoded_public_key, tmp_buf, tmp_buf_len, 768,
	    "encoded public keys") != 0) {
		warnx("compare_data");
		failed |= 1;
	}
	free(tmp_buf);
	tmp_buf = NULL;

	if (!mlkem768_encode_private_key(&priv, &encoded_private_key,
	    &encoded_private_key_len)) {
		warnx("mlkem768_encode_private_key");
		failed |= 1;
	}

	memcpy(first_two_bytes, encoded_private_key, sizeof(first_two_bytes));
	memset(encoded_private_key, 0xff, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);

	/*  Parsing should fail because the first coefficient is >= kPrime. */
	if (MLKEM768_parse_private_key(&priv2, &cbs)) {
		warnx("MLKEM768_parse_private_key should have failed");
		failed |= 1;
	}

	memcpy(encoded_private_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);

	if (!MLKEM768_parse_private_key(&priv2, &cbs)) {
		warnx("MLKEM768_parse_private_key");
		failed |= 1;
	}

	if (!mlkem768_encode_private_key(&priv2, &tmp_buf, &tmp_buf_len)) {
		warnx("mlkem768_encode_private_key");
		failed |= 1;
	}

	if (encoded_private_key_len != tmp_buf_len) {
		warnx("mlkem768 encode private key lengths differ");
		failed |= 1;
	}

	if (compare_data(encoded_private_key, tmp_buf, tmp_buf_len, 768,
	    "encoded private key") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	free(tmp_buf);
	tmp_buf = NULL;

	MLKEM768_encap(ciphertext, shared_secret1, &pub);
	MLKEM768_decap(shared_secret2, ciphertext, MLKEM768_CIPHERTEXT_BYTES,
	    &priv);
	if (compare_data(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    768, "shared secrets with priv") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	MLKEM768_decap(shared_secret2, ciphertext, MLKEM768_CIPHERTEXT_BYTES,
	    &priv2);
	if (compare_data(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    768, "shared secrets with priv2") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	free(encoded_private_key);

	return failed;
}

static int
MlKem1024UnitTest(void)
{
	struct MLKEM1024_private_key priv = { 0 }, priv2 = { 0 };
	struct MLKEM1024_public_key pub = { 0 }, pub2 = { 0 };
	uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
	uint8_t ciphertext[MLKEM1024_CIPHERTEXT_BYTES];
	uint8_t shared_secret1[MLKEM_SHARED_SECRET_BYTES];
	uint8_t shared_secret2[MLKEM_SHARED_SECRET_BYTES];
	uint8_t first_two_bytes[2];
	uint8_t *encoded_private_key = NULL, *tmp_buf = NULL;
	size_t encoded_private_key_len, tmp_buf_len;
	CBS cbs;
	int failed = 0;

	MLKEM1024_generate_key(encoded_public_key, NULL, &priv);

	memcpy(first_two_bytes, encoded_public_key, sizeof(first_two_bytes));
	memset(encoded_public_key, 0xff, sizeof(first_two_bytes));

	CBS_init(&cbs, encoded_public_key, sizeof(encoded_public_key));

	/* Parsing should fail because the first coefficient is >= kPrime. */
	if (MLKEM1024_parse_public_key(&pub, &cbs)) {
		warnx("MLKEM1024_parse_public_key should have failed");
		failed |= 1;
	}

	memcpy(encoded_public_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_public_key, sizeof(encoded_public_key));
	if (!MLKEM1024_parse_public_key(&pub, &cbs)) {
		warnx("MLKEM1024_parse_public_key");
		failed |= 1;
	}

	if (CBS_len(&cbs) != 0u) {
		warnx("CBS_len must be 0");
		failed |= 1;
	}

	if (!mlkem1024_encode_public_key(&pub, &tmp_buf, &tmp_buf_len)) {
		warnx("encode_public_key");
		failed |= 1;
	}
	if (sizeof(encoded_public_key) != tmp_buf_len) {
		warnx("mlkem1024 encoded public key lengths differ");
		failed |= 1;
	}

	if (compare_data(encoded_public_key, tmp_buf, tmp_buf_len, 1024,
	    "encoded public keys") != 0) {
		warnx("compare_data");
		failed |= 1;
	}
	free(tmp_buf);
	tmp_buf = NULL;

	MLKEM1024_public_from_private(&pub2, &priv);
	if (!mlkem1024_encode_public_key(&pub2, &tmp_buf, &tmp_buf_len)) {
		warnx("mlkem1024_encode_public_key");
		failed |= 1;
	}
	if (sizeof(encoded_public_key) != tmp_buf_len) {
		warnx("mlkem1024 encoded public key lengths differ");
		failed |= 1;
	}

	if (compare_data(encoded_public_key, tmp_buf, tmp_buf_len, 1024,
	    "encoded public keys") != 0) {
		warnx("compare_data");
		failed |= 1;
	}
	free(tmp_buf);
	tmp_buf = NULL;

	if (!mlkem1024_encode_private_key(&priv, &encoded_private_key,
	    &encoded_private_key_len)) {
		warnx("mlkem1024_encode_private_key");
		failed |= 1;
	}

	memcpy(first_two_bytes, encoded_private_key, sizeof(first_two_bytes));
	memset(encoded_private_key, 0xff, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);

	/*  Parsing should fail because the first coefficient is >= kPrime. */
	if (MLKEM1024_parse_private_key(&priv2, &cbs)) {
		warnx("MLKEM1024_parse_private_key should have failed");
		failed |= 1;
	}

	memcpy(encoded_private_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);

	if (!MLKEM1024_parse_private_key(&priv2, &cbs)) {
		warnx("MLKEM1024_parse_private_key");
		failed |= 1;
	}

	if (!mlkem1024_encode_private_key(&priv2, &tmp_buf, &tmp_buf_len)) {
		warnx("mlkem1024_encode_private_key");
		failed |= 1;
	}

	if (encoded_private_key_len != tmp_buf_len) {
		warnx("mlkem1024 encode private key lengths differ");
		failed |= 1;
	}

	if (compare_data(encoded_private_key, tmp_buf, tmp_buf_len, 1024,
	    "encoded private key") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	free(tmp_buf);
	tmp_buf = NULL;

	MLKEM1024_encap(ciphertext, shared_secret1, &pub);
	MLKEM1024_decap(shared_secret2, ciphertext, MLKEM1024_CIPHERTEXT_BYTES,
	    &priv);
	if (compare_data(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    1024, "shared secrets with priv") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	MLKEM1024_decap(shared_secret2, ciphertext, MLKEM1024_CIPHERTEXT_BYTES,
	    &priv2);
	if (compare_data(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    1024, "shared secrets with priv2") != 0) {
		warnx("compare_data");
		failed |= 1;
	}

	free(encoded_private_key);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= MlKem768UnitTest();
	failed |= MlKem1024UnitTest();

	return failed;
}
