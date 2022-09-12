/*	$OpenBSD: rc2_test.c,v 1.4 2022/09/12 13:09:01 tb Exp $ */
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
#include <openssl/rc2.h>

#include <stdint.h>
#include <string.h>

struct rc2_test {
	const int mode;
	const uint8_t key[64];
	const int key_len;
	const int key_bits;
	const int len;
	const uint8_t in[8];
	const uint8_t out[8];
};

static const struct rc2_test rc2_tests[] = {
	/* ECB (Test vectors from RFC 2268) */
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.key_len = 8,
		.key_bits = 63,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0xeb, 0xb7, 0x73, 0xf9, 0x93, 0x27, 0x8e, 0xff,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		},
		.key_len = 8,
		.key_bits = 64,
		.len = 8,
		.in = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		},
		.out = {
			0x27, 0x8b, 0x27, 0xe4, 0x2e, 0x2f, 0x0d, 0x49,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.key_len = 8,
		.key_bits = 64,
		.len = 8,
		.in = {
			0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		},
		.out = {
			0x30, 0x64, 0x9e, 0xdf, 0x9b, 0xe7, 0xd2, 0xc2,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x88,
		},
		.key_len = 1,
		.key_bits = 64,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x61, 0xa8, 0xa2, 0x44, 0xad, 0xac, 0xcc, 0xf0,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x88, 0xbc, 0xa9, 0x0e, 0x90, 0x87, 0x5a,
		},
		.key_len = 7,
		.key_bits = 64,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x6c, 0xcf, 0x43, 0x08, 0x97, 0x4c, 0x26, 0x7f,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x88, 0xbc, 0xa9, 0x0e, 0x90, 0x87, 0x5a, 0x7f,
			0x0f, 0x79, 0xc3, 0x84, 0x62, 0x7b, 0xaf, 0xb2,
		},
		.key_len = 16,
		.key_bits = 64,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x1a, 0x80, 0x7d, 0x27, 0x2b, 0xbe, 0x5d, 0xb1,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x88, 0xbc, 0xa9, 0x0e, 0x90, 0x87, 0x5a, 0x7f,
			0x0f, 0x79, 0xc3, 0x84, 0x62, 0x7b, 0xaf, 0xb2,
		},
		.key_len = 16,
		.key_bits = 128,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x22, 0x69, 0x55, 0x2a, 0xb0, 0xf8, 0x5c, 0xa6,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x88, 0xbc, 0xa9, 0x0e, 0x90, 0x87, 0x5a, 0x7f,
			0x0f, 0x79, 0xc3, 0x84, 0x62, 0x7b, 0xaf, 0xb2,
			0x16, 0xf8, 0x0a, 0x6f, 0x85, 0x92, 0x05, 0x84,
			0xc4, 0x2f, 0xce, 0xb0, 0xbe, 0x25, 0x5d, 0xaf,
			0x1e,
		},
		.key_len = 33,
		.key_bits = 129,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x5b, 0x78, 0xd3, 0xa4, 0x3d, 0xff, 0xf1, 0xf1,
		},
	},

	/* ECB (Test vectors from http://websites.umich.edu/~x509/ssleay/rrc2.html) */
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.key_len = 16,
		.key_bits = 1024,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x1c, 0x19, 0x8a, 0x83, 0x8d, 0xf0, 0x28, 0xb7,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		},
		.key_len = 16,
		.key_bits = 1024,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x21, 0x82, 0x9C, 0x78, 0xA9, 0xF9, 0xC0, 0x74,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.key_len = 16,
		.key_bits = 1024,
		.len = 8,
		.in = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		},
		.out = {
			0x13, 0xdb, 0x35, 0x17, 0xd3, 0x21, 0x86, 0x9e,
		},
	},
	{
		.mode = NID_rc2_ecb,
		.key = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		},
		.key_len = 16,
		.key_bits = 1024,
		.len = 8,
		.in = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.out = {
			0x50, 0xdc, 0x01, 0x62, 0xbd, 0x75, 0x7f, 0x31,
		},
	},
};

#define N_RC2_TESTS (sizeof(rc2_tests) / sizeof(rc2_tests[0]))

static int
rc2_ecb_test(size_t test_number, const struct rc2_test *rt)
{
	RC2_KEY key;
	uint8_t out[8];

	/* Encryption */
	memset(out, 0, sizeof(out));
	RC2_set_key(&key, rt->key_len, rt->key, rt->key_bits);
	RC2_ecb_encrypt(rt->in, out, &key, 1);

	if (memcmp(rt->out, out, rt->len) != 0) {
		fprintf(stderr, "FAIL (%s:%zu): encryption mismatch\n",
		    SN_rc2_ecb, test_number);
		return 0;
	}

	/* Decryption */
	memset(out, 0, sizeof(out));
	RC2_set_key(&key, rt->key_len, rt->key, rt->key_bits);
	RC2_ecb_encrypt(rt->out, out, &key, 0);

	if (memcmp(rt->in, out, rt->len) != 0) {
		fprintf(stderr, "FAIL (%s:%zu): decryption mismatch\n",
		    SN_rc2_ecb, test_number);
		return 0;
	}

	return 1;
}

