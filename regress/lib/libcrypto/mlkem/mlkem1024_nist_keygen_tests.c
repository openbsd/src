/*	$OpenBSD: mlkem1024_nist_keygen_tests.c,v 1.4 2024/12/20 00:07:12 tb Exp $ */
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
MlkemNistKeygenFileTest(CBB *z_cbb, CBB *d_cbb, CBB *ek_cbb, CBB *dk_cbb,
    size_t line)
{
	CBB seed_cbb;
	uint8_t *z = NULL, *d = NULL, *ek = NULL, *dk = NULL;
	size_t z_len = 0, d_len = 0, ek_len = 0, dk_len = 0;
	uint8_t seed[MLKEM_SEED_BYTES];
	struct MLKEM1024_private_key priv;
	uint8_t *encoded_private_key = NULL;
	uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
	size_t len;
	int failed = 1;

	if (!CBB_init_fixed(&seed_cbb, seed, sizeof(seed)))
		goto err;

	if (!CBB_finish(z_cbb, &z, &z_len))
		goto err;
	if (!CBB_finish(d_cbb, &d, &d_len))
		goto err;
	if (!CBB_finish(ek_cbb, &ek, &ek_len))
		goto err;
	if (!CBB_finish(dk_cbb, &dk, &dk_len))
		goto err;

	if (!CBB_add_bytes(&seed_cbb, d, d_len))
		goto err;
	if (!CBB_add_bytes(&seed_cbb, z, z_len))
		goto err;
	if (!CBB_finish(&seed_cbb, NULL, &len))
		goto err;

	if (!compare_length(MLKEM_SEED_BYTES, len, line, "z or d length bogus"))
		goto err;

	MLKEM1024_generate_key_external_entropy(encoded_public_key, &priv, seed);

	if (!mlkem1024_encode_private_key(&priv, &encoded_private_key, &len)) {
		warnx("#%zu mlkem1024_encode_private_key", line);
		goto err;
	}

	if (!compare_length(MLKEM1024_PRIVATE_KEY_BYTES, len, line,
	    "private key length"))
		goto err;

	failed = compare_data(ek, encoded_public_key, MLKEM1024_PUBLIC_KEY_BYTES,
	    line, "public key");
	failed |= compare_data(dk, encoded_private_key, MLKEM1024_PRIVATE_KEY_BYTES,
	    line, "private key");

 err:
	CBB_cleanup(&seed_cbb);
	CBB_cleanup(z_cbb);
	CBB_cleanup(d_cbb);
	CBB_cleanup(ek_cbb);
	CBB_cleanup(dk_cbb);
	freezero(z, z_len);
	freezero(d, d_len);
	freezero(ek, ek_len);
	freezero(dk, dk_len);
	free(encoded_private_key);

	return failed;
}

#define S_START		0
#define S_Z		1
#define S_D		2
#define S_EK		3
#define S_DK		4

#define S2S(x) case x: return #x

static const char *
state2str(int state)
{
	switch (state) {
	S2S(S_START);
	S2S(S_Z);
	S2S(S_D);
	S2S(S_EK);
	S2S(S_DK);
	default:
		errx(1, "unknown state %d", state);
	}
}

int
main(int argc, char **argv)
{
	CBB z = { 0 }, d = { 0 }, ek = { 0 }, dk = { 0 };
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

	state = S_Z;
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
			state = S_Z;
			break;
		case S_Z:
			if (!get_string_cbs(&cbs, "z: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &z, line, msg);
			state = S_D;
			break;
		case S_D:
			if (!get_string_cbs(&cbs, "d: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &d, line, msg);
			state = S_EK;
			break;
		case S_EK:
			if (!get_string_cbs(&cbs, "ek: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &ek, line, msg);
			state = S_DK;
			break;
		case S_DK:
			if (!get_string_cbs(&cbs, "dk: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &dk, line, msg);

			failed |= MlkemNistKeygenFileTest(&z, &d, &ek, &dk, line);

			state = S_START;
			break;
		}
	}
	free(buf);

	if (ferror(fp))
		err(1, NULL);
	fclose(fp);

	return failed;
}
