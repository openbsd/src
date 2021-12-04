/* $OpenBSD: ec_asn1_test.c,v 1.2 2021/12/04 17:03:43 tb Exp $ */
/*
 * Copyright (c) 2017, 2021 Joel Sing <jsing@openbsd.org>
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

#include <err.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/objects.h>

const uint8_t ec_secp256r1_pkparameters_named_curve[] = {
	0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03,
	0x01, 0x07,
};

const uint8_t ec_secp256r1_pkparameters_parameters[] = {
	0x30, 0x81, 0xf7, 0x02, 0x01, 0x01, 0x30, 0x2c,
	0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x01,
	0x01, 0x02, 0x21, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x30, 0x5b, 0x04, 0x20,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc,
	0x04, 0x20, 0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a,
	0x93, 0xe7, 0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98,
	0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53,
	0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2,
	0x60, 0x4b, 0x03, 0x15, 0x00, 0xc4, 0x9d, 0x36,
	0x08, 0x86, 0xe7, 0x04, 0x93, 0x6a, 0x66, 0x78,
	0xe1, 0x13, 0x9d, 0x26, 0xb7, 0x81, 0x9f, 0x7e,
	0x90, 0x04, 0x41, 0x04, 0x6b, 0x17, 0xd1, 0xf2,
	0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5,
	0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81,
	0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45,
	0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2,
	0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a,
	0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57,
	0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68,
	0x37, 0xbf, 0x51, 0xf5, 0x02, 0x21, 0x00, 0xff,
	0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbc,
	0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3,
	0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51, 0x02,
	0x01, 0x01,
};

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");

	fprintf(stderr, "\n");
}

static int
compare_data(const char *label, const unsigned char *d1, size_t d1_len,
    const unsigned char *d2, size_t d2_len)
{
	if (d1_len != d2_len) {
		fprintf(stderr, "FAIL: got %s with length %zu, want %zu\n",
		    label, d1_len, d2_len);
		return -1;
	}
	if (memcmp(d1, d2, d1_len) != 0) {
		fprintf(stderr, "FAIL: %sdiffer\n", label);
		fprintf(stderr, "got:\n");
		hexdump(d1, d1_len);
		fprintf(stderr, "want:\n");
		hexdump(d2, d2_len);
		return -1;
	}
	return 0;
}

static int
ec_group_pkparameters_test(const char *label, int asn1_flag,
    const uint8_t *test_data, size_t test_data_len)
{
	EC_GROUP *group_a = NULL, *group_b = NULL;
	unsigned char *out = NULL, *data = NULL;
	const unsigned char *p;
	BIO *bio_mem = NULL;
	int failure = 1;
	int len;

	/*
	 * Test i2d_ECPKParameters/d2i_ECPKParameters.
	 */
	if ((group_a = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1)) == NULL)
		errx(1, "failed to create EC_GROUP");

	EC_GROUP_set_asn1_flag(group_a, asn1_flag);

	if ((len = i2d_ECPKParameters(group_a, &out)) < 0) {
		fprintf(stderr, "FAIL: i2d_ECPKParameters failed\n");
		goto done;
	}
	if (compare_data(label, out, len, test_data, test_data_len) == -1)
		goto done;

	p = out;
	if ((group_b = d2i_ECPKParameters(NULL, &p, len)) == NULL) {
		fprintf(stderr, "FAIL: d2i_ECPKParameters failed\n");
		goto done;
	}

	if (EC_GROUP_cmp(group_a, group_b, NULL) != 0) {
		fprintf(stderr, "FAIL: EC_GROUPs do not match!\n");
		goto done;
	}

	p = out;
	if ((group_a = d2i_ECPKParameters(&group_a, &p, len)) == NULL) {
		fprintf(stderr, "FAIL: d2i_ECPKParameters failed\n");
		goto done;
	}

	if (EC_GROUP_cmp(group_a, group_b, NULL) != 0) {
		fprintf(stderr, "FAIL: EC_GROUPs do not match!\n");
		goto done;
	}

	/*
	 * Test i2d_ECPKParameters_bio/d2i_ECPKParameters_bio.
	 */
	if ((bio_mem = BIO_new(BIO_s_mem())) == NULL)
                errx(1, "BIO_new failed for BIO_s_mem");

	if ((len = i2d_ECPKParameters_bio(bio_mem, group_a)) < 0) {
		fprintf(stderr, "FAIL: i2d_ECPKParameters_bio failed\n");
		goto done;
	}

	len = BIO_get_mem_data(bio_mem, &data);
	if (compare_data(label, out, len, test_data, test_data_len) == -1)
		goto done;

	EC_GROUP_free(group_b);
	if ((group_b = d2i_ECPKParameters_bio(bio_mem, NULL)) == NULL) {
		fprintf(stderr, "FAIL: d2i_ECPKParameters_bio failed\n");
		goto done;
	}

	if (EC_GROUP_cmp(group_a, group_b, NULL) != 0) {
		fprintf(stderr, "FAIL: EC_GROUPs do not match!\n");
		goto done;
	}

	failure = 0;

 done:
	BIO_free_all(bio_mem);
	EC_GROUP_free(group_a);
	EC_GROUP_free(group_b);
	free(out);

	return (failure);
}

static int
ec_group_pkparameters_named_curve_test(void)
{
	return ec_group_pkparameters_test("ECPKPARAMETERS named curve",
	    OPENSSL_EC_NAMED_CURVE, ec_secp256r1_pkparameters_named_curve,
	    sizeof(ec_secp256r1_pkparameters_named_curve));
}

static int
ec_group_pkparameters_parameters_test(void)
{
	return ec_group_pkparameters_test("ECPKPARAMETERS parameters",
	    OPENSSL_EC_EXPLICIT_CURVE, ec_secp256r1_pkparameters_parameters,
	    sizeof(ec_secp256r1_pkparameters_parameters));
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= ec_group_pkparameters_named_curve_test();
	failed |= ec_group_pkparameters_parameters_test();

	return (failed);
}
