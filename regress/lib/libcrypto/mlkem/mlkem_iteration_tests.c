/*	$OpenBSD: mlkem_iteration_tests.c,v 1.2 2024/12/26 07:26:45 tb Exp $ */
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
#include <stdlib.h>

#include "mlkem.h"

#include "mlkem_internal.h"
#include "mlkem_tests_util.h"
#include "sha3_internal.h"

/*
 * Based on https://c2sp.org/CCTV/ML-KEM
 *
 * The final value has been updated to reflect the change from Kyber to ML-KEM.
 *
 * The deterministic RNG is a single SHAKE-128 instance with an empty input.
 * (The RNG stream starts with 7f9c2ba4e88f827d616045507605853e.)
 */
const uint8_t kExpectedSeedStart[16] = {
	0x7f, 0x9c, 0x2b, 0xa4, 0xe8, 0x8f, 0x82, 0x7d, 0x61, 0x60, 0x45,
	0x50, 0x76, 0x05, 0x85, 0x3e
};

/*
 * Filippo says:
 * ML-KEM-768: f7db260e1137a742e05fe0db9525012812b004d29040a5b606aad3d134b548d3
 * but Boring believes this:
 */
const uint8_t kExpectedAdam768[32] = {
	0xf9, 0x59, 0xd1, 0x8d, 0x3d, 0x11, 0x80, 0x12, 0x14, 0x33, 0xbf,
	0x0e, 0x05, 0xf1, 0x1e, 0x79, 0x08, 0xcf, 0x9d, 0x03, 0xed, 0xc1,
	0x50, 0xb2, 0xb0, 0x7c, 0xb9, 0x0b, 0xef, 0x5b, 0xc1, 0xc1
};

/*
 * Filippo says:
 * ML-KEM-1024: 47ac888fe61544efc0518f46094b4f8a600965fc89822acb06dc7169d24f3543
 * but Boring believes this:
 */
const uint8_t kExpectedAdam1024[32] = {
	0xe3, 0xbf, 0x82, 0xb0, 0x13, 0x30, 0x7b, 0x2e, 0x9d, 0x47, 0xdd,
	0xe7, 0x91, 0xff, 0x6d, 0xfc, 0x82, 0xe6, 0x94, 0xe6, 0x38, 0x24,
	0x04, 0xab, 0xdb, 0x94, 0x8b, 0x90, 0x8b, 0x75, 0xba, 0xd5
};

struct iteration_ctx {
	uint8_t *encoded_public_key;
	size_t encoded_public_key_len;
	uint8_t *ciphertext;
	size_t ciphertext_len;
	uint8_t *invalid_ciphertext;
	size_t invalid_ciphertext_len;
	void *priv;
	void *pub;

	mlkem_encode_private_key_fn encode_private_key;
	mlkem_encap_external_entropy_fn encap_external_entropy;
	mlkem_generate_key_external_entropy_fn generate_key_external_entropy;
	mlkem_public_from_private_fn public_from_private;
	mlkem_decap_fn decap;

	const uint8_t *start;
	size_t start_len;

	const uint8_t *expected;
	size_t expected_len;
};

