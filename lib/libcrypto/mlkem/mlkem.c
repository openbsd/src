/*	$OpenBSD: mlkem.c,v 1.1 2025/08/14 15:48:48 beck Exp $ */
/*
 * Copyright (c) 2025, Bob Beck <beck@obtuse.com>
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
#include <stdlib.h>

#include <openssl/mlkem.h>
#include "mlkem_internal.h"

static inline int
private_key_is_new(const MLKEM_private_key *key)
{
	return (key != NULL &&
	    key->state == MLKEM_PRIVATE_KEY_UNINITIALIZED &&
	    (key->rank == RANK768 || key->rank == RANK1024));
}

static inline int
private_key_is_valid(const MLKEM_private_key *key)
{
	return (key != NULL &&
	    key->state == MLKEM_PRIVATE_KEY_INITIALIZED &&
	    (key->rank == RANK768 || key->rank == RANK1024));
}

static inline int
public_key_is_new(const MLKEM_public_key *key)
{
	return (key != NULL &&
	    key->state == MLKEM_PUBLIC_KEY_UNINITIALIZED &&
	    (key->rank == RANK768 || key->rank == RANK1024));
}

static inline int
public_key_is_valid(const MLKEM_public_key *key)
{
	return (key != NULL &&
	    key->state == MLKEM_PUBLIC_KEY_INITIALIZED &&
	    (key->rank == RANK768 || key->rank == RANK1024));
}

/*
 * ML-KEM operations
 */

int
MLKEM_generate_key_external_entropy(MLKEM_private_key *private_key,
    uint8_t **out_encoded_public_key, size_t *out_encoded_public_key_len,
    const uint8_t *entropy)
{
	uint8_t *k = NULL;
	size_t k_len = 0;
	int ret = 0;

	if (*out_encoded_public_key != NULL)
		goto err;

	if (!private_key_is_new(private_key))
		goto err;

	k_len = MLKEM768_PUBLIC_KEY_BYTES;
	if (private_key->rank == RANK1024)
		k_len = MLKEM1024_PUBLIC_KEY_BYTES;

	if ((k = calloc(1, k_len)) == NULL)
		goto err;

	switch (private_key->rank) {
	case RANK768:
		if (!MLKEM768_generate_key_external_entropy(k, private_key,
		    entropy))
			goto err;
		break;
	case RANK1024:
		if (!MLKEM1024_generate_key_external_entropy(k, private_key,
		    entropy))
			goto err;
		break;
	}

	private_key->state = MLKEM_PRIVATE_KEY_INITIALIZED;

	*out_encoded_public_key = k;
	*out_encoded_public_key_len = k_len;
	k = NULL;

	ret = 1;

 err:
	freezero(k, k_len);

	return ret;
}

int
MLKEM_generate_key(MLKEM_private_key *private_key,
    uint8_t **out_encoded_public_key, size_t *out_encoded_public_key_len,
    uint8_t **out_optional_seed, size_t *out_optional_seed_len)
{
	uint8_t *entropy_buf = NULL;
	int ret = 0;

	if (*out_encoded_public_key != NULL)
		goto err;

	if (out_optional_seed != NULL && *out_optional_seed != NULL)
		goto err;

	if ((entropy_buf = calloc(1, MLKEM_SEED_LENGTH)) == NULL)
		goto err;

	arc4random_buf(entropy_buf, MLKEM_SEED_LENGTH);
	if (!MLKEM_generate_key_external_entropy(private_key,
	    out_encoded_public_key, out_encoded_public_key_len,
	    entropy_buf))
		goto err;

	if (out_optional_seed != NULL) {
		*out_optional_seed = entropy_buf;
		*out_optional_seed_len = MLKEM_SEED_LENGTH;
		entropy_buf = NULL;
	}

	ret = 1;

 err:
	freezero(entropy_buf, MLKEM_SEED_LENGTH);

	return ret;
}
LCRYPTO_ALIAS(MLKEM_generate_key);

