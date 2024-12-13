/* Copyright (c) 2024, Google Inc.
 * Copyright (c) 2024, Bob Beck <beck@obtuse.com>
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

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>

#include "bytestring.h"
#include "mlkem.h"
#include "mlkem_internal.h"
#include "sha3_internal.h"
#include "mlkem_tests_util.h"

static void
MlkemEncapFileTest(CBS *entropy, CBS *public_key, CBS *expected_ciphertext,
    CBS *expected_shared_secret, int should_fail)
{
	uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
	uint8_t ciphertext[MLKEM1024_CIPHERTEXT_BYTES];
	struct MLKEM1024_public_key pub;
	int parse_ok;

	parse_ok = MLKEM1024_parse_public_key(&pub, public_key);
	if (!parse_ok) {
		TEST(!should_fail, "parse_public_key");
		return;
	}
	MLKEM1024_encap(ciphertext, shared_secret, &pub);
	TEST_DATAEQ(shared_secret, CBS_data(expected_shared_secret),
	    MLKEM_SHARED_SECRET_BYTES, "shared_secret");
	TEST_DATAEQ(ciphertext, CBS_data(expected_ciphertext),
	    MLKEM1024_CIPHERTEXT_BYTES, "shared_secret");
}

#define S_START 0
#define S_COMMENT 1
#define S_ENTROPY 2
#define S_PUBLIC_KEY 3
#define S_RESULT 4
#define S_CIPHERTEXT 5
#define S_SHARED_SECRET  6

int
main(int argc, char **argv)
{
	CBS entropy, public_key, ciphertext, shared_secret;
	const uint8_t *p = NULL;
	int should_fail = 0;
	char *buf;
	FILE *fp;
	int state;

	fprintf(stderr, "Testing encap test vectors in %s\n", argv[1]);
	TEST((fp = fopen(argv[1], "r")) == NULL, "can't open test file");
	MALLOC(buf, 16*1024);
	state = S_COMMENT;
	test_number = 1;
	while (fgets(buf, 16*1024, fp) != NULL) {
		switch (state) {
		case S_START:
			if (strcmp(buf, "\n") != 0)
				break;
			state = S_COMMENT;
			break;
		case S_COMMENT:
			if (strncmp(buf, "#", 1) != 0)
				break;
			state = S_ENTROPY;
			break;
		case S_ENTROPY:
			if (strncmp(buf, "entropy: ", strlen("entropy: ")) != 0)
				break;
			grab_data(&entropy, buf, strlen("entropy: "));
			p = CBS_data(&entropy);
			state = S_PUBLIC_KEY;
			break;
		case S_PUBLIC_KEY:
			if (strncmp(buf, "public_key: ",
			    strlen("public_key: ")) != 0)
				break;
			grab_data(&public_key, buf, strlen("public_key: "));
			p = CBS_data(&public_key);
			state = S_RESULT;
			break;
		case S_RESULT:
			if (strncmp(buf, "result: pass",
			    strlen("result: pass")) != 0)
				should_fail = 1;
			else
				should_fail = 0;
			state = S_CIPHERTEXT;
			break;
		case S_CIPHERTEXT:
			if (strncmp(buf, "ciphertext: ",
			    strlen("ciphertext: ")) != 0)
				break;
			grab_data(&ciphertext, buf, strlen("ciphertext: "));
			state = S_RESULT;
			break;
		case S_SHARED_SECRET:
			if (strncmp(buf, "shared_secret: ",
			    strlen("shared_secret: ")) != 0)
				break;
			grab_data(&shared_secret, buf,
			    strlen("shared_secret: "));
			MlkemEncapFileTest(&entropy, &public_key, &ciphertext,
			    &shared_secret, should_fail);
			free((void *)CBS_data(&ciphertext));
			free((void *)CBS_data(&shared_secret));
			free((void *)p);

			test_number++;
			state = S_START;
			break;
		}
	}

	free(buf);
	exit(failure);
}
