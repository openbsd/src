/*	$OpenBSD$ */
/*
 * Copyright (c) 2026 Bob Beck <beck@obtuse.com>
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

#include <openssl/mldsa.h>

#include "mlkem_tests_util.h"

static const uint8_t message[] = "ML-DSA unit test message";
static const uint8_t context[] = "ML-DSA unit test context";

static int
MldsaUnitTest(int rank)
{
	MLDSA_private_key *priv = NULL, *priv_seed = NULL, *priv_bad = NULL;
	MLDSA_public_key *pub = NULL, *pub_seed = NULL, *pub_parsed = NULL;
	uint8_t *encoded_public_key = NULL, *seed = NULL, *marshaled = NULL;
	uint8_t *signature = NULL, *signature2 = NULL;
	size_t encoded_public_key_len = 0, seed_len = 0, marshaled_len = 0;
	size_t signature_len = 0, signature2_len = 0;
	int failed = 0;

	if ((priv = MLDSA_private_key_new(rank)) == NULL) {
		warnx("MLDSA_private_key_new");
		failed |= 1;
		goto done;
	}

	if (!MLDSA_generate_key(priv, &encoded_public_key,
	    &encoded_public_key_len, &seed, &seed_len)) {
		warnx("MLDSA_generate_key");
		failed |= 1;
		goto done;
	}

	/* The public key derived from the private key must match. */
	if ((pub = MLDSA_public_key_new(rank)) == NULL) {
		warnx("MLDSA_public_key_new");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_public_from_private(priv, pub)) {
		warnx("MLDSA_public_from_private");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_marshal_public_key(pub, &marshaled, &marshaled_len)) {
		warnx("MLDSA_marshal_public_key");
		failed |= 1;
		goto done;
	}
	if (marshaled_len != encoded_public_key_len ||
	    compare_data(encoded_public_key, marshaled, marshaled_len,
	    "public_from_private") != 0) {
		warnx("public_from_private key mismatch");
		failed |= 1;
	}
	free(marshaled);
	marshaled = NULL;

	/* Reconstructing the key from the seed must reproduce the public key. */
	if ((priv_seed = MLDSA_private_key_new(rank)) == NULL) {
		warnx("MLDSA_private_key_new");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_private_key_from_seed(priv_seed, seed, seed_len)) {
		warnx("MLDSA_private_key_from_seed");
		failed |= 1;
		goto done;
	}
	if ((pub_seed = MLDSA_public_key_new(rank)) == NULL) {
		warnx("MLDSA_public_key_new");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_public_from_private(priv_seed, pub_seed)) {
		warnx("MLDSA_public_from_private (seed)");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_marshal_public_key(pub_seed, &marshaled, &marshaled_len)) {
		warnx("MLDSA_marshal_public_key (seed)");
		failed |= 1;
		goto done;
	}
	if (marshaled_len != encoded_public_key_len ||
	    compare_data(encoded_public_key, marshaled, marshaled_len,
	    "from_seed") != 0) {
		warnx("from_seed public key mismatch");
		failed |= 1;
	}
	free(marshaled);
	marshaled = NULL;

	/* A seed of the wrong length must be rejected. */
	if ((priv_bad = MLDSA_private_key_new(rank)) == NULL) {
		warnx("MLDSA_private_key_new");
		failed |= 1;
		goto done;
	}
	if (MLDSA_private_key_from_seed(priv_bad, seed, seed_len - 1)) {
		warnx("MLDSA_private_key_from_seed accepted a short seed");
		failed |= 1;
	}

	/* Sign and verify round trip. */
	if (!MLDSA_sign(priv, message, sizeof(message), NULL, 0, &signature,
	    &signature_len)) {
		warnx("MLDSA_sign");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_verify(pub, signature, signature_len, message,
	    sizeof(message), NULL, 0)) {
		warnx("MLDSA_verify");
		failed |= 1;
	}

	/* A tampered signature must not verify. */
	signature[0] ^= 0x80;
	if (MLDSA_verify(pub, signature, signature_len, message,
	    sizeof(message), NULL, 0)) {
		warnx("MLDSA_verify accepted a tampered signature");
		failed |= 1;
	}
	signature[0] ^= 0x80;

	/* A different message must not verify. */
	if (MLDSA_verify(pub, signature, signature_len, message,
	    sizeof(message) - 1, NULL, 0)) {
		warnx("MLDSA_verify accepted the wrong message");
		failed |= 1;
	}

	/* An unexpected context must not verify. */
	if (MLDSA_verify(pub, signature, signature_len, message,
	    sizeof(message), context, sizeof(context))) {
		warnx("MLDSA_verify accepted an unexpected context");
		failed |= 1;
	}

	/* Signing and verifying with a context. */
	free(signature);
	signature = NULL;
	if (!MLDSA_sign(priv, message, sizeof(message), context,
	    sizeof(context), &signature, &signature_len)) {
		warnx("MLDSA_sign with context");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_verify(pub, signature, signature_len, message,
	    sizeof(message), context, sizeof(context))) {
		warnx("MLDSA_verify with context");
		failed |= 1;
	}
	if (MLDSA_verify(pub, signature, signature_len, message,
	    sizeof(message), NULL, 0)) {
		warnx("MLDSA_verify accepted a missing context");
		failed |= 1;
	}

	/* Signing is randomized: two signatures differ but both verify. */
	free(signature);
	signature = NULL;
	if (!MLDSA_sign(priv, message, sizeof(message), NULL, 0, &signature,
	    &signature_len)) {
		warnx("MLDSA_sign");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_sign(priv, message, sizeof(message), NULL, 0, &signature2,
	    &signature2_len)) {
		warnx("MLDSA_sign");
		failed |= 1;
		goto done;
	}
	if (signature_len == signature2_len &&
	    memcmp(signature, signature2, signature_len) == 0) {
		warnx("signatures are not randomized");
		failed |= 1;
	}
	if (!MLDSA_verify(pub, signature2, signature2_len, message,
	    sizeof(message), NULL, 0)) {
		warnx("MLDSA_verify second signature");
		failed |= 1;
	}

	/* Parsing the encoded public key and re-marshalling it round trips. */
	if ((pub_parsed = MLDSA_public_key_new(rank)) == NULL) {
		warnx("MLDSA_public_key_new");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_parse_public_key(pub_parsed, encoded_public_key,
	    encoded_public_key_len)) {
		warnx("MLDSA_parse_public_key");
		failed |= 1;
		goto done;
	}
	if (!MLDSA_marshal_public_key(pub_parsed, &marshaled, &marshaled_len)) {
		warnx("MLDSA_marshal_public_key (parsed)");
		failed |= 1;
		goto done;
	}
	if (marshaled_len != encoded_public_key_len ||
	    compare_data(encoded_public_key, marshaled, marshaled_len,
	    "parse/marshal") != 0) {
		warnx("parse/marshal public key mismatch");
		failed |= 1;
	}
	free(marshaled);
	marshaled = NULL;

	/* A public key of the wrong length must not parse. */
	MLDSA_public_key_free(pub_parsed);
	if ((pub_parsed = MLDSA_public_key_new(rank)) == NULL) {
		warnx("MLDSA_public_key_new");
		failed |= 1;
		goto done;
	}
	if (MLDSA_parse_public_key(pub_parsed, encoded_public_key,
	    encoded_public_key_len - 1)) {
		warnx("MLDSA_parse_public_key accepted a short input");
		failed |= 1;
	}

 done:
	MLDSA_private_key_free(priv);
	MLDSA_private_key_free(priv_seed);
	MLDSA_private_key_free(priv_bad);
	MLDSA_public_key_free(pub);
	MLDSA_public_key_free(pub_seed);
	MLDSA_public_key_free(pub_parsed);
	free(encoded_public_key);
	free(seed);
	free(marshaled);
	free(signature);
	free(signature2);

	return failed;
}

int
main(void)
{
	int failed = 0;

	failed |= MldsaUnitTest(MLDSA44_RANK);
	failed |= MldsaUnitTest(MLDSA65_RANK);
	failed |= MldsaUnitTest(MLDSA87_RANK);

	return failed;
}