int
MLKEM_private_key_from_seed(MLKEM_private_key *private_key,
    const uint8_t *seed, size_t seed_len)
{
	int ret = 0;

	if (!private_key_is_new(private_key))
		goto err;

	if (seed_len != MLKEM_SEED_LENGTH)
		goto err;

	switch (private_key->rank) {
	case RANK768:
		if (!MLKEM768_private_key_from_seed(seed,
		    seed_len, private_key))
			goto err;
		break;
	case RANK1024:
		if (!MLKEM1024_private_key_from_seed(private_key,
		    seed, seed_len))
			goto err;
		break;
	}

	private_key->state = MLKEM_PRIVATE_KEY_INITIALIZED;

	ret = 1;

 err:

	return ret;
}
LCRYPTO_ALIAS(MLKEM_private_key_from_seed);

int
MLKEM_public_from_private(const MLKEM_private_key *private_key,
    MLKEM_public_key *public_key)
{
	if (!private_key_is_valid(private_key))
		return 0;
	if (!public_key_is_new(public_key))
		return 0;
	if (public_key->rank != private_key->rank)
		return 0;
	switch (private_key->rank) {
	case RANK768:
		MLKEM768_public_from_private(private_key, public_key);
		break;
	case RANK1024:
		MLKEM1024_public_from_private(private_key, public_key);
		break;
	}

	public_key->state = MLKEM_PUBLIC_KEY_INITIALIZED;

	return 1;
}
LCRYPTO_ALIAS(MLKEM_public_from_private);

int
MLKEM_encap_external_entropy(const MLKEM_public_key *public_key,
    const uint8_t *entropy, uint8_t **out_ciphertext,
    size_t *out_ciphertext_len, uint8_t **out_shared_secret,
    size_t *out_shared_secret_len)
{
	uint8_t *secret = NULL;
	uint8_t *ciphertext = NULL;
	size_t ciphertext_len = 0;
	int ret = 0;

	if (*out_ciphertext != NULL)
		goto err;

	if (*out_shared_secret != NULL)
		goto err;

	if (!public_key_is_valid(public_key))
		goto err;

	if ((secret = calloc(1, MLKEM_SHARED_SECRET_LENGTH)) == NULL)
		goto err;

	ciphertext_len = MLKEM_public_key_ciphertext_length(public_key);

	if ((ciphertext = calloc(1, ciphertext_len)) == NULL)
		goto err;

	switch (public_key->rank) {
	case RANK768:
		MLKEM768_encap_external_entropy(ciphertext, secret, public_key,
		    entropy);
		break;

	case RANK1024:
		MLKEM1024_encap_external_entropy(ciphertext, secret, public_key,
		    entropy);
		break;
	}
	*out_ciphertext = ciphertext;
	*out_ciphertext_len = ciphertext_len;
	ciphertext = NULL;
	*out_shared_secret = secret;
	*out_shared_secret_len = MLKEM_SHARED_SECRET_LENGTH;
	secret = NULL;

	ret = 1;

 err:
	freezero(secret, MLKEM_SHARED_SECRET_LENGTH);
	freezero(ciphertext, ciphertext_len);

	return ret;
}

int
MLKEM_encap(const MLKEM_public_key *public_key,
    uint8_t **out_ciphertext, size_t *out_ciphertext_len,
    uint8_t **out_shared_secret, size_t *out_shared_secret_len)
{
	uint8_t entropy[MLKEM_ENCAP_ENTROPY];

	arc4random_buf(entropy, MLKEM_ENCAP_ENTROPY);

	return MLKEM_encap_external_entropy(public_key, entropy, out_ciphertext,
	    out_ciphertext_len, out_shared_secret, out_shared_secret_len);
}
LCRYPTO_ALIAS(MLKEM_encap);

