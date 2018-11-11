/*	$OpenBSD: sm3test.c,v 1.1 2018/11/11 07:12:33 tb Exp $	*/
/*
 * Copyright (c) 2018, Ribose Inc
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>

#define SM3_TESTS 3

const char *sm3_input[SM3_TESTS] = {
	"",
	"abc",
	"abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"
};

const char *sm3_expected[SM3_TESTS] = {
	"1ab21d8355cfa17f8e61194831e81a8f22bec8c728fefb747ed035eb5082aa2b",
	"66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0",
	"debe9ff92275b8a138604889c18e5a4d6fdb70e5387e5765293dcba39c0c5732"
};

char *hex_encode(const uint8_t *, size_t);

char *
hex_encode(const uint8_t *input, size_t len)
{
	const char *hex = "0123456789abcdef";
	char *out;
	size_t i;

	if ((out = malloc(len * 2 + 1)) == NULL)
		err(1, NULL);
	for (i = 0; i < len; i++) {
		out[i * 2]   = hex[input[i] >> 4];
		out[i * 2 + 1] = hex[input[i] & 0x0f];
	}
	out[len * 2] = '\0';

	return out;
}

int
main(int argc, char *argv[])
{
	EVP_MD_CTX *ctx;
	uint8_t digest[32];
	char *hexdigest;
	int numerrors = 0, i;
	
	if ((ctx = EVP_MD_CTX_new()) == NULL)
		err(1, NULL);
	
	for (i = 0; i != SM3_TESTS; ++i) {
		if (!EVP_DigestInit(ctx, EVP_sm3()))
			errx(1, "EVP_DigestInit() failed");
		if (!EVP_DigestUpdate(ctx, sm3_input[i], strlen(sm3_input[i])))
			errx(1, "EVP_DigestInit() failed");
		if (!EVP_DigestFinal(ctx, digest, NULL))
			errx(1, "EVP_DigestFinal() failed");
	
		hexdigest = hex_encode(digest, sizeof(digest));
	
		if (strcmp(hexdigest, sm3_expected[i]) != 0) {
			fprintf(stderr,
			    "TEST %d failed\nProduced %s\nExpected %s\n",
			    i, hexdigest, sm3_expected[i]);
			numerrors++;
		} else
			fprintf(stderr, "SM3 test %d ok\n", i);
		free(hexdigest);
	}

	EVP_MD_CTX_free(ctx);

	return (numerrors > 0) ? 1 : 0;
}
