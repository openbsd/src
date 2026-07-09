/*	$OpenBSD$ */
/*
 * Copyright (c) 2026 Bob Beck <beck@obtuse.com>
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

#include <stdlib.h>
#include <string.h>

#include <openssl/mldsa.h>

#include "mldsa_internal.h"

static inline int
private_key_is_new(const MLDSA_private_key *key)
{
	return key != NULL &&
	    key->state == MLDSA_PRIVATE_KEY_UNINITIALIZED &&
	    (key->rank == MLDSA65_RANK || key->rank == MLDSA87_RANK);
}

static inline int
private_key_is_valid(const MLDSA_private_key *key)
{
	return key != NULL &&
	    key->state == MLDSA_PRIVATE_KEY_INITIALIZED &&
	    (key->rank == MLDSA65_RANK || key->rank == MLDSA87_RANK);
}

static inline int
public_key_is_new(const MLDSA_public_key *key)
{
	return key != NULL &&
	    key->state == MLDSA_PUBLIC_KEY_UNINITIALIZED &&
	    (key->rank == MLDSA65_RANK || key->rank == MLDSA87_RANK);
}

static inline int
public_key_is_valid(const MLDSA_public_key *key)
{
	return key != NULL &&
	    key->state == MLDSA_PUBLIC_KEY_INITIALIZED &&
	    (key->rank == MLDSA65_RANK || key->rank == MLDSA87_RANK);
}

int
MLDSA_generate_key(MLDSA_private_key *private_key,
    uint8_t **out_encoded_public_key, size_t *out_encoded_public_key_len,
    uint8_t **out_optional_seed, size_t *out_optional_seed_len)
{
	uint8_t *seed = NULL;
	int ret = 0;

	if (*out_encoded_public_key != NULL)
		goto err;
	if (out_optional_seed != NULL && *out_optional_seed != NULL)
		goto err;
	if (!private_key_is_new(private_key))
		goto err;

	if ((seed = calloc(1, MLDSA_SEED_LENGTH)) == NULL)
		goto err;
	arc4random_buf(seed, MLDSA_SEED_LENGTH);

	if (!mldsa_generate_key_external_entropy(private_key,
	    out_encoded_public_key, out_encoded_public_key_len, seed))
		goto err;

	private_key->state = MLDSA_PRIVATE_KEY_INITIALIZED;

	if (out_optional_seed != NULL) {
		*out_optional_seed = seed;
		*out_optional_seed_len = MLDSA_SEED_LENGTH;
		seed = NULL;
	}

	ret = 1;

 err:
	freezero(seed, MLDSA_SEED_LENGTH);

	return ret;
}
LCRYPTO_ALIAS(MLDSA_generate_key);

int
MLDSA_private_key_from_seed(MLDSA_private_key *private_key, const uint8_t *seed,
    size_t seed_len)
{
	if (!private_key_is_new(private_key))
		return 0;
	if (seed_len != MLDSA_SEED_LENGTH)
		return 0;

	if (!mldsa_private_key_from_seed(seed, seed_len, private_key))
		return 0;

	private_key->state = MLDSA_PRIVATE_KEY_INITIALIZED;

	return 1;
}
LCRYPTO_ALIAS(MLDSA_private_key_from_seed);

int
MLDSA_public_from_private(const MLDSA_private_key *private_key,
    MLDSA_public_key *public_key)
{
	if (!private_key_is_valid(private_key))
		return 0;
	if (!public_key_is_new(public_key))
		return 0;
	if (public_key->rank != private_key->rank)
		return 0;

	if (!mldsa_public_from_private(private_key, public_key))
		return 0;

	public_key->state = MLDSA_PUBLIC_KEY_INITIALIZED;

	return 1;
}
LCRYPTO_ALIAS(MLDSA_public_from_private);

int
MLDSA_sign(const MLDSA_private_key *private_key, const uint8_t *message,
    size_t message_len, const uint8_t *optional_context,
    size_t optional_context_len, uint8_t **out_signature,
    size_t *out_signature_len)
{
	if (*out_signature != NULL)
		return 0;
	if (!private_key_is_valid(private_key))
		return 0;

	return mldsa_sign(private_key, message, message_len, optional_context,
	    optional_context_len, out_signature, out_signature_len);
}
LCRYPTO_ALIAS(MLDSA_sign);

int
MLDSA_verify(const MLDSA_public_key *public_key, const uint8_t *signature,
    size_t signature_len, const uint8_t *message, size_t message_len,
    const uint8_t *context, size_t context_len)
{
	if (!public_key_is_valid(public_key))
		return 0;

	return mldsa_verify(public_key, signature, signature_len, message,
	    message_len, context, context_len);
}
LCRYPTO_ALIAS(MLDSA_verify);

int
MLDSA_marshal_public_key(const MLDSA_public_key *public_key, uint8_t **out,
    size_t *out_len)
{
	if (*out != NULL)
		return 0;
	if (!public_key_is_valid(public_key))
		return 0;

	return mldsa_marshal_public_key(public_key, out, out_len);
}
LCRYPTO_ALIAS(MLDSA_marshal_public_key);

int
MLDSA_parse_public_key(MLDSA_public_key *public_key, const uint8_t *in,
    size_t in_len)
{
	if (!public_key_is_new(public_key))
		return 0;

	if (!mldsa_parse_public_key(in, in_len, public_key))
		return 0;

	public_key->state = MLDSA_PUBLIC_KEY_INITIALIZED;

	return 1;
}
LCRYPTO_ALIAS(MLDSA_parse_public_key);

int
MLDSA_parse_private_key(MLDSA_private_key *private_key, const uint8_t *in,
    size_t in_len)
{
	if (!private_key_is_new(private_key))
		return 0;

	if (!mldsa_parse_private_key(in, in_len, private_key))
		return 0;

	private_key->state = MLDSA_PRIVATE_KEY_INITIALIZED;

	return 1;
}
LCRYPTO_ALIAS(MLDSA_parse_private_key);