static int
MlkemIterativeTest(struct iteration_ctx *ctx)
{
	uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
	uint8_t encap_entropy[MLKEM_ENCAP_ENTROPY];
	uint8_t seed[MLKEM_SEED_BYTES] = {0};
	sha3_ctx drng, results;
	uint8_t out[32];
	int i;

	shake128_init(&drng);
	shake128_init(&results);

	shake_xof(&drng);
	for (i = 0; i < 10000; i++) {
		uint8_t *encoded_private_key = NULL;
		size_t encoded_private_key_len;

		/*
		 * This should draw both d and z from DRNG concatenating in
		 * seed.
		 */
		shake_out(&drng, seed, sizeof(seed));
		if (i == 0) {
			if (compare_data(seed, ctx->start, ctx->start_len,
			    "seed start") != 0)
				errx(1, "compare_data");
		}

		/* generate ek as encoded_public_key */
		ctx->generate_key_external_entropy(ctx->encoded_public_key,
		    ctx->priv, seed);
		ctx->public_from_private(ctx->pub, ctx->priv);

		/* hash in ek */
		shake_update(&results, ctx->encoded_public_key,
		    ctx->encoded_public_key_len);

		/* marshal priv to dk as encoded_private_key */
		if (!ctx->encode_private_key(ctx->priv, &encoded_private_key,
		    &encoded_private_key_len))
			errx(1, "encode private key");

		/* hash in dk */
		shake_update(&results, encoded_private_key,
		    encoded_private_key_len);

		free(encoded_private_key);

		/* draw m as encap entropy from DRNG */
		shake_out(&drng, encap_entropy, sizeof(encap_entropy));

		/* generate ct as ciphertext, k as shared_secret */
		ctx->encap_external_entropy(ctx->ciphertext, shared_secret,
		    ctx->pub, encap_entropy);

		/* hash in ct */
		shake_update(&results, ctx->ciphertext, ctx->ciphertext_len);
		/* hash in k */
		shake_update(&results, shared_secret, sizeof(shared_secret));

		/* draw ct as invalid_ciphertxt from DRNG */
		shake_out(&drng, ctx->invalid_ciphertext,
		    ctx->invalid_ciphertext_len);

		/* generate k as shared secret from invalid ciphertext */
		if (!ctx->decap(shared_secret, ctx->invalid_ciphertext,
		    ctx->invalid_ciphertext_len, ctx->priv))
			errx(1, "decap failed");

		/* hash in k */
		shake_update(&results, shared_secret, sizeof(shared_secret));
	}
	shake_xof(&results);
	shake_out(&results, out, sizeof(out));

	return compare_data(ctx->expected, out, sizeof(out), "final result hash");
}

int
main(void)
{
	uint8_t encoded_public_key768[MLKEM768_PUBLIC_KEY_BYTES];
	uint8_t ciphertext768[MLKEM768_CIPHERTEXT_BYTES];
	uint8_t invalid_ciphertext768[MLKEM768_CIPHERTEXT_BYTES];
	struct MLKEM768_private_key priv768;
	struct MLKEM768_public_key pub768;
	struct iteration_ctx iteration768 = {
		.encoded_public_key = encoded_public_key768,
		.encoded_public_key_len = sizeof(encoded_public_key768),
		.ciphertext = ciphertext768,
		.ciphertext_len = sizeof(ciphertext768),
		.invalid_ciphertext = invalid_ciphertext768,
		.invalid_ciphertext_len = sizeof(invalid_ciphertext768),
		.priv = &priv768,
		.pub = &pub768,
		.encap_external_entropy = mlkem768_encap_external_entropy,
		.encode_private_key = mlkem768_encode_private_key,
		.generate_key_external_entropy =
		    mlkem768_generate_key_external_entropy,
		.public_from_private = mlkem768_public_from_private,
		.decap = mlkem768_decap,
		.start = kExpectedSeedStart,
		.start_len = sizeof(kExpectedSeedStart),
		.expected = kExpectedAdam768,
		.expected_len = sizeof(kExpectedAdam768),
	};
	uint8_t encoded_public_key1024[MLKEM1024_PUBLIC_KEY_BYTES];
	uint8_t ciphertext1024[MLKEM1024_CIPHERTEXT_BYTES];
	uint8_t invalid_ciphertext1024[MLKEM1024_CIPHERTEXT_BYTES];
	struct MLKEM1024_private_key priv1024;
	struct MLKEM1024_public_key pub1024;
	struct iteration_ctx iteration1024 = {
		.encoded_public_key = encoded_public_key1024,
		.encoded_public_key_len = sizeof(encoded_public_key1024),
		.ciphertext = ciphertext1024,
		.ciphertext_len = sizeof(ciphertext1024),
		.invalid_ciphertext = invalid_ciphertext1024,
		.invalid_ciphertext_len = sizeof(invalid_ciphertext1024),
		.priv = &priv1024,
		.pub = &pub1024,
		.encap_external_entropy = mlkem1024_encap_external_entropy,
		.encode_private_key = mlkem1024_encode_private_key,
		.generate_key_external_entropy =
		    mlkem1024_generate_key_external_entropy,
		.public_from_private = mlkem1024_public_from_private,
		.decap = mlkem1024_decap,
		.start = kExpectedSeedStart,
		.start_len = sizeof(kExpectedSeedStart),
		.expected = kExpectedAdam1024,
		.expected_len = sizeof(kExpectedAdam1024),
	};
	int failed = 0;

	failed |= MlkemIterativeTest(&iteration768);
	failed |= MlkemIterativeTest(&iteration1024);

	return failed;
}
