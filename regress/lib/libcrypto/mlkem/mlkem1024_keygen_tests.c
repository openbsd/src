/*	$OpenBSD: mlkem1024_keygen_tests.c,v 1.5 2024/12/20 00:07:12 tb Exp $ */
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

#include <assert.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bytestring.h"
#include "mlkem.h"

#include "mlkem_internal.h"
#include "mlkem_tests_util.h"

static int
MlkemKeygenFileTest(CBB *seed_cbb, CBB *public_key_cbb, CBB *private_key_cbb,
    size_t line)
{
	struct MLKEM1024_private_key priv;
	uint8_t *seed = NULL, *public_key = NULL, *private_key = NULL;
	size_t seed_len = 0, public_key_len = 0, private_key_len = 0;
	uint8_t *encoded_private_key = NULL;
	uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
	size_t len;
	int failed = 1;

	if (!CBB_finish(seed_cbb, &seed, &seed_len))
		goto err;
	if (!compare_length(MLKEM_SEED_BYTES, seed_len, line, "seed length"))
		goto err;
	if (!CBB_finish(public_key_cbb, &public_key, &public_key_len))
		goto err;
	if (!compare_length(MLKEM1024_PUBLIC_KEY_BYTES, public_key_len, line,
	    "public key length"))
		goto err;
	if (!CBB_finish(private_key_cbb, &private_key, &private_key_len))
		goto err;
	if (!compare_length(MLKEM1024_PUBLIC_KEY_BYTES, public_key_len, line,
	    "public key length"))
		goto err;

	MLKEM1024_generate_key_external_entropy(encoded_public_key, &priv,
	    seed);
	if (!mlkem1024_encode_private_key(&priv, &encoded_private_key, &len)) {
		warnx("#%zu: encoded_private_key", line);
		goto err;
	}

	if (!compare_length(MLKEM1024_PRIVATE_KEY_BYTES, len, line,
	    "private key length"))
		goto err;

	failed = compare_data(private_key, encoded_private_key,
	    MLKEM1024_PRIVATE_KEY_BYTES, line, "private key");
	failed |= compare_data(public_key, encoded_public_key,
	    MLKEM1024_PUBLIC_KEY_BYTES, line, "public key");

 err:
	CBB_cleanup(seed_cbb);
	CBB_cleanup(public_key_cbb);
	CBB_cleanup(private_key_cbb);
	freezero(seed, seed_len);
	freezero(public_key, public_key_len);
	freezero(private_key, private_key_len);
	free(encoded_private_key);

	return failed;
}

#define S_START		0
#define S_COMMENT	1
#define S_SEED		2
#define S_PUBLIC_KEY	3
#define S_PRIVATE_KEY	4

#define S2S(x) case x: return #x

static const char *
state2str(int state)
{
	switch (state) {
	S2S(S_START);
	S2S(S_COMMENT);
	S2S(S_SEED);
	S2S(S_PUBLIC_KEY);
	S2S(S_PRIVATE_KEY);
	default:
		errx(1, "unknown state %d", state);
	}
}

int
main(int argc, char **argv)
{
	CBB seed = { 0 }, public_key = { 0 }, private_key = { 0 };
	const char *test;
	size_t line = 0;
	char *buf = NULL;
	size_t buflen = 0;
	ssize_t len;
	FILE *fp;
	int state;
	int failed = 0;

	if (argc < 2)
		errx(1, "%s: missing test file", argv[0]);

	test = argv[1];

	if ((fp = fopen(test, "r")) == NULL)
		err(1, "cant't open test file");

	state = S_COMMENT;
	line = 0;

	while ((len = getline(&buf, &buflen, fp)) != -1) {
		const char *msg = state2str(state);
		CBS cbs;
		uint8_t u8;

		line++;
		CBS_init(&cbs, buf, len);

		if (!CBS_get_last_u8(&cbs, &u8))
			errx(1, "#%zu %s: CBB_get_last_u8", line, msg);
		assert(u8 == '\n');

		switch (state) {
		case S_START:
			state = S_COMMENT;
			break;
		case S_COMMENT:
			if (!CBS_get_u8(&cbs, &u8))
				errx(1, "#%zu %s: CBB_get_u8", line, msg);
			assert(u8 == '#');
			if (!CBS_skip(&cbs, CBS_len(&cbs)))
				errx(1, "#%zu %s: CBB_skip", line, msg);
			state = S_SEED;
			break;
		case S_SEED:
			if (!get_string_cbs(&cbs, "seed: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &seed, line, msg);
			state = S_PUBLIC_KEY;
			break;
		case S_PUBLIC_KEY:
			if (!get_string_cbs(&cbs, "public_key: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &public_key, line, msg);
			state = S_PRIVATE_KEY;
			break;
		case S_PRIVATE_KEY:
			if (!get_string_cbs(&cbs, "private_key: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &private_key, line, msg);

			failed |= MlkemKeygenFileTest(&seed, &public_key,
			    &private_key, line);

			state = S_START;
			break;
		}
		if (CBS_len(&cbs) > 0)
			errx(1, "#%zu %s: CBS_len", line, msg);
	}
	free(buf);

	if (ferror(fp))
		err(1, NULL);
	fclose(fp);

	return failed;
}