static int
rc2_evp_test(size_t test_number, const struct rc2_test *rt, const char *label,
    const EVP_CIPHER *cipher)
{
	EVP_CIPHER_CTX *ctx;
	uint8_t out[512];
	int in_len, out_len, total_len;
	int i;
	int success = 0;

	if ((ctx = EVP_CIPHER_CTX_new()) == NULL) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_CIPHER_CTX_new failed\n",
		    label, test_number);
		goto failed;
	}

	/* EVP encryption */
	total_len = 0;
	memset(out, 0, sizeof(out));
	if (!EVP_EncryptInit(ctx, cipher, NULL, NULL)) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_EncryptInit failed\n",
		    label, test_number);
		goto failed;
	}

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_SET_RC2_KEY_BITS,
	    rt->key_bits, NULL) <= 0) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_CIPHER_CTX_ctrl failed\n",
		    label, test_number);
		goto failed;
	}

	if (!EVP_CIPHER_CTX_set_key_length(ctx, rt->key_len)) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP_CIPHER_CTX_set_key_length failed\n",
		    label, test_number);
		goto failed;
	}

	if (!EVP_CIPHER_CTX_set_padding(ctx, 0)) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP_CIPHER_CTX_set_padding failed\n",
		    label, test_number);
		goto failed;
	}

	if (!EVP_EncryptInit(ctx, NULL, rt->key, NULL)) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_EncryptInit failed\n",
		    label, test_number);
		goto failed;
	}

	for (i = 0; i < rt->len;) {
		in_len = arc4random_uniform(sizeof(rt->len) / 2);
		if (in_len > rt->len - i)
			in_len = rt->len - i;

		if (!EVP_EncryptUpdate(ctx, out + total_len, &out_len,
		    rt->in + i, in_len)) {
			fprintf(stderr,
			    "FAIL (%s:%zu): EVP_EncryptUpdate failed\n",
			    label, test_number);
			goto failed;
		}

		i += in_len;
		total_len += out_len;
	}

	if (!EVP_EncryptFinal_ex(ctx, out + out_len, &out_len)) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_EncryptFinal_ex failed\n",
		    label, test_number);
		goto failed;
	}
	total_len += out_len;

	if (!EVP_CIPHER_CTX_reset(ctx)) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP_CIPHER_CTX_reset failed\n",
		    label, test_number);
		goto failed;
	}

	if (total_len != rt->len) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP encryption length mismatch\n",
		    label, test_number);
		goto failed;
	}

	if (memcmp(rt->out, out, rt->len) != 0) {
		fprintf(stderr, "FAIL (%s:%zu): EVP encryption mismatch\n",
		    label, test_number);
		goto failed;
	}

	/* EVP decryption */
	total_len = 0;
	memset(out, 0, sizeof(out));
	if (!EVP_DecryptInit(ctx, cipher, NULL, NULL)) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_DecryptInit failed\n",
		    label, test_number);
		goto failed;
	}

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_SET_RC2_KEY_BITS,
	    rt->key_bits, NULL) <= 0) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_CIPHER_CTX_ctrl failed\n",
		    label, test_number);
		goto failed;
	}

	if (!EVP_CIPHER_CTX_set_key_length(ctx, rt->key_len)) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP_CIPHER_CTX_set_key_length failed\n",
		    label, test_number);
		goto failed;
	}

	if (!EVP_CIPHER_CTX_set_padding(ctx, 0)) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP_CIPHER_CTX_set_padding failed\n",
		    label, test_number);
		goto failed;
	}

	if (!EVP_DecryptInit(ctx, NULL, rt->key, NULL)) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_DecryptInit failed\n",
		    label, test_number);
		goto failed;
	}

	for (i = 0; i < rt->len;) {
		in_len = arc4random_uniform(sizeof(rt->len) / 2);
		if (in_len > rt->len - i)
			in_len = rt->len - i;

		if (!EVP_DecryptUpdate(ctx, out + total_len, &out_len,
		    rt->out + i, in_len)) {
			fprintf(stderr,
			    "FAIL (%s:%zu): EVP_DecryptUpdate failed\n",
			    label, test_number);
			goto failed;
		}

		i += in_len;
		total_len += out_len;
	}

	if (!EVP_DecryptFinal_ex(ctx, out + total_len, &out_len)) {
		fprintf(stderr, "FAIL (%s:%zu): EVP_DecryptFinal_ex failed\n",
		    label, test_number);
		goto failed;
	}
	total_len += out_len;

	if (!EVP_CIPHER_CTX_reset(ctx)) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP_CIPHER_CTX_reset failed\n",
		    label, test_number);
		goto failed;
	}

	if (total_len != rt->len) {
		fprintf(stderr,
		    "FAIL (%s:%zu): EVP decryption length mismatch\n",
		    label, test_number);
		goto failed;
	}

	if (memcmp(rt->in, out, rt->len) != 0) {
		fprintf(stderr, "FAIL (%s:%zu): EVP decryption mismatch\n",
		    label, test_number);
		goto failed;
	}

	success = 1;

 failed:
	EVP_CIPHER_CTX_free(ctx);
	return success;
}

static int
rc2_test(void)
{
	const struct rc2_test *rt;
	const char *label;
	const EVP_CIPHER *cipher;
	size_t i;
	int failed = 1;

	for (i = 0; i < N_RC2_TESTS; i++) {
		rt = &rc2_tests[i];
		switch (rt->mode) {
		case NID_rc2_ecb:
			label = SN_rc2_ecb;
			cipher = EVP_rc2_ecb();
			if (!rc2_ecb_test(i, rt))
				goto failed;
			break;
		default:
			fprintf(stderr, "FAIL: unknown mode (%d)\n",
			    rt->mode);
			goto failed;
		}

		if (!rc2_evp_test(i, rt, label, cipher))
			goto failed;
	}

	failed = 0;

 failed:
	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= rc2_test();

	return failed;
}
