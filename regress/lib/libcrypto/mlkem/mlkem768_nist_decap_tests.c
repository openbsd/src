/*	$OpenBSD: mlkem768_nist_decap_tests.c,v 1.3 2024/12/20 00:07:12 tb Exp $ */
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
MlkemNistDecapFileTest(CBB *c_cbb, CBB *k_cbb, CBS *dk, size_t line)
{
	uint8_t *c = NULL, *k = NULL;
	size_t c_len = 0, k_len = 0;
	uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
	struct MLKEM768_private_key priv;
	int failed = 1;

	if (!CBB_finish(c_cbb, &c, &c_len))
		goto err;
	if (!CBB_finish(k_cbb, &k, &k_len))
		goto err;

	if (!compare_length(MLKEM768_PRIVATE_KEY_BYTES, CBS_len(dk), line,
	    "private key len bogus"))
		goto err;
	if (!compare_length(MLKEM_SHARED_SECRET_BYTES, k_len, line,
	    "shared secret len bogus"))
		goto err;

	if (!MLKEM768_parse_private_key(&priv, dk)) {
		warnx("#%zu MLKEM768_parse_private_key", line);
		goto err;
	}
	if (!MLKEM768_decap(shared_secret, c, c_len, &priv)) {
		warnx("#%zu MLKEM768_decap", line);
		goto err;
	}

	failed = compare_data(shared_secret, k, k_len, line, "shared_secret");

 err:
	CBB_cleanup(c_cbb);
	CBB_cleanup(k_cbb);
	freezero(c, c_len);
	freezero(k, k_len);

	return failed;
}

#define S_START		0
#define S_C		1
#define S_K		2
#define S_EMPTY		3

#define S2S(x) case x: return #x

static const char *
state2str(int state)
{
	switch (state) {
	S2S(S_START);
	S2S(S_C);
	S2S(S_K);
	S2S(S_EMPTY);
	default:
		errx(1, "unknown state %d", state);
	}
}

int
main(int argc, char **argv)
{
	CBB dk_cbb = { 0 }, c = { 0 }, k = { 0 };
	CBS instr;
	uint8_t *dk = NULL;
	size_t dk_len = 0;
	uint8_t bracket, newline;
	const char *test;
	size_t line;
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

	if ((len = getline(&buf, &buflen, fp)) == -1)
		err(1, "failed to read instruction line");

	/*
	 * The private key is enclosed in brackets in an "instruction line".
	 */
	line = 1;
	CBS_init(&instr, buf, len);
	if (!CBS_get_u8(&instr, &bracket))
		err(1, "failed to parse instruction line '['");
	assert(bracket == '[');
	if (!CBS_get_last_u8(&instr, &newline))
		errx(1, "failed to parse instruction line '\\n'");
	assert(newline == '\n');
	if (!CBS_get_last_u8(&instr, &bracket))
		errx(1, "failed to parse instruction line ']'");
	assert(bracket == ']');
	if (!get_string_cbs(&instr, "dk: ", line, "private key"))
		errx(1, "failed to read instruction line 'dk: '");
	hex_decode_cbs(&instr, &dk_cbb, line, "private key");
	assert(CBS_len(&instr) == 0);

	if (!CBB_finish(&dk_cbb, &dk, &dk_len))
		errx(1, "CBB finish instruction line");

	state = S_START;

	while ((len = getline(&buf, &buflen, fp)) != -1) {
		const char *msg = state2str(state);
		CBS cbs, dk_cbs;
		uint8_t u8;

		line++;
		CBS_init(&cbs, buf, len);

		if (!CBS_get_last_u8(&cbs, &u8))
			errx(1, "#%zu %s: CBB_get_last_u8", line, msg);
		assert(u8 == '\n');

		switch (state) {
		case S_START:
			state = S_C;
			break;
		case S_C:
			if (!get_string_cbs(&cbs, "c: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &c, line, msg);
			state = S_K;
			break;
		case S_K:
			if (!get_string_cbs(&cbs, "k: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &k, line, msg);
			state = S_EMPTY;
			break;
		case S_EMPTY:
			CBS_init(&dk_cbs, dk, dk_len);

			failed |= MlkemNistDecapFileTest(&c, &k, &dk_cbs, line);

			state = S_C;
			break;
		}
		if (CBS_len(&cbs) > 0)
			errx(1, "#%zu %s: CBS_len", line, msg);
	}
	free(buf);

	if (ferror(fp))
		err(1, NULL);
	fclose(fp);

	freezero(dk, dk_len);

	return failed;
}
