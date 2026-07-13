/*	$OpenBSD$ */
/* Copyright 2024 The BoringSSL Authors
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
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef OPENSSL_HEADER_CRYPTO_MLDSA_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_MLDSA_INTERNAL_H

#include <openssl/mldsa.h>

#include "bytestring.h"

#if defined(__cplusplus)
extern "C" {
#endif

__BEGIN_HIDDEN_DECLS

/* Public opaque ML-DSA key structures. */

#define MLDSA_PUBLIC_KEY_UNINITIALIZED 1
#define MLDSA_PUBLIC_KEY_INITIALIZED 2
#define MLDSA_PRIVATE_KEY_UNINITIALIZED 3
#define MLDSA_PRIVATE_KEY_INITIALIZED 4

struct MLDSA_public_key_st {
	uint16_t rank;
	int state;
	struct MLDSA44_public_key *key_44;
	struct MLDSA65_public_key *key_65;
	struct MLDSA87_public_key *key_87;
};

struct MLDSA_private_key_st {
	uint16_t rank;
	int state;
	struct MLDSA44_private_key *key_44;
	struct MLDSA65_private_key *key_65;
	struct MLDSA87_private_key *key_87;
};

/*
 * ML-DSA-65 and ML-DSA-87
 *
 * This implements the Module-Lattice-Based Digital Signature Standard from
 * https://csrc.nist.gov/pubs/fips/204/final
 *
 * You should prefer ML-DSA-65 where possible. ML-DSA-87 is larger and exists
 * for those with a requirement for a higher security category.
 */

/*
 * MLDSA44_public_key contains an ML-DSA-44 public key. The contents of this
 * object should never leave the address space since the format is unstable.
 */
struct MLDSA44_public_key {
	uint8_t bytes[32 + 64 + 256 * 4 * 4];
	uint32_t alignment;
};

/*
 * MLDSA44_private_key contains an ML-DSA-44 private key. The contents of this
 * object should never leave the address space since the format is unstable.
 */
struct MLDSA44_private_key {
	uint8_t bytes[32 + 32 + 64 + 256 * 4 * (4 + 4 + 4)];
	uint32_t alignment;
};

/*
 * MLDSA65_public_key contains an ML-DSA-65 public key. The contents of this
 * object should never leave the address space since the format is unstable.
 */
struct MLDSA65_public_key {
	uint8_t bytes[32 + 64 + 256 * 4 * 6];
	uint32_t alignment;
};

/*
 * MLDSA65_private_key contains an ML-DSA-65 private key. The contents of this
 * object should never leave the address space since the format is unstable.
 */
struct MLDSA65_private_key {
	uint8_t bytes[32 + 32 + 64 + 256 * 4 * (5 + 6 + 6)];
	uint32_t alignment;
};

/*
 * MLDSA87_public_key contains an ML-DSA-87 public key. The contents of this
 * object should never leave the address space since the format is unstable.
 */
struct MLDSA87_public_key {
	uint8_t bytes[32 + 64 + 256 * 4 * 8];
	uint32_t alignment;
};

/*
 * MLDSA87_private_key contains an ML-DSA-87 private key. The contents of this
 * object should never leave the address space since the format is unstable.
 */
struct MLDSA87_private_key {
	uint8_t bytes[32 + 32 + 64 + 256 * 4 * (7 + 8 + 8)];
	uint32_t alignment;
};

/*
 * MLDSA_SEED_LENGTH is the number of bytes in an ML-DSA seed value. An ML-DSA
 * seed is normally used to represent a private key.
 */
#define MLDSA_SEED_LENGTH 32

/*
 * MLDSA_SIGNATURE_RANDOMIZER_LENGTH is the number of bytes of randomizer used
 * when signing in the (default) randomized mode.
 */
#define MLDSA_SIGNATURE_RANDOMIZER_LENGTH 32

/*
 * MLDSA44_PUBLIC_KEY_BYTES is the number of bytes in an encoded ML-DSA-44
 * public key.
 */
#define MLDSA44_PUBLIC_KEY_BYTES 1312

/*
 * MLDSA44_PRIVATE_KEY_BYTES is the number of bytes in an encoded ML-DSA-44
 * private key.
 */
#define MLDSA44_PRIVATE_KEY_BYTES 2560

/*
 * MLDSA44_SIGNATURE_BYTES is the number of bytes in an encoded ML-DSA-44
 * signature.
 */
#define MLDSA44_SIGNATURE_BYTES 2420

/*
 * MLDSA65_PUBLIC_KEY_BYTES is the number of bytes in an encoded ML-DSA-65
 * public key.
 */
#define MLDSA65_PUBLIC_KEY_BYTES 1952

/*
 * MLDSA65_PRIVATE_KEY_BYTES is the number of bytes in an encoded ML-DSA-65
 * private key.
 */
#define MLDSA65_PRIVATE_KEY_BYTES 4032

/*
 * MLDSA65_SIGNATURE_BYTES is the number of bytes in an encoded ML-DSA-65
 * signature.
 */
#define MLDSA65_SIGNATURE_BYTES 3309

/*
 * MLDSA87_PUBLIC_KEY_BYTES is the number of bytes in an encoded ML-DSA-87
 * public key.
 */
#define MLDSA87_PUBLIC_KEY_BYTES 2592

/*
 * MLDSA87_PRIVATE_KEY_BYTES is the number of bytes in an encoded ML-DSA-87
 * private key.
 */
#define MLDSA87_PRIVATE_KEY_BYTES 4896

/*
 * MLDSA87_SIGNATURE_BYTES is the number of bytes in an encoded ML-DSA-87
 * signature.
 */
#define MLDSA87_SIGNATURE_BYTES 4627

