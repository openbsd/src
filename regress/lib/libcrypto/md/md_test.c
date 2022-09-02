/*	$OpenBSD: md_test.c,v 1.1.1.1 2022/09/02 13:34:48 tb Exp $ */
/*
 * Copyright (c) 2022 Joshua Sing <joshua@hypera.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include <openssl/evp.h>
#include <openssl/md4.h>
#include <openssl/md5.h>

#include <stdint.h>
#include <string.h>

struct md_test {
	const int algorithm;
	const uint8_t in[128];
	const size_t in_len;
	const uint8_t out[EVP_MAX_MD_SIZE];
};

static const struct md_test md_tests[] = {
	/* MD4 (RFC 1320 test vectors)  */
	{
		.algorithm = NID_md4,
		.in = "",
		.in_len = 0,
		.out = {
			0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31,
			0xb7, 0x3c, 0x59, 0xd7, 0xe0, 0xc0, 0x89, 0xc0,
		}
	},
	{
		.algorithm = NID_md4,
		.in = "a",
		.in_len = 1,
		.out = {
			0xbd, 0xe5, 0x2c, 0xb3, 0x1d, 0xe3, 0x3e, 0x46,
			0x24, 0x5e, 0x05, 0xfb, 0xdb, 0xd6, 0xfb, 0x24,
		}
	},
	{
		.algorithm = NID_md4,
		.in = "abc",
		.in_len = 3,
		.out = {
			0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52,
			0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d,
		}
	},
	{
		.algorithm = NID_md4,
		.in = "message digest",
		.in_len = 14,
		.out = {
			0xd9, 0x13, 0x0a, 0x81, 0x64, 0x54, 0x9f, 0xe8,
			0x18, 0x87, 0x48, 0x06, 0xe1, 0xc7, 0x01, 0x4b,
		}
	},
	{
		.algorithm = NID_md4,
		.in = "abcdefghijklmnopqrstuvwxyz",
		.in_len = 26,
		.out = {
			0xd7, 0x9e, 0x1c, 0x30, 0x8a, 0xa5, 0xbb, 0xcd,
			0xee, 0xa8, 0xed, 0x63, 0xdf, 0x41, 0x2d, 0xa9,
		}
	},
	{
		.algorithm = NID_md4,
		.in =
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv"
		    "wxyz0123456789",
		.in_len = 62,
		.out = {
			0x04, 0x3f, 0x85, 0x82, 0xf2, 0x41, 0xdb, 0x35,
			0x1c, 0xe6, 0x27, 0xe1, 0x53, 0xe7, 0xf0, 0xe4,
		}
	},
	{
		.algorithm = NID_md4,
		.in =
		    "123456789012345678901234567890123456789012345678"
		    "90123456789012345678901234567890",
		.in_len = 80,
		.out = {
			0xe3, 0x3b, 0x4d, 0xdc, 0x9c, 0x38, 0xf2, 0x19,
			0x9c, 0x3e, 0x7b, 0x16, 0x4f, 0xcc, 0x05, 0x36,
		}
	},

	/* MD5 (RFC 1321 test vectors)  */
	{
		.algorithm = NID_md5,
		.in = "",
		.in_len = 0,
		.out = {
			0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
			0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e,
		}
	},
	{
		.algorithm = NID_md5,
		.in = "a",
		.in_len = 1,
		.out = {
			0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8,
			0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61,
		}
	},
	{
		.algorithm = NID_md5,
		.in = "abc",
		.in_len = 3,
		.out = {
			0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
			0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72,
		}
	},
	{
		.algorithm = NID_md5,
		.in = "message digest",
		.in_len = 14,
		.out = {
			0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d,
			0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0,
		}
	},
	{
		.algorithm = NID_md5,
		.in = "abcdefghijklmnopqrstuvwxyz",
		.in_len = 26,
		.out = {
			0xc3, 0xfc, 0xd3, 0xd7, 0x61, 0x92, 0xe4, 0x00,
			0x7d, 0xfb, 0x49, 0x6c, 0xca, 0x67, 0xe1, 0x3b,
		}
	},
	{
		.algorithm = NID_md5,
		.in =
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv"
		    "wxyz0123456789",
		.in_len = 62,
		.out = {
			0xd1, 0x74, 0xab, 0x98, 0xd2, 0x77, 0xd9, 0xf5,
			0xa5, 0x61, 0x1c, 0x2c, 0x9f, 0x41, 0x9d, 0x9f,
		}
	},
	{
		.algorithm = NID_md5,
		.in =
		    "123456789012345678901234567890123456789012345678"
		    "90123456789012345678901234567890",
		.in_len = 80,
		.out = {
			0x57, 0xed, 0xf4, 0xa2, 0x2b, 0xe3, 0xc9, 0x55,
			0xac, 0x49, 0xda, 0x2e, 0x21, 0x07, 0xb6, 0x7a,
		}
	},
};

