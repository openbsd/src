/*	$OpenBSD: mlkem1024_decap_tests.c,v 1.3 2024/12/20 00:07:12 tb Exp $ */
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

#include "mlkem_tests_util.h"

static int
MlkemDecapFileTest(CBB *ciphertext_cbb, CBB *shared_secret_cbb,
    CBB *private_key_cbb, int should_fail, size_t line)
{
	struct MLKEM1024_private_key priv;
	uint8_t *ciphertext = NULL, *shared_secret = NULL, *private_key = NULL;
	size_t ciphertext_len = 0, shared_secret_len = 0, private_key_len = 0;
	uint8_t shared_secret_buf[MLKEM_SHARED_SECRET_BYTES];
	CBS private_key_cbs;
	int failed = 1;

	if (!CBB_finish(ciphertext_cbb, &ciphertext, &ciphertext_len))
		goto err;
	if (!CBB_finish(shared_secret_cbb, &shared_secret, &shared_secret_len))
		goto err;
	if (!CBB_finish(private_key_cbb, &private_key, &private_key_len))
		goto err;

	CBS_init(&private_key_cbs, private_key, private_key_len);

	if (!MLKEM1024_parse_private_key(&priv, &private_key_cbs)) {
		if ((failed = !should_fail))
			warnx("#%zu: parse_private_key", line);
		goto err;
	}
	if (!MLKEM1024_decap(shared_secret_buf, ciphertext, ciphertext_len,
	    &priv)) {
		if ((failed = !should_fail))
			warnx("#%zu: decap", line);
		goto err;
	}

	failed = compare_data(shared_secret, shared_secret_buf,
	    MLKEM_SHARED_SECRET_BYTES, line, "shared_secret");

	if (should_fail != failed) {
		warnx("FAIL: #%zu: should_fail %d, failed %d",
		    line, should_fail, failed);
		failed = 1;
	}

 err:
	CBB_cleanup(ciphertext_cbb);
	CBB_cleanup(shared_secret_cbb);
	CBB_cleanup(private_key_cbb);
	freezero(ciphertext, ciphertext_len);
	freezero(shared_secret, shared_secret_len);
	freezero(private_key, private_key_len);

	return failed;
}

#define S_START			0
#define S_COMMENT		1
#define S_PRIVATE_KEY		2
#define S_CIPHERTEXT		3
#define S_RESULT		4
#define S_SHARED_SECRET		5

#define S2S(x) case x: return #x

static const char *
state2str(int state)
{
	switch (state) {
	S2S(S_START);
	S2S(S_COMMENT);
	S2S(S_PRIVATE_KEY);
	S2S(S_CIPHERTEXT);
	S2S(S_RESULT);
	S2S(S_SHARED_SECRET);
	default:
		errx(1, "unknown state %d", state);
	}
}

int
main(int argc, char **argv)
{
	CBB ciphertext = { 0 }, shared_secret = { 0 }, private_key = { 0 };
	int should_fail = 0;
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
			state = S_PRIVATE_KEY;
			break;
		case S_PRIVATE_KEY:
			if (!get_string_cbs(&cbs, "private_key: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &private_key, line, msg);
			state = S_CIPHERTEXT;
			break;
		case S_CIPHERTEXT:
			if (!get_string_cbs(&cbs, "ciphertext: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &ciphertext, line, msg);
			state = S_RESULT;
			break;
		case S_RESULT:
			if (!get_string_cbs(&cbs, "result: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			should_fail = get_string_cbs(&cbs, "fail", line, msg);
			state = S_SHARED_SECRET;
			break;
		case S_SHARED_SECRET:
			if (!get_string_cbs(&cbs, "shared_secret: ", line, msg))
				errx(1, "#%zu %s: get_string_cbs", line, msg);
			hex_decode_cbs(&cbs, &shared_secret, line, msg);

			failed |= MlkemDecapFileTest(&ciphertext, &shared_secret,
			    &private_key, should_fail, line);

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
