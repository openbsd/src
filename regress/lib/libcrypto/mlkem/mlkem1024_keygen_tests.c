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

static int
encode_private_key(const struct MLKEM1024_private_key *priv, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	if (!CBB_init(&cbb, MLKEM1024_PUBLIC_KEY_BYTES))
		return 0;
	if (!MLKEM1024_marshal_private_key(&cbb, priv))
		return 0;
	if (!CBB_finish(&cbb, out_buf, out_len))
		return 0;
	CBB_cleanup(&cbb);
	return 1;
}

static void
MlkemKeygenFileTest(CBS *seed, CBS *public_key, CBS *private_key)
{
	struct MLKEM1024_private_key priv;
	uint8_t *encoded_private_key = NULL;
	uint8_t encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES];
	size_t len;

	TEST(CBS_len(seed) != MLKEM_SEED_BYTES, "seed len bogus");
	TEST(CBS_len(private_key) != MLKEM1024_PRIVATE_KEY_BYTES,
	    "expected private key len bogus");
	TEST(CBS_len(public_key) != MLKEM1024_PUBLIC_KEY_BYTES,
	    "expected public key len bogus");
	MLKEM1024_generate_key_external_entropy(encoded_public_key, &priv,
	    CBS_data(seed));
	TEST(!encode_private_key(&priv, &encoded_private_key,
	    &len), "encode_private_key");
	TEST(len != MLKEM1024_PRIVATE_KEY_BYTES, "private key len bogus");
	TEST_DATAEQ(encoded_public_key, CBS_data(public_key),
	    MLKEM1024_PUBLIC_KEY_BYTES, "public key");
	TEST_DATAEQ(encoded_private_key, CBS_data(private_key),
	    MLKEM1024_PRIVATE_KEY_BYTES, "private key");
	free(encoded_private_key);
}

#define S_START 0
#define S_SEED 1
#define S_PUBLIC_KEY  2
#define S_PRIVATE_KEY	3

int
main(int argc, char **argv)
{
	CBS seed, public_key, private_key;
	char *buf;
	FILE *fp;
	int state;

	fprintf(stderr, "Testing keygen test vectors in %s\n", argv[1]);
	TEST((fp = fopen(argv[1], "r")) == NULL, "can't open test file");
	MALLOC(buf, 16*1024);
	state = S_SEED;
	test_number = 1;
	while (fgets(buf, 16*1024, fp) != NULL) {
		switch (state) {
		case S_START:
			if (strcmp(buf, "\n") != 0)
				break;
			state = S_SEED;
			break;
		case S_SEED:
			if (strncmp(buf, "seed: ", strlen("seed: ")) != 0) {
				break;
			}
			grab_data(&seed, buf, strlen("seed: "));
			state = S_PUBLIC_KEY;
			break;
		case S_PUBLIC_KEY:
			if (strncmp(buf, "public_key: ",
			    strlen("public_key: ")) != 0)
				break;
			grab_data(&public_key, buf, strlen("public_key: "));
			state = S_PRIVATE_KEY;
			break;
		case S_PRIVATE_KEY:
			if (strncmp(buf, "private_key: ",
			    strlen("private_key: ")) != 0)
				break;
			grab_data(&private_key, buf, strlen("private_key: "));
			state = S_START;
			break;

			MlkemKeygenFileTest(&seed, &public_key, &private_key);
			free((void *)CBS_data(&seed));
			free((void *)CBS_data(&public_key));
			free((void *)CBS_data(&private_key));

			test_number++;
			state = S_START;
			break;
		}
	}

	free(buf);
	exit(failure);
}