#define N_MD_TESTS (sizeof(md_tests) / sizeof(md_tests[0]))

typedef unsigned char *(*md_hash_func)(const unsigned char *, size_t,
    unsigned char *);

static int
md_hash_from_algorithm(int algorithm, const char **out_label,
    md_hash_func *out_func, const EVP_MD **out_md, size_t *out_len)
{
	switch (algorithm) {
	case NID_md4:
		*out_label = SN_md4;
		*out_func = MD4;
		*out_md = EVP_md4();
		*out_len = MD4_DIGEST_LENGTH;
		break;
	case NID_md5:
		*out_label = SN_md5;
		*out_func = MD5;
		*out_md = EVP_md5();
		*out_len = MD5_DIGEST_LENGTH;
		break;
	default:
		fprintf(stderr, "FAIL: unknown algorithm (%d)\n",
			algorithm);
		return 0;
	}

	return 1;
}

static int
md_test(void)
{
	unsigned char *(*md_func)(const unsigned char *, size_t, unsigned char *);
	const struct md_test *st;
	EVP_MD_CTX *hash = NULL;
	const EVP_MD *md;
	uint8_t out[EVP_MAX_MD_SIZE];
	size_t in_len, out_len;
	size_t i;
	const char *label;
	int failed = 1;

	if ((hash = EVP_MD_CTX_new()) == NULL) {
		fprintf(stderr, "FAIL: EVP_MD_CTX_new() failed\n");
		goto failed;
	}

	for (i = 0; i < N_MD_TESTS; i++) {
		st = &md_tests[i];
		if (!md_hash_from_algorithm(st->algorithm, &label, &md_func,
		    &md, &out_len))
			goto failed;

		/* Digest */
		memset(out, 0, sizeof(out));
		md_func(st->in, st->in_len, out);
		if (memcmp(st->out, out, out_len) != 0) {
			fprintf(stderr, "FAIL (%s): mismatch\n", label);
			goto failed;
		}

		/* EVP single-shot digest */
		memset(out, 0, sizeof(out));
		if (!EVP_Digest(st->in, st->in_len, out, NULL, md, NULL)) {
			fprintf(stderr, "FAIL (%s): EVP_Digest failed\n",
			    label);
			goto failed;
		}

		if (memcmp(st->out, out, out_len) != 0) {
			fprintf(stderr, "FAIL (%s): EVP single-shot mismatch\n",
			    label);
			goto failed;
		}

		/* EVP digest */
		memset(out, 0, sizeof(out));
		if (!EVP_DigestInit_ex(hash, md, NULL)) {
			fprintf(stderr, "FAIL (%s): EVP_DigestInit_ex failed\n",
			    label);
			goto failed;
		}

		in_len = st->in_len / 2;
		if (!EVP_DigestUpdate(hash, st->in, in_len)) {
			fprintf(stderr,
			    "FAIL (%s): EVP_DigestUpdate first half failed\n",
			    label);
			goto failed;
		}

		if (!EVP_DigestUpdate(hash, st->in + in_len,
			st->in_len - in_len)) {
			fprintf(stderr,
			    "FAIL (%s): EVP_DigestUpdate second half failed\n",
			    label);
			goto failed;
		}

		if (!EVP_DigestFinal_ex(hash, out, NULL)) {
			fprintf(stderr,
			    "FAIL (%s): EVP_DigestFinal_ex failed\n",
			    label);
			goto failed;
		}

		if (memcmp(st->out, out, out_len) != 0) {
			fprintf(stderr, "FAIL (%s): EVP mismatch\n", label);
			goto failed;
		}
	}

	failed = 0;

 failed:
	EVP_MD_CTX_free(hash);
	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= md_test();

	return failed;
}
