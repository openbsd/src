/* Copyright (c) 2024, Google Inc.
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
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bytestring.h"
#include "mlkem.h"
#include "mlkem_internal.h"
#include "mlkem_tests_util.h"

static int
encode_public_key(const struct MLKEM768_public_key *pub, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	if (!CBB_init(&cbb, MLKEM768_PUBLIC_KEY_BYTES))
		return 0;
	if (!MLKEM768_marshal_public_key(&cbb, pub))
		return 0;
	if (!CBB_finish(&cbb, out_buf, out_len))
		return 0;
	CBB_cleanup(&cbb);
	return 1;
}

static int
encode_private_key(const struct MLKEM768_private_key *priv, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	if (!CBB_init(&cbb, MLKEM768_PUBLIC_KEY_BYTES))
		return 0;
	if (!MLKEM768_marshal_private_key(&cbb, priv))
		return 0;
	if (!CBB_finish(&cbb, out_buf, out_len))
		return 0;
	CBB_cleanup(&cbb);
	return 1;
}

int
main(int argc, char **argv)
{
	struct MLKEM768_private_key *priv, *priv2;
	struct MLKEM768_public_key *pub, *pub2;
	uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
	uint8_t ciphertext[MLKEM768_CIPHERTEXT_BYTES];
	uint8_t shared_secret1[MLKEM_SHARED_SECRET_BYTES];
	uint8_t shared_secret2[MLKEM_SHARED_SECRET_BYTES];
	uint8_t first_two_bytes[2];
	uint8_t *encoded_private_key = NULL, *tmp_buf = NULL;
	size_t encoded_private_key_len, tmp_buf_len;
	CBS cbs;

	fprintf(stderr, "ML-KEM 768...\n");

	MALLOC(priv, sizeof(struct MLKEM768_private_key));
	MLKEM768_generate_key(encoded_public_key, NULL, priv);

	memcpy(first_two_bytes, encoded_public_key, sizeof(first_two_bytes));
	memset(encoded_public_key, 0xff, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_public_key,
	    sizeof(encoded_public_key));
	MALLOC(pub, sizeof(struct MLKEM768_public_key));
	/*  Parsing should fail because the first coefficient is >= kPrime; */
	TEST(MLKEM768_parse_public_key(pub, &cbs),
	    "Kyber_parse_public_key should have failed");

	memcpy(encoded_public_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_public_key, sizeof(encoded_public_key));
	TEST(!MLKEM768_parse_public_key(pub, &cbs),
	    "MLKEM768_parse_public_key");
	TEST(CBS_len(&cbs) != 0u, "CBS_len must be 0");

	TEST(!encode_public_key(pub, &tmp_buf, &tmp_buf_len),
	    "encode_public_key");
	TEST(sizeof(encoded_public_key) != tmp_buf_len,
	    "encoded public key lengths differ");
	TEST_DATAEQ(tmp_buf, encoded_public_key, tmp_buf_len,
	    "encoded public keys");
	free(tmp_buf);
	tmp_buf = NULL;

	MALLOC(pub2, sizeof(struct MLKEM768_public_key));
	MLKEM768_public_from_private(pub2, priv);
	TEST(!encode_public_key(pub2, &tmp_buf, &tmp_buf_len),
	    "encode_public_key");
	TEST(sizeof(encoded_public_key) != tmp_buf_len,
	    "encoded public key lengths differ");
	TEST_DATAEQ(tmp_buf, encoded_public_key, tmp_buf_len,
	    "encoded pubic keys");
	free(tmp_buf);
	tmp_buf = NULL;

	TEST(!encode_private_key(priv, &encoded_private_key,
	    &encoded_private_key_len), "encode_private_key");

	memcpy(first_two_bytes, encoded_private_key, sizeof(first_two_bytes));
	memset(encoded_private_key, 0xff, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);
	MALLOC(priv2, sizeof(struct MLKEM768_private_key));
	/*  Parsing should fail because the first coefficient is >= kPrime. */
	TEST(MLKEM768_parse_private_key(priv2, &cbs), "Should not have parsed");

	memcpy(encoded_private_key, first_two_bytes, sizeof(first_two_bytes));
	CBS_init(&cbs, encoded_private_key, encoded_private_key_len);
	TEST(!MLKEM768_parse_private_key(priv2, &cbs),
	    "MLKEM768_parse_private_key");
	TEST(!encode_private_key(priv2, &tmp_buf, &tmp_buf_len),
	    "encode_private_key");
	TEST(encoded_private_key_len != tmp_buf_len,
	    "encoded private key lengths differ");
	TEST_DATAEQ(tmp_buf, encoded_private_key, encoded_private_key_len,
	    "encoded private keys");
	free(tmp_buf);
	tmp_buf = NULL;

	MLKEM768_encap(ciphertext, shared_secret1, pub);
	MLKEM768_decap(shared_secret2, ciphertext, MLKEM768_CIPHERTEXT_BYTES,
	    priv);
	TEST_DATAEQ(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    "shared secrets with priv");
	MLKEM768_decap(shared_secret2, ciphertext, MLKEM768_CIPHERTEXT_BYTES,
	    priv2);
	TEST_DATAEQ(shared_secret1, shared_secret2, MLKEM_SHARED_SECRET_BYTES,
	    "shared secrets with priv2");

	free(encoded_private_key);
	free(pub);
	free(pub2);
	free(priv);
	free(priv2);

	exit(failure);
}