int
MLKEM_decap(const MLKEM_private_key *private_key,
    const uint8_t *ciphertext, size_t ciphertext_len,
    uint8_t **out_shared_secret, size_t *out_shared_secret_len)
{
	uint8_t *s = NULL;
	int ret = 0;

	if (*out_shared_secret != NULL)
		goto err;

	if (!private_key_is_valid(private_key))
		goto err;

	if (ciphertext_len != MLKEM_private_key_ciphertext_length(private_key))
		goto err;

	if ((s = calloc(1, MLKEM_SHARED_SECRET_LENGTH)) == NULL)
		goto err;

	switch (private_key->rank) {
	case RANK768:
		MLKEM768_decap(private_key, ciphertext, ciphertext_len, s);
		break;

	case RANK1024:
		MLKEM1024_decap(private_key, ciphertext, ciphertext_len, s);
		break;
	}

	*out_shared_secret = s;
	*out_shared_secret_len = MLKEM_SHARED_SECRET_LENGTH;
	s = NULL;

	ret = 1;

 err:
	freezero(s, MLKEM_SHARED_SECRET_LENGTH);

	return ret;
}
LCRYPTO_ALIAS(MLKEM_decap);

int
MLKEM_marshal_public_key(const MLKEM_public_key *public_key, uint8_t **out,
    size_t *out_len)
{
	if (*out != NULL)
		return 0;

	if (!public_key_is_valid(public_key))
		return 0;

	switch (public_key->rank) {
	case RANK768:
		return MLKEM768_marshal_public_key(public_key, out, out_len);
	case RANK1024:
		return MLKEM1024_marshal_public_key(public_key, out, out_len);
	default:
		return 0;
	}
}
LCRYPTO_ALIAS(MLKEM_marshal_public_key);

/*
 * Not exposed publicly, becuase the NIST private key format is gigantisch, and
 * seeds should be used instead.  Used for the NIST tests.
 */
int
MLKEM_marshal_private_key(const MLKEM_private_key *private_key, uint8_t **out,
    size_t *out_len)
{
	if (*out != NULL)
		return 0;

	if (!private_key_is_valid(private_key))
		return 0;

	switch (private_key->rank) {
	case RANK768:
		return MLKEM768_marshal_private_key(private_key, out, out_len);
	case RANK1024:
		return MLKEM1024_marshal_private_key(private_key, out, out_len);
	default:
		return 0;
	}
}

int
MLKEM_parse_public_key(MLKEM_public_key *public_key, const uint8_t *in,
    size_t in_len)
{
	if (!public_key_is_new(public_key))
		return 0;

	if (in_len != MLKEM_public_key_encoded_length(public_key))
		return 0;

	switch (public_key->rank) {
	case RANK768:
		if (!MLKEM768_parse_public_key(in, in_len,
		    public_key))
			return 0;
		break;
	case RANK1024:
		if (!MLKEM1024_parse_public_key(in, in_len,
		    public_key))
			return 0;
		break;
	}

	public_key->state = MLKEM_PUBLIC_KEY_INITIALIZED;

	return 1;
}
LCRYPTO_ALIAS(MLKEM_parse_public_key);

int
MLKEM_parse_private_key(MLKEM_private_key *private_key, const uint8_t *in,
    size_t in_len)
{
	if (!private_key_is_new(private_key))
		return 0;

	if (in_len != MLKEM_private_key_encoded_length(private_key))
		return 0;

	switch (private_key->rank) {
	case RANK768:
		if (!MLKEM768_parse_private_key(in, in_len, private_key))
			return 0;
		break;
	case RANK1024:
		if (!MLKEM1024_parse_private_key(in, in_len, private_key))
			return 0;
		break;
	}

	private_key->state = MLKEM_PRIVATE_KEY_INITIALIZED;

	return 1;
}
LCRYPTO_ALIAS(MLKEM_parse_private_key);
/*	$OpenBSD: mlkem.c,v 1.1 2025/08/14 15:48:48 beck Exp $ */
/*
 * Copyright (c) 2025, Bob Beck <beck@obtuse.com>
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

#include <openssl/mlkem.h>


int MLKEM768_generate_key(
    uint8_t out_encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[MLKEM_SEED_BYTES],
    struct MLKEM768_private_key *out_private_key);

/*
 * MLKEM768_private_key_from_seed derives a private key from a seed that was
 * generated by |MLKEM768_generate_key|. It fails and returns 0 if |seed_len| is
 * incorrect, otherwise it writes |*out_private_key| and returns 1.
 */
