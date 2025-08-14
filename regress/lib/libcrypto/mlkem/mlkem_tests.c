/*	$OpenBSD: mlkem_tests.c,v 1.7 2025/08/14 15:48:48 beck Exp $ */
/*
 * Copyright (c) 2024 Google Inc.
 * Copyright (c) 2024 Theo Buehler <tb@openbsd.org>
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

#include "mlkem_internal.h"

#include "mlkem_tests_util.h"
#include "parse_test_file.h"

enum test_type {
	TEST_TYPE_NORMAL,
	TEST_TYPE_NIST,
};

struct decap_ctx {
	struct parse *parse_ctx;
	int rank;
	MLKEM_private_key *private_key;
};

enum decap_states {
	DECAP_PRIVATE_KEY,
	DECAP_CIPHERTEXT,
	DECAP_RESULT,
	DECAP_SHARED_SECRET,
	N_DECAP_STATES,
};

static const struct line_spec decap_state_machine[] = {
	[DECAP_PRIVATE_KEY] = {
		.state = DECAP_PRIVATE_KEY,
		.type = LINE_HEX,
		.name = "private key",
		.label = "private_key",
	},
	[DECAP_CIPHERTEXT] = {
		.state = DECAP_CIPHERTEXT,
		.type = LINE_HEX,
		.name = "cipher text",
		.label = "ciphertext",
	},
	[DECAP_RESULT] = {
		.state = DECAP_RESULT,
		.type = LINE_STRING_MATCH,
		.name = "result",
		.label = "result",
		.match = "fail",
	},
	[DECAP_SHARED_SECRET] = {
		.state = DECAP_SHARED_SECRET,
		.type = LINE_HEX,
		.name = "shared secret",
		.label = "shared_secret",
	},
};

static int
decap_init(void *ctx, void *parse_ctx)
{
	struct decap_ctx *decap = ctx;

	decap->parse_ctx = parse_ctx;

	return (decap->private_key = MLKEM_private_key_new(decap->rank))
	    != NULL;
}

static void
decap_finish(void *ctx)
{
	struct decap_ctx *decap = ctx;

	MLKEM_private_key_free(decap->private_key);
	decap->private_key = NULL;
}

static int
MlkemDecapFileTest(struct decap_ctx *decap)
{
	struct parse *p = decap->parse_ctx;
	uint8_t *shared_secret_buf = NULL;
	CBS ciphertext, shared_secret, private_key;
	size_t s_len = 0;
	int should_fail;
	int failed = 1;

	parse_get_cbs(p, DECAP_CIPHERTEXT, &ciphertext);
	parse_get_cbs(p, DECAP_SHARED_SECRET, &shared_secret);
	parse_get_cbs(p, DECAP_PRIVATE_KEY, &private_key);
	parse_get_int(p, DECAP_RESULT, &should_fail);

	if (!MLKEM_parse_private_key(decap->private_key,
	    CBS_data(&private_key), CBS_len(&private_key))) {
		if ((failed = !should_fail))
			parse_info(p, "parse private key");
		goto err;
	}
	if (!MLKEM_decap(decap->private_key, CBS_data(&ciphertext),
	    CBS_len(&ciphertext), &shared_secret_buf, &s_len)) {
		if ((failed = !should_fail))
			parse_info(p, "decap");
		goto err;
	}

	if (s_len != MLKEM_SHARED_SECRET_LENGTH) {
		if ((failed = !should_fail))
			parse_info(p, "shared secret length %zu != %d", s_len,
			    MLKEM_SHARED_SECRET_LENGTH);
		goto err;
	}

	failed = !parse_data_equal(p, "shared_secret", &shared_secret,
	    shared_secret_buf, s_len);

	if (should_fail != failed) {
		parse_info(p, "FAIL: should_fail %d, failed %d",
		    should_fail, failed);
		failed = 1;
	}

 err:
	freezero(shared_secret_buf, s_len);
	return failed;
}

static int
decap_run_test_case(void *ctx)
{
	return MlkemDecapFileTest(ctx);
}

static const struct test_parse decap_parse = {
	.states = decap_state_machine,
	.num_states = N_DECAP_STATES,

	.init = decap_init,
	.finish = decap_finish,

	.run_test_case = decap_run_test_case,
};

enum nist_decap_instructions {
	NIST_DECAP_DK,
	N_NIST_DECAP_INSTRUCTIONS,
};

static const struct line_spec nist_decap_instruction_state_machine[] = {
	[NIST_DECAP_DK] = {
		.state = NIST_DECAP_DK,
		.type = LINE_HEX,
		.name = "private key (instruction [dk])",
		.label = "dk",
	},
};

enum nist_decap_states {
	NIST_DECAP_C,
	NIST_DECAP_K,
	N_NIST_DECAP_STATES,
};

static const struct line_spec nist_decap_state_machine[] = {
	[NIST_DECAP_C] = {
		.state = NIST_DECAP_C,
		.type = LINE_HEX,
		.name = "ciphertext (c)",
		.label = "c",
	},
	[NIST_DECAP_K] = {
		.state = NIST_DECAP_K,
		.type = LINE_HEX,
		.name = "shared secret (k)",
		.label = "k",
	},
};

static int
MlkemNistDecapFileTest(struct decap_ctx *decap)
{
	struct parse *p = decap->parse_ctx;
	uint8_t *shared_secret = NULL;
	CBS dk, c, k;
	size_t s_len = 0;
	int failed = 1;

	parse_instruction_get_cbs(p, NIST_DECAP_DK, &dk);
	parse_get_cbs(p, NIST_DECAP_C, &c);
	parse_get_cbs(p, NIST_DECAP_K, &k);

	if (!parse_length_equal(p, "private key",
	    MLKEM_private_key_encoded_length(decap->private_key), CBS_len(&dk)))
		goto err;
	if (!parse_length_equal(p, "shared secret",
	    MLKEM_SHARED_SECRET_BYTES, CBS_len(&k)))
		goto err;

	if (!MLKEM_parse_private_key(decap->private_key, CBS_data(&dk),
	    CBS_len(&dk))) {
		parse_info(p, "parse private key");
		goto err;
	}
	if (!MLKEM_decap(decap->private_key, CBS_data(&c), CBS_len(&c),
	    &shared_secret, &s_len)) {
		parse_info(p, "decap");
		goto err;
	}

	if (s_len != MLKEM_SHARED_SECRET_LENGTH) {
		parse_info(p, "shared secret length %zu != %d", s_len,
		    MLKEM_SHARED_SECRET_LENGTH);
		goto err;
	}

	failed = !parse_data_equal(p, "shared secret", &k,
	    shared_secret, s_len);

 err:
	free(shared_secret);
	return failed;
}

static int
nist_decap_run_test_case(void *ctx)
{
	return MlkemNistDecapFileTest(ctx);
}

static const struct test_parse nist_decap_parse = {
	.instructions = nist_decap_instruction_state_machine,
	.num_instructions = N_NIST_DECAP_INSTRUCTIONS,

	.states = nist_decap_state_machine,
	.num_states = N_NIST_DECAP_STATES,

	.init = decap_init,
	.finish = decap_finish,

	.run_test_case = nist_decap_run_test_case,
};

static int
mlkem_decap_tests(const char *fn, int rank, enum test_type test_type)
{
	struct decap_ctx decap = {
		.rank = rank,
	};
	int ret = 0;

	if (test_type == TEST_TYPE_NORMAL)
		ret = parse_test_file(fn, &decap_parse, &decap);
	else if (test_type == TEST_TYPE_NIST)
		ret = parse_test_file(fn, &nist_decap_parse, &decap);
	else
		errx(1, "unknown decap test: rank %d, type %d", rank,
		    test_type);

	return ret;
}

struct encap_ctx {
	struct parse *parse_ctx;

	int rank;
	MLKEM_public_key *public_key;
	uint8_t *ciphertext;
	size_t ciphertext_len;
};

enum encap_states {
	ENCAP_ENTROPY,
	ENCAP_PUBLIC_KEY,
	ENCAP_RESULT,
	ENCAP_CIPHERTEXT,
	ENCAP_SHARED_SECRET,
	N_ENCAP_STATES,
};

static const struct line_spec encap_state_machine[] = {
	[ENCAP_ENTROPY] = {
		.state = ENCAP_ENTROPY,
		.type = LINE_HEX,
		.name = "entropy",
		.label = "entropy",
	},
	[ENCAP_PUBLIC_KEY] = {
		.state = ENCAP_PUBLIC_KEY,
		.type = LINE_HEX,
		.name = "public key",
		.label = "public_key",
	},
	[ENCAP_RESULT] = {
		.state = ENCAP_RESULT,
		.type = LINE_STRING_MATCH,
		.name = "result",
		.label = "result",
		.match = "fail",
	},
	[ENCAP_CIPHERTEXT] = {
		.state = ENCAP_CIPHERTEXT,
		.type = LINE_HEX,
		.name = "ciphertext",
		.label = "ciphertext",
	},
	[ENCAP_SHARED_SECRET] = {
		.state = ENCAP_SHARED_SECRET,
		.type = LINE_HEX,
		.name = "shared secret",
		.label = "shared_secret",
	},
};

static int
encap_init(void *ctx, void *parse_ctx)
{
	struct encap_ctx *encap = ctx;

	encap->parse_ctx = parse_ctx;

	encap->ciphertext = NULL;
	return (encap->public_key = MLKEM_public_key_new(encap->rank))
	    != NULL;
}

static void
encap_finish(void *ctx)
{
	struct encap_ctx *encap = ctx;

	freezero(encap->ciphertext, encap->ciphertext_len);
	encap->ciphertext = NULL;
	MLKEM_public_key_free(encap->public_key);
	encap->public_key = NULL;
}

static int
MlkemEncapFileTest(struct encap_ctx *encap)
{
	CBS entropy, public_key, ciphertext, shared_secret;
	struct parse *p = encap->parse_ctx;
	uint8_t *shared_secret_buf = NULL;
	size_t s_len = 0;

	int should_fail;
	int failed = 1;

	parse_get_cbs(p, ENCAP_ENTROPY, &entropy);
	parse_get_cbs(p, ENCAP_PUBLIC_KEY, &public_key);
	parse_get_cbs(p, ENCAP_CIPHERTEXT, &ciphertext);
	parse_get_cbs(p, ENCAP_SHARED_SECRET, &shared_secret);
	parse_get_int(p, ENCAP_RESULT, &should_fail);

	if (!MLKEM_parse_public_key(encap->public_key, CBS_data(&public_key),
	    CBS_len(&public_key))) {
		if ((failed = !should_fail))
			parse_info(p, "parse public key");
		goto err;
	}
	if (!MLKEM_encap_external_entropy(encap->public_key, CBS_data(&entropy),
	    &encap->ciphertext, &encap->ciphertext_len, &shared_secret_buf,
	    &s_len)) {
		if ((failed = !should_fail))
			parse_info(p, "encap_external_entropy");
		goto err;
	}

	if (s_len != MLKEM_SHARED_SECRET_LENGTH) {
		if ((failed = !should_fail))
			parse_info(p, "shared secret length %zu != %d", s_len,
			    MLKEM_SHARED_SECRET_LENGTH);
		goto err;
	}

	failed = !parse_data_equal(p, "shared_secret", &shared_secret,
	    shared_secret_buf, s_len);
	failed |= !parse_data_equal(p, "ciphertext", &ciphertext,
	    encap->ciphertext, encap->ciphertext_len);


	if (should_fail != failed) {
		parse_info(p, "FAIL: should_fail %d, failed %d",
		    should_fail, failed);
		failed = 1;
	}

 err:
	freezero(shared_secret_buf, s_len);
	return failed;
}

static int
encap_run_test_case(void *ctx)
{
	return MlkemEncapFileTest(ctx);
}

static const struct test_parse encap_parse = {
	.states = encap_state_machine,
	.num_states = N_ENCAP_STATES,

	.init = encap_init,
	.finish = encap_finish,

	.run_test_case = encap_run_test_case,
};

static int
mlkem_encap_tests(const char *fn, int rank)
{
	struct encap_ctx encap = {
		.rank = rank,
	};

	return parse_test_file(fn, &encap_parse, &encap);
}

struct keygen_ctx {
	struct parse *parse_ctx;

	int rank;
	MLKEM_private_key *private_key;
	uint8_t *encoded_public_key;
	size_t encoded_public_key_len;
};

enum keygen_states {
	KEYGEN_SEED,
	KEYGEN_PUBLIC_KEY,
	KEYGEN_PRIVATE_KEY,
	N_KEYGEN_STATES,
};

static const struct line_spec keygen_state_machine[] = {
	[KEYGEN_SEED] = {
		.state = KEYGEN_SEED,
		.type = LINE_HEX,
		.name = "seed",
		.label = "seed",
	},
	[KEYGEN_PUBLIC_KEY] = {
		.state = KEYGEN_PUBLIC_KEY,
		.type = LINE_HEX,
		.name = "public key",
		.label = "public_key",
	},
	[KEYGEN_PRIVATE_KEY] = {
		.state = KEYGEN_PRIVATE_KEY,
		.type = LINE_HEX,
		.name = "private key",
		.label = "private_key",
	},
};

static int
keygen_init(void *ctx, void *parse_ctx)
{
	struct keygen_ctx *keygen = ctx;

	keygen->parse_ctx = parse_ctx;

	return (keygen->private_key = MLKEM_private_key_new(keygen->rank))
	    != NULL;
}

static void
keygen_finish(void *ctx)
{
	struct keygen_ctx *keygen = ctx;

	freezero(keygen->encoded_public_key, keygen->encoded_public_key_len);
	keygen->encoded_public_key = NULL;
	MLKEM_private_key_free(keygen->private_key);
	keygen->private_key = NULL;
}

static int
MlkemKeygenFileTest(struct keygen_ctx *keygen)
{
	struct parse *p = keygen->parse_ctx;
	CBS seed, public_key, private_key;
	uint8_t *encoded_private_key = NULL;
	size_t encoded_private_key_len = 0;
	int failed = 1;

	parse_get_cbs(p, KEYGEN_SEED, &seed);
	parse_get_cbs(p, KEYGEN_PUBLIC_KEY, &public_key);
	parse_get_cbs(p, KEYGEN_PRIVATE_KEY, &private_key);

	if (!parse_length_equal(p, "seed", MLKEM_SEED_LENGTH, CBS_len(&seed)))
		goto err;

	if (!MLKEM_generate_key_external_entropy(keygen->private_key,
	    &keygen->encoded_public_key, &keygen->encoded_public_key_len,
	    CBS_data(&seed))) {
		parse_info(p, "generate_key_external_entropy");
		goto err;
	}

	if (!parse_length_equal(p, "public key",
	    keygen->encoded_public_key_len, CBS_len(&public_key)))
		goto err;
	if (!parse_length_equal(p, "private key",
	    MLKEM_private_key_encoded_length(keygen->private_key),
	    CBS_len(&private_key)))
		goto err;

	if (!MLKEM_marshal_private_key(keygen->private_key,
	    &encoded_private_key, &encoded_private_key_len)) {
		parse_info(p, "encode private key");
		goto err;
	}

	failed = !parse_data_equal(p, "private key", &private_key,
	    encoded_private_key, encoded_private_key_len);
	failed |= !parse_data_equal(p, "public key", &public_key,
	    keygen->encoded_public_key, keygen->encoded_public_key_len);

 err:
	freezero(encoded_private_key, encoded_private_key_len);

	return failed;
}

static int
keygen_run_test_case(void *ctx)
{
	return MlkemKeygenFileTest(ctx);
}

static const struct test_parse keygen_parse = {
	.states = keygen_state_machine,
	.num_states = N_KEYGEN_STATES,

	.init = keygen_init,
	.finish = keygen_finish,

	.run_test_case = keygen_run_test_case,
};

enum nist_keygen_states {
	NIST_KEYGEN_Z,
	NIST_KEYGEN_D,
	NIST_KEYGEN_EK,
	NIST_KEYGEN_DK,
	N_NIST_KEYGEN_STATES,
};

static const struct line_spec nist_keygen_state_machine[] = {
	[NIST_KEYGEN_Z] = {
		.state = NIST_KEYGEN_Z,
		.type = LINE_HEX,
		.name = "seed (z)",
		.label = "z",
	},
	[NIST_KEYGEN_D] = {
		.state = NIST_KEYGEN_D,
		.type = LINE_HEX,
		.name = "seed (d)",
		.label = "d",
	},
	[NIST_KEYGEN_EK] = {
		.state = NIST_KEYGEN_EK,
		.type = LINE_HEX,
		.name = "public key (ek)",
		.label = "ek",
	},
	[NIST_KEYGEN_DK] = {
		.state = NIST_KEYGEN_DK,
		.type = LINE_HEX,
		.name = "private key (dk)",
		.label = "dk",
	},
};

static int
MlkemNistKeygenFileTest(struct keygen_ctx *keygen)
{
	struct parse *p = keygen->parse_ctx;
	CBB seed_cbb;
	CBS z, d, ek, dk;
	uint8_t seed[MLKEM_SEED_LENGTH];
	size_t seed_len;
	uint8_t *encoded_private_key = NULL;
	size_t encoded_private_key_len = 0;
	int failed = 1;

	parse_get_cbs(p, NIST_KEYGEN_Z, &z);
	parse_get_cbs(p, NIST_KEYGEN_D, &d);
	parse_get_cbs(p, NIST_KEYGEN_EK, &ek);
	parse_get_cbs(p, NIST_KEYGEN_DK, &dk);

	if (!CBB_init_fixed(&seed_cbb, seed, sizeof(seed)))
		parse_errx(p, "CBB_init_fixed");
	if (!CBB_add_bytes(&seed_cbb, CBS_data(&d), CBS_len(&d)))
		parse_errx(p, "CBB_add_bytes");
	if (!CBB_add_bytes(&seed_cbb, CBS_data(&z), CBS_len(&z)))
		parse_errx(p, "CBB_add_bytes");
	if (!CBB_finish(&seed_cbb, NULL, &seed_len))
		parse_errx(p, "CBB_finish");

	if (!parse_length_equal(p, "bogus z or d", MLKEM_SEED_LENGTH, seed_len))
		goto err;

	if (!MLKEM_generate_key_external_entropy(keygen->private_key,
	    &keygen->encoded_public_key, &keygen->encoded_public_key_len,
	    seed)) {
		parse_info(p, "generate_key_external_entropy");
		goto err;
	}

	if (!MLKEM_marshal_private_key(keygen->private_key,
	    &encoded_private_key, &encoded_private_key_len)) {
		parse_info(p, "encode private key");
		goto err;
	}

	failed = !parse_data_equal(p, "public key", &ek,
	    keygen->encoded_public_key, keygen->encoded_public_key_len);
	failed |= !parse_data_equal(p, "private key", &dk,
	    encoded_private_key, encoded_private_key_len);

 err:
	freezero(encoded_private_key, encoded_private_key_len);

	return failed;
}

static int
nist_keygen_run_test_case(void *ctx)
{
	return MlkemNistKeygenFileTest(ctx);
}

static const struct test_parse nist_keygen_parse = {
	.states = nist_keygen_state_machine,
	.num_states = N_NIST_KEYGEN_STATES,

	.init = keygen_init,
	.finish = keygen_finish,

	.run_test_case = nist_keygen_run_test_case,
};

static int
mlkem_keygen_tests(const char *fn, int rank, enum test_type test_type)
{
	struct keygen_ctx keygen = {
		.rank = rank,
	};
	int ret = 0;

	if (test_type == TEST_TYPE_NORMAL)
		ret = parse_test_file(fn, &keygen_parse, &keygen);
	else if (test_type == TEST_TYPE_NIST)
		ret = parse_test_file(fn, &nist_keygen_parse, &keygen);
	else
		errx(1, "unknown keygen test: rank %d, type %d", rank,
		    test_type);

	return ret;
}

static int
run_mlkem_test(const char *test, const char *fn)
{
	if (strcmp(test, "mlkem768_decap_tests") == 0)
		return mlkem_decap_tests(fn, RANK768, TEST_TYPE_NORMAL);
	if (strcmp(test, "mlkem768_nist_decap_tests") == 0)
		return mlkem_decap_tests(fn, RANK768, TEST_TYPE_NIST);
	if (strcmp(test, "mlkem1024_decap_tests") == 0)
		return mlkem_decap_tests(fn, RANK1024, TEST_TYPE_NORMAL);
	if (strcmp(test, "mlkem1024_nist_decap_tests") == 0)
		return mlkem_decap_tests(fn, RANK1024, TEST_TYPE_NIST);

	if (strcmp(test, "mlkem768_encap_tests") == 0)
		return mlkem_encap_tests(fn, RANK768);
	if (strcmp(test, "mlkem1024_encap_tests") == 0)
		return mlkem_encap_tests(fn, RANK1024);

	if (strcmp(test, "mlkem768_keygen_tests") == 0)
		return mlkem_keygen_tests(fn, RANK768, TEST_TYPE_NORMAL);
	if (strcmp(test, "mlkem768_nist_keygen_tests") == 0)
		return mlkem_keygen_tests(fn, RANK768, TEST_TYPE_NIST);
	if (strcmp(test, "mlkem1024_keygen_tests") == 0)
		return mlkem_keygen_tests(fn, RANK1024, TEST_TYPE_NORMAL);
	if (strcmp(test, "mlkem1024_nist_keygen_tests") == 0)
		return mlkem_keygen_tests(fn, RANK1024, TEST_TYPE_NIST);

	errx(1, "unknown test %s (test file %s)", test, fn);
}

int
main(int argc, const char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: mlkem_test test testfile.txt\n");
		exit(1);
	}

	return run_mlkem_test(argv[1], argv[2]);
}
