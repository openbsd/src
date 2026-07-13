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

#include "bytestring.h"
#include "mldsa_internal.h"

#include "parse_test_file.h"

/*
 * ML-DSA key generation known answer tests.
 *
 * Derive a key pair deterministically from the seed and check that the
 * encoded public and private keys match the NIST ACVP vectors.
 */

struct keygen_ctx {
	struct parse *parse_ctx;

	int rank;
};

enum keygen_states {
	KEYGEN_SEED,
	KEYGEN_PUB,
	KEYGEN_PRIV,
	N_KEYGEN_STATES,
};

static const struct line_spec keygen_state_machine[] = {
	[KEYGEN_SEED] = {
		.state = KEYGEN_SEED,
		.type = LINE_HEX,
		.name = "seed",
		.label = "seed",
	},
	[KEYGEN_PUB] = {
		.state = KEYGEN_PUB,
		.type = LINE_HEX,
		.name = "public key",
		.label = "pub",
	},
	[KEYGEN_PRIV] = {
		.state = KEYGEN_PRIV,
		.type = LINE_HEX,
		.name = "private key",
		.label = "priv",
	},
};

static int
keygen_init(void *ctx, void *parse_ctx)
{
	struct keygen_ctx *keygen = ctx;

	keygen->parse_ctx = parse_ctx;

	return 1;
}

static void
keygen_finish(void *ctx)
{
	(void)ctx;
}

static int
MldsaKeygenFileTest(struct keygen_ctx *keygen)
{
	struct parse *p = keygen->parse_ctx;
	MLDSA_private_key *priv_key = NULL;
	CBS seed, pub, priv;
	uint8_t *encoded_public_key = NULL;
	size_t encoded_public_key_len = 0;
	uint8_t *encoded_private_key = NULL;
	size_t encoded_private_key_len = 0;
	int failed = 1;

	parse_get_cbs(p, KEYGEN_SEED, &seed);
	parse_get_cbs(p, KEYGEN_PUB, &pub);
	parse_get_cbs(p, KEYGEN_PRIV, &priv);

	if (!parse_length_equal(p, "seed", MLDSA_SEED_LENGTH, CBS_len(&seed)))
		goto err;

	if ((priv_key = MLDSA_private_key_new(keygen->rank)) == NULL)
		parse_errx(p, "MLDSA_private_key_new");

	if (!mldsa_generate_key_external_entropy(priv_key, &encoded_public_key,
	    &encoded_public_key_len, CBS_data(&seed))) {
		parse_info(p, "mldsa_generate_key_external_entropy");
		goto err;
	}

	if (!mldsa_marshal_private_key(priv_key, &encoded_private_key,
	    &encoded_private_key_len)) {
		parse_info(p, "mldsa_marshal_private_key");
		goto err;
	}

	failed = !parse_data_equal(p, "public key", &pub,
	    encoded_public_key, encoded_public_key_len);
	failed |= !parse_data_equal(p, "private key", &priv,
	    encoded_private_key, encoded_private_key_len);

 err:
	MLDSA_private_key_free(priv_key);
	freezero(encoded_public_key, encoded_public_key_len);
	freezero(encoded_private_key, encoded_private_key_len);

	return failed;
}

static int
keygen_run_test_case(void *ctx)
{
	return MldsaKeygenFileTest(ctx);
}

static const struct test_parse keygen_parse = {
	.states = keygen_state_machine,
	.num_states = N_KEYGEN_STATES,

	.init = keygen_init,
	.finish = keygen_finish,

	.run_test_case = keygen_run_test_case,
};

static int
mldsa_keygen_tests(const char *fn, int rank)
{
	struct keygen_ctx keygen = {
		.rank = rank,
	};

	return parse_test_file(fn, &keygen_parse, &keygen);
}

/*
 * ML-DSA signature generation known answer tests.
 *
 * Parse the private key, sign the message deterministically with a zero
 * randomizer using the internal signing function, and check that the
 * signature matches the NIST ACVP vectors. The generated signature is then
 * verified as a round trip sanity check.
 */

struct siggen_ctx {
	struct parse *parse_ctx;

	int rank;
};

enum siggen_states {
	SIGGEN_SK,
	SIGGEN_MESSAGE,
	SIGGEN_SIGNATURE,
	N_SIGGEN_STATES,
};