/*
 * Internal ML-DSA-65 and ML-DSA-87 functions come largely from BoringSSL, but
 * converted to C from templated C++. Due to this history, most internal
 * functions do not allocate, and are expected to be handed memory allocated by
 * the caller. The caller is generally expected to know what sizes to allocate
 * based upon the rank of the key (either public or private) that they are
 * starting with. This avoids the need to handle memory allocation failures deep
 * in the implementation, as what is needed is allocated up front in the public
 * facing functions, and failure is handled there.
 */

/* Key generation. */

/*
 * mldsa_private_key_from_seed derives a private key into |out_private_key| of
 * the rank of |*out_private_key| from a seed that was generated by
 * |MLDSA_generate_key|. It fails and returns zero if |seed_len| is incorrect,
 * or if |*out_private_key| has not been initialized. Otherwise it writes to
 * |*out_private_key| and returns one.
 */
int mldsa_private_key_from_seed(const uint8_t *seed, size_t seed_len,
    MLDSA_private_key *out_private_key);

/*
 * mldsa_public_from_private sets |*out_public_key| to the public key that
 * corresponds to |*private_key|. It returns one on success and zero on failure.
 */
int mldsa_public_from_private(const MLDSA_private_key *private_key,
    MLDSA_public_key *out_public_key);

/* Signing and verification. */

/*
 * mldsa_sign generates a signature for |msg| of |msg_len| using |private_key|
 * (in the randomized mode) and the context |context| of |context_len|, writing
 * the newly allocated encoded signature to |*out_encoded_signature| and its
 * length to |*out_encoded_signature_len|. It returns one on success and zero on
 * failure. The caller is responsible for freeing |*out_encoded_signature|.
 */
int mldsa_sign(const MLDSA_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len,
    uint8_t **out_encoded_signature, size_t *out_encoded_signature_len);

/*
 * mldsa_verify verifies that |signature| of |signature_len| is a valid
 * signature for |msg| of |msg_len| using |public_key| and the context
 * |context| of |context_len|. It returns one on success and zero on failure.
 */
int mldsa_verify(const MLDSA_public_key *public_key, const uint8_t *signature,
    size_t signature_len, const uint8_t *msg, size_t msg_len,
    const uint8_t *context, size_t context_len);

/* Serialisation of keys. */

/*
 * mldsa_marshal_public_key serializes |public_key| to a newly allocated buffer
 * in |*output| of length |*output_len|, in the standard format for ML-DSA
 * public keys. It returns one on success or zero on failure. The caller is
 * responsible for freeing |*output|.
 */
int mldsa_marshal_public_key(const MLDSA_public_key *public_key,
    uint8_t **output, size_t *output_len);

/*
 * mldsa_parse_public_key parses a public key, in the format generated by
 * |mldsa_marshal_public_key|, from |input| of |input_len| and writes the result
 * to |out_public_key|. It returns one on success or zero on parse error or if
 * there are trailing bytes in |input|.
 */
int mldsa_parse_public_key(const uint8_t *input, size_t input_len,
    MLDSA_public_key *out_public_key);

/*
 * mldsa_marshal_private_key serializes |private_key| to a newly allocated
 * buffer in |*output| of length |*output_len|, in the standard format for
 * ML-DSA private keys. It returns one on success or zero on failure. The caller
 * is responsible for freeing |*output|. This format is verbose and should be
 * avoided; private keys should be stored as seeds and parsed using
 * |mldsa_private_key_from_seed|.
 */
int mldsa_marshal_private_key(const MLDSA_private_key *private_key,
    uint8_t **output, size_t *output_len);

/*
 * mldsa_parse_private_key parses a private key, in the format generated by
 * |mldsa_marshal_private_key|, from |input| of |input_len| and writes the
 * result to |out_private_key|. It returns one on success or zero on parse error
 * or if there are trailing bytes in |input|.
 */
int mldsa_parse_private_key(const uint8_t *input, size_t input_len,
    MLDSA_private_key *out_private_key);

/* Functions that are only used for test purposes. */

/*
 * mldsa_generate_key_external_entropy is a deterministic function to create a
 * pair of ML-DSA keys of the rank of |*out_private_key|, using the supplied
 * entropy. The entropy needs to be uniformly randomly generated. This function
 * should only be used for tests; regular callers should use the
 * non-deterministic |MLDSA_generate_key| directly.
 */
int mldsa_generate_key_external_entropy(MLDSA_private_key *out_private_key,
    uint8_t **out_encoded_public_key, size_t *out_encoded_public_key_len,
    const uint8_t entropy[MLDSA_SEED_LENGTH]);

/*
 * mldsa_sign_internal behaves like |mldsa_sign| but takes the context prefix
 * and the signing randomizer as explicit arguments. It is called internally and
 * by tests.
 */
int mldsa_sign_internal(const MLDSA_private_key *private_key,
    const uint8_t *msg, size_t msg_len, const uint8_t *context_prefix,
    size_t context_prefix_len, const uint8_t *context, size_t context_len,
    const uint8_t randomizer[MLDSA_SIGNATURE_RANDOMIZER_LENGTH],
    uint8_t **out_encoded_signature, size_t *out_encoded_signature_len);

/*
 * mldsa_verify_internal behaves like |mldsa_verify| but takes the context
 * prefix as an explicit argument. It is called internally and by tests.
 */
int mldsa_verify_internal(const MLDSA_public_key *public_key,
    const uint8_t *signature, size_t signature_len, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len);

__END_HIDDEN_DECLS

#if defined(__cplusplus)
}
#endif

#endif  /* OPENSSL_HEADER_CRYPTO_MLDSA_INTERNAL_H */
