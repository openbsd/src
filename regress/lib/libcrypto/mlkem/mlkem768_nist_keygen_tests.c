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
encode_private_key(const struct MLKEM768_private_key *priv, uint8_t **out_buf,
    size_t *out_len)
{
	CBB cbb;
	if (!CBB_init(&cbb, MLKEM768_PUBLIC_KEY_BYTES))
		return 0;
	if (!MLKEM768_marshal_private_key(&cbb, priv))
		return 0;
	if (!CBB_finish(&cbb, out_buf, out_len))
		return 0;
	CBB_cleanup(&cbb);
	return 1;
}

static void
MlkemNistKeygenFileTest(CBS *z, CBS *d, CBS *ek, CBS *dk)
{
	uint8_t seed[MLKEM_SEED_BYTES];
	struct MLKEM768_private_key priv;
	uint8_t *encoded_private_key = NULL;
	uint8_t encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES];
	size_t len;

	TEST(CBS_len(d) != (MLKEM_SEED_BYTES / 2), "d len bogus");
	TEST(CBS_len(z) != (MLKEM_SEED_BYTES / 2), "z len bogus");
	TEST(CBS_len(dk) != MLKEM768_PRIVATE_KEY_BYTES,
	    "expected private key len bogus");
	TEST(CBS_len(ek) != MLKEM768_PUBLIC_KEY_BYTES,
	    "expected public key len bogus");
	memcpy(&seed[0], CBS_data(d), CBS_len(d));
	memcpy(&seed[MLKEM_SEED_BYTES / 2], CBS_data(z), CBS_len(z));
	MLKEM768_generate_key_external_entropy(encoded_public_key, &priv, seed);
	TEST(!encode_private_key(&priv, &encoded_private_key,
	    &len), "encode_private_key");
	TEST(len != MLKEM768_PRIVATE_KEY_BYTES, "private key len bogus");
	TEST_DATAEQ(encoded_public_key, CBS_data(ek),
	    MLKEM768_PUBLIC_KEY_BYTES, "public key");
	TEST_DATAEQ(encoded_private_key, CBS_data(dk),
	    MLKEM768_PRIVATE_KEY_BYTES, "private key");
	free(encoded_private_key);
}

#define S_START 0
#define S_Z 1
#define S_D  2
#define S_EK	3
#define S_DK	4

int
main(int argc, char **argv)
{
	CBS z, d, ek, dk;
	char *buf;
	FILE *fp;
	int state;

	fprintf(stderr, "Testing NIST keygen test vectors in %s\n", argv[1]);
	TEST((fp = fopen(argv[1], "r")) == NULL, "can't open test file");
	MALLOC(buf, 16*1024);
	state = S_Z;
	test_number = 1;
	while (fgets(buf, 16*1024, fp) != NULL) {
		switch (state) {
		case S_START:
			if (strcmp(buf, "\n") != 0)
				break;
			state = S_Z;
			break;
		case S_Z:
			if (strncmp(buf, "z: ", strlen("z: ")) != 0) {
				break;
			}
			grab_data(&z, buf, strlen("z: "));
			state = S_D;
			break;
		case S_D:
			if (strncmp(buf, "d: ", strlen("d: ")) != 0)
				break;
			grab_data(&d, buf, strlen("d: "));
			state = S_EK;
			break;
		case S_EK:
			if (strncmp(buf, "ek: ", strlen("ek: ")) != 0)
				break;
			grab_data(&ek, buf, strlen("ek: "));
			state = S_DK;
			break;
		case S_DK:
			if (strncmp(buf, "dk: ", strlen("dk: ")) != 0)
				break;
			grab_data(&dk, buf, strlen("dk: "));

			MlkemNistKeygenFileTest(&z, &d, &ek, &dk);
			free((void *)CBS_data(&z));
			free((void *)CBS_data(&d));
			free((void *)CBS_data(&ek));
			free((void *)CBS_data(&dk));

			test_number++;
			state = S_START;
			break;
		}
	}

	free(buf);
	exit(failure);
}