static const struct line_spec siggen_state_machine[] = {
	[SIGGEN_SK] = {
		.state = SIGGEN_SK,
		.type = LINE_HEX,
		.name = "private key",
		.label = "sk",
	},
	[SIGGEN_MESSAGE] = {
		.state = SIGGEN_MESSAGE,
		.type = LINE_HEX,
		.name = "message",
		.label = "message",
	},
	[SIGGEN_SIGNATURE] = {
		.state = SIGGEN_SIGNATURE,
		.type = LINE_HEX,
		.name = "signature",
		.label = "signature",
	},
};

static int
siggen_init(void *ctx, void *parse_ctx)
{
	struct siggen_ctx *siggen = ctx;

	siggen->parse_ctx = parse_ctx;

	return 1;
}

static void
siggen_finish(void *ctx)
{
	(void)ctx;
}

static int
MldsaSiggenFileTest(struct siggen_ctx *siggen)
{
	struct parse *p = siggen->parse_ctx;
	MLDSA_private_key *priv_key = NULL;
	MLDSA_public_key *pub_key = NULL;
	CBS sk, message, signature;
	uint8_t randomizer[MLDSA_SIGNATURE_RANDOMIZER_LENGTH] = { 0 };
	uint8_t *encoded_signature = NULL;
	size_t encoded_signature_len = 0;
	int failed = 1;

	parse_get_cbs(p, SIGGEN_SK, &sk);
	parse_get_cbs(p, SIGGEN_MESSAGE, &message);
	parse_get_cbs(p, SIGGEN_SIGNATURE, &signature);

	if ((priv_key = MLDSA_private_key_new(siggen->rank)) == NULL)
		parse_errx(p, "MLDSA_private_key_new");

	if (!MLDSA_parse_private_key(priv_key, CBS_data(&sk), CBS_len(&sk))) {
		parse_info(p, "MLDSA_parse_private_key");
		goto err;
	}

	if (!mldsa_sign_internal(priv_key, CBS_data(&message), CBS_len(&message),
	    NULL, 0, NULL, 0, randomizer, &encoded_signature,
	    &encoded_signature_len)) {
		parse_info(p, "mldsa_sign_internal");
		goto err;
	}

	failed = !parse_data_equal(p, "signature", &signature,
	    encoded_signature, encoded_signature_len);

	/* The signature we produced must verify. */
	if ((pub_key = MLDSA_public_key_new(siggen->rank)) == NULL)
		parse_errx(p, "MLDSA_public_key_new");

	if (!MLDSA_public_from_private(priv_key, pub_key)) {
		parse_info(p, "MLDSA_public_from_private");
		failed = 1;
		goto err;
	}

	if (!mldsa_verify_internal(pub_key, encoded_signature,
	    encoded_signature_len, CBS_data(&message), CBS_len(&message),
	    NULL, 0, NULL, 0)) {
		parse_info(p, "mldsa_verify_internal");
		failed = 1;
	}

 err:
	MLDSA_private_key_free(priv_key);
	MLDSA_public_key_free(pub_key);
	freezero(encoded_signature, encoded_signature_len);

	return failed;
}

static int
siggen_run_test_case(void *ctx)
{
	return MldsaSiggenFileTest(ctx);
}

static const struct test_parse siggen_parse = {
	.states = siggen_state_machine,
	.num_states = N_SIGGEN_STATES,

	.init = siggen_init,
	.finish = siggen_finish,

	.run_test_case = siggen_run_test_case,
};

static int
mldsa_siggen_tests(const char *fn, int rank)
{
	struct siggen_ctx siggen = {
		.rank = rank,
	};

	return parse_test_file(fn, &siggen_parse, &siggen);
}

static int
run_mldsa_test(const char *test, const char *fn)
{
	if (strcmp(test, "mldsa65_nist_keygen_tests") == 0)
		return mldsa_keygen_tests(fn, MLDSA65_RANK);
	if (strcmp(test, "mldsa87_nist_keygen_tests") == 0)
		return mldsa_keygen_tests(fn, MLDSA87_RANK);

	if (strcmp(test, "mldsa65_nist_siggen_tests") == 0)
		return mldsa_siggen_tests(fn, MLDSA65_RANK);
	if (strcmp(test, "mldsa87_nist_siggen_tests") == 0)
		return mldsa_siggen_tests(fn, MLDSA87_RANK);

	errx(1, "unknown test %s (test file %s)", test, fn);
}

int
main(int argc, const char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: mldsa_tests test testfile.txt\n");
		exit(1);
	}

	return run_mldsa_test(argv[1], argv[2]);
}
