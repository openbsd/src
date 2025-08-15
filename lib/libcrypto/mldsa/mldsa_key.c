/* $OpenBSD$ */
/*
 * Copyright (c) 2025 Bob Beck <beck@obtuse.com>
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

MLDSA_private_key *
MLDSA_private_key_new(int rank)
{
	struct MLDSA87_private_key *key_87 = NULL;
	struct MLDSA65_private_key *key_65 = NULL;
	MLDSA_private_key *key = NULL;
	MLDSA_private_key *ret = NULL;

	if ((key = calloc(1, sizeof(MLDSA_private_key))) == NULL)
		goto err;

	switch (rank) {
	case MLDSA65_RANK:
		if ((key_65 = calloc(1, sizeof(*key_65))) == NULL)
			goto err;
		key->key_65 = key_65;
		break;
	case MLDSA87_RANK:
		if ((key_87 = calloc(1, sizeof(*key_87))) == NULL)
			goto err;
		key->key_87 = key_87;
		break;
	default:
		goto err;
	}
	key->rank = rank;
	key->state = MLDSA_PRIVATE_KEY_UNINITIALIZED;

	ret = key;
	key = NULL;

 err:
	MLDSA_private_key_free(key);

	return ret;
}
LCRYPTO_ALIAS(MLDSA_private_key_new);

void
MLDSA_private_key_free(MLDSA_private_key *key)
{
	if (key == NULL)
		return;

	freezero(key->key_65, sizeof(*key->key_65));
	freezero(key->key_87, sizeof(*key->key_87));
	freezero(key, sizeof(*key));
}
LCRYPTO_ALIAS(MLDSA_private_key_free);

MLDSA_public_key *
MLDSA_public_key_new(int rank)
{
	struct MLDSA87_public_key *key_87 = NULL;
	struct MLDSA65_public_key *key_65 = NULL;
	MLDSA_public_key *key = NULL;
	MLDSA_public_key *ret = NULL;

	if ((key = calloc(1, sizeof(MLDSA_public_key))) == NULL)
		goto err;

	switch (rank) {
	case MLDSA65_RANK:
		if ((key_65 = calloc(1, sizeof(*key_65))) == NULL)
			goto err;
		key->key_65 = key_65;
		break;
	case MLDSA87_RANK:
		if ((key_87 = calloc(1, sizeof(*key_87))) == NULL)
			goto err;
		key->key_87 = key_87;
		break;
	default:
		goto err;
	}

	key->rank = rank;
	key->state = MLDSA_PUBLIC_KEY_UNINITIALIZED;

	ret = key;
	key = NULL;

 err:
	MLDSA_public_key_free(key);

	return ret;
}
LCRYPTO_ALIAS(MLDSA_public_key_new);

void
MLDSA_public_key_free(MLDSA_public_key *key)
{
	if (key == NULL)
		return;

	freezero(key->key_65, sizeof(*key->key_65));
	freezero(key->key_87, sizeof(*key->key_87));
	freezero(key, sizeof(*key));
}
LCRYPTO_ALIAS(MLDSA_public_key_free);