int MLKEM768_private_key_from_seed(struct MLKEM768_private_key *out_private_key,
    const uint8_t *seed, size_t seed_len);

/*
 * MLKEM_public_from_private sets |*out_public_key| to the public key that
 * corresponds to |private_key|. (This is faster than parsing the output of
 * |MLKEM_generate_key| if, for some reason, you need to encapsulate to a key
 * that was just generated.)
 */
void MLKEM768_public_from_private(struct MLKEM768_public_key *out_public_key,
    const struct MLKEM768_private_key *private_key);

/* MLKEM768_CIPHERTEXT_BYTES is number of bytes in the ML-KEM768 ciphertext. */
#define MLKEM768_CIPHERTEXT_BYTES 1088

/*
 * MLKEM768_encap encrypts a random shared secret for |public_key|, writes the
 * ciphertext to |out_ciphertext|, and writes the random shared secret to
 * |out_shared_secret|.
 */
void MLKEM768_encap(uint8_t out_ciphertext[MLKEM768_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const struct MLKEM768_public_key *public_key);

/*
 * MLKEM768_decap decrypts a shared secret from |ciphertext| using |private_key|
 * and writes it to |out_shared_secret|. If |ciphertext_len| is incorrect it
 * returns 0, otherwise it rreturns 1. If |ciphertext| is invalid,
 * |out_shared_secret| is filled with a key that will always be the same for the
 * same |ciphertext| and |private_key|, but which appears to be random unless
 * one has access to |private_key|. These alternatives occur in constant time.
 * Any subsequent symmetric encryption using |out_shared_secret| must use an
 * authenticated encryption scheme in order to discover the decapsulation
 * failure.
 */
int MLKEM768_decap(uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len,
    const struct MLKEM768_private_key *private_key);

/* Serialisation of keys. */

/*
 * MLKEM768_marshal_public_key serializes |public_key| to |out| in the standard
 * format for ML-KEM public keys. It returns one on success or zero on allocation
 * error.
 */
int MLKEM768_marshal_public_key(uint8_t **output, size_t *output_len,
    const struct MLKEM768_public_key *public_key);

/*
 * MLKEM768_parse_public_key parses a public key, in the format generated by
 * |MLKEM_marshal_public_key|, from |in| and writes the result to
 * |out_public_key|. It returns one on success or zero on parse error or if
 * there are trailing bytes in |in|.
 */
int MLKEM768_parse_public_key(struct MLKEM768_public_key *out_public_key,
    const uint8_t *input, size_t input_len);

/*
 * MLKEM_parse_private_key parses a private key, in the format generated by
 * |MLKEM_marshal_private_key|, from |in| and writes the result to
 * |out_private_key|. It returns one on success or zero on parse error or if
 * there are trailing bytes in |in|. This formate is verbose and should be avoided.
 * Private keys should be stored as seeds and parsed using |MLKEM768_private_key_from_seed|.
 */
int MLKEM768_parse_private_key(struct MLKEM768_private_key *out_private_key,
    const uint8_t *input, size_t input_len);

/*
 * ML-KEM-1024
 *
 * ML-KEM-1024 also exists. You should prefer ML-KEM-768 where possible.
 */

/*
 * MLKEM1024_public_key contains an ML-KEM-1024 public key. The contents of this
 * object should never leave the address space since the format is unstable.
 */
struct MLKEM1024_public_key {
	union {
		uint8_t bytes[512 * (4 + 16) + 32 + 32];
		uint16_t alignment;
	} opaque;
};

/*
 * MLKEM1024_private_key contains a ML-KEM-1024 private key. The contents of
 * this object should never leave the address space since the format is
 * unstable.
 */
struct MLKEM1024_private_key {
	union {
		uint8_t bytes[512 * (4 + 4 + 16) + 32 + 32 + 32];
		uint16_t alignment;
	} opaque;
};

/*
 * MLKEM1024_PUBLIC_KEY_BYTES is the number of bytes in an encoded ML-KEM-1024
 * public key.
 */
#define MLKEM1024_PUBLIC_KEY_BYTES 1568

/*
 * MLKEM1024_generate_key generates a random public/private key pair, writes the
 * encoded public key to |out_encoded_public_key| and sets |out_private_key| to
 * the private key. If |optional_out_seed| is not NULL then the seed used to
 * generate the private key is written to it.
 */
int MLKEM1024_generate_key(
    uint8_t out_encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[MLKEM_SEED_BYTES],
    struct MLKEM1024_private_key *out_private_key);

/*
 * MLKEM1024_private_key_from_seed derives a private key from a seed that was
 * generated by |MLKEM1024_generate_key|. It fails and returns 0 if |seed_len|
 * is incorrect, otherwise it writes |*out_private_key| and returns 1.
 */
int MLKEM1024_private_key_from_seed(
    struct MLKEM1024_private_key *out_private_key, const uint8_t *seed,
    size_t seed_len);

/*
 * MLKEM1024_public_from_private sets |*out_public_key| to the public key that
 * corresponds to |private_key|. (This is faster than parsing the output of
 * |MLKEM1024_generate_key| if, for some reason, you need to encapsulate to a
 * key that was just generated.)
 */
void MLKEM1024_public_from_private(struct MLKEM1024_public_key *out_public_key,
    const struct MLKEM1024_private_key *private_key);

/* MLKEM1024_CIPHERTEXT_BYTES is number of bytes in the ML-KEM-1024 ciphertext. */
#define MLKEM1024_CIPHERTEXT_BYTES 1568

/*
 * MLKEM1024_encap encrypts a random shared secret for |public_key|, writes the
 * ciphertext to |out_ciphertext|, and writes the random shared secret to
 * |out_shared_secret|.
 */
void MLKEM1024_encap(uint8_t out_ciphertext[MLKEM1024_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const struct MLKEM1024_public_key *public_key);

/*
 * MLKEM1024_decap decrypts a shared secret from |ciphertext| using
 * |private_key| and writes it to |out_shared_secret|. If |ciphertext_len| is
 * incorrect it returns 0, otherwise it returns 1. If |ciphertext| is invalid
 * (but of the correct length), |out_shared_secret| is filled with a key that
 * will always be the same for the same |ciphertext| and |private_key|, but
 * which appears to be random unless one has access to |private_key|. These
 * alternatives occur in constant time. Any subsequent symmetric encryption
 * using |out_shared_secret| must use an authenticated encryption scheme in
 * order to discover the decapsulation failure.
 */
int MLKEM1024_decap(uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len,
    const struct MLKEM1024_private_key *private_key);

/*
 * Serialisation of ML-KEM-1024 keys.
 * MLKEM1024_marshal_public_key serializes |public_key| to |out| in the standard
 * format for ML-KEM-1024 public keys. It returns one on success or zero on
 * allocation error.
 */
int MLKEM1024_marshal_public_key(uint8_t **output, size_t *output_len,
    const struct MLKEM1024_public_key *public_key);

/*
 * MLKEM1024_parse_public_key parses a public key, in the format generated by
 * |MLKEM1024_marshal_public_key|, from |in| and writes the result to
 * |out_public_key|. It returns one on success or zero on parse error or if
 * there are trailing bytes in |in|.
 */
int MLKEM1024_parse_public_key(struct MLKEM1024_public_key *out_public_key,
    const uint8_t *input, size_t input_len);

/*
 * MLKEM1024_parse_private_key parses a private key, in NIST's format for
 * private keys, from |in| and writes the result to |out_private_key|. It
 * returns one on success or zero on parse error or if there are trailing bytes
 * in |in|. This format is verbose and should be avoided. Private keys should be
 * stored as seeds and parsed using |MLKEM1024_private_key_from_seed|.
 */
int MLKEM1024_parse_private_key(struct MLKEM1024_private_key *out_private_key,
    const uint8_t *input, size_t input_len);

#if defined(__cplusplus)
}
#endif

#endif  /* OPENSSL_HEADER_MLKEM_H */
