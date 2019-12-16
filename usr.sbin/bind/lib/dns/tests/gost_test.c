/*
 * Copyright (C) 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: gost_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/* ! \file */

#include <config.h>

#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include <isc/util.h>
#include <isc/print.h>
#include <isc/string.h>

#include "dnstest.h"

#ifdef HAVE_OPENSSL_GOST
#include "../dst_gost.h"
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/bn.h>
#endif

#ifdef HAVE_PKCS11_GOST
#include "../dst_gost.h"
#include <pk11/internal.h>
#define WANT_GOST_PARAMS
#include <pk11/constants.h>
#include <pkcs11/pkcs11.h>
#endif

#if defined(HAVE_OPENSSL_GOST) || defined(HAVE_PKCS11_GOST)
/*
 * Test data from Wikipedia GOST (hash function)
 */

unsigned char digest[ISC_GOST_DIGESTLENGTH];
unsigned char buffer[1024];
const char *s;
char str[2 * ISC_GOST_DIGESTLENGTH + 1];
int i = 0;

isc_result_t
tohexstr(unsigned char *d, unsigned int len, char *out);
/*
 * Precondition: a hexadecimal number in *d, the length of that number in len,
 *   and a pointer to a character array to put the output (*out).
 * Postcondition: A String representation of the given hexadecimal number is
 *   placed into the array *out
 *
 * 'out' MUST point to an array of at least len * 2 + 1
 *
 * Return values: ISC_R_SUCCESS if the operation is sucessful
 */

isc_result_t
tohexstr(unsigned char *d, unsigned int len, char *out) {

	out[0]='\0';
	char c_ret[] = "AA";
	unsigned int j;
	strcat(out, "0x");
	for (j = 0; j < len; j++) {
		sprintf(c_ret, "%02X", d[j]);
		strcat(out, c_ret);
	}
	strcat(out, "\0");
	return (ISC_R_SUCCESS);
}


#define TEST_INPUT(x) (x), sizeof(x)-1

typedef struct hash_testcase {
	const char *input;
	size_t input_len;
	const char *result;
	int repeats;
} hash_testcase_t;

ATF_TC(isc_gost_md);
ATF_TC_HEAD(isc_gost_md, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "GOST R 34.11-94 examples from Wikipedia");
}
ATF_TC_BODY(isc_gost_md, tc) {
	isc_gost_t gost;
	isc_result_t result;

	UNUSED(tc);

	/*
	 * These are the various test vectors.  All of these are passed
	 * through the hash function and the results are compared to the
	 * result specified here.
	 */
	hash_testcase_t testcases[] = {
		/* Test 1 */
		{
			TEST_INPUT(""),
			"0x981E5F3CA30C841487830F84FB433E1"
			"3AC1101569B9C13584AC483234CD656C0",
			1
		},
		/* Test 2 */
		{
			TEST_INPUT("a"),
			"0xE74C52DD282183BF37AF0079C9F7805"
			"5715A103F17E3133CEFF1AACF2F403011",
			1
		},
		/* Test 3 */
		{
			TEST_INPUT("abc"),
			"0xB285056DBF18D7392D7677369524DD1"
			"4747459ED8143997E163B2986F92FD42C",
			1
		},
		/* Test 4 */
		{
			TEST_INPUT("message digest"),
			"0xBC6041DD2AA401EBFA6E9886734174F"
			"EBDB4729AA972D60F549AC39B29721BA0",
			1
		},
		/* Test 5 */
		{
			TEST_INPUT("The quick brown fox jumps "
				   "over the lazy dog"),
			"0x9004294A361A508C586FE53D1F1B027"
			"46765E71B765472786E4770D565830A76",
			1
		},

		/* Test 6 */
		{
			TEST_INPUT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcde"
				   "fghijklmnopqrstuvwxyz0123456789"),
			"0x73B70A39497DE53A6E08C67B6D4DB85"
			"3540F03E9389299D9B0156EF7E85D0F61",
			1
		},
		/* Test 7 */
		{
			TEST_INPUT("1234567890123456789012345678901"
				   "2345678901234567890123456789012"
				   "345678901234567890"),
			"0x6BC7B38989B28CF93AE8842BF9D7529"
			"05910A7528A61E5BCE0782DE43E610C90",
			1
		},
		/* Test 8 */
		{
			TEST_INPUT("This is message, length=32 bytes"),
			"0x2CEFC2F7B7BDC514E18EA57FA74FF35"
			"7E7FA17D652C75F69CB1BE7893EDE48EB",
			1
		},
		/* Test 9 */
		{
			TEST_INPUT("Suppose the original message "
				   "has length = 50 bytes"),
			"0xC3730C5CBCCACF915AC292676F21E8B"
			"D4EF75331D9405E5F1A61DC3130A65011",
			1
		},
		/* Test 10 */
		{
			TEST_INPUT("U") /* times 128 */,
			"0x1C4AC7614691BBF427FA2316216BE8F"
			"10D92EDFD37CD1027514C1008F649C4E8",
			128
		},
		/* Test 11 */
		{
			TEST_INPUT("a") /* times 1000000 */,
			"0x8693287AA62F9478F7CB312EC0866B6"
			"C4E4A0F11160441E8F4FFCD2715DD554F",
			1000000
		},
		{ NULL, 0, NULL, 1 }
	};

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	hash_testcase_t *testcase = testcases;

	while (testcase->input != NULL && testcase->result != NULL) {
		result = isc_gost_init(&gost);
		ATF_REQUIRE(result == ISC_R_SUCCESS);
		for(i = 0; i < testcase->repeats; i++) {
			result = isc_gost_update(&gost,
					(const isc_uint8_t *) testcase->input,
					testcase->input_len);
			ATF_REQUIRE(result == ISC_R_SUCCESS);
		}
		result = isc_gost_final(&gost, digest);
		ATF_REQUIRE(result == ISC_R_SUCCESS);
		tohexstr(digest, ISC_GOST_DIGESTLENGTH, str);
		ATF_CHECK_STREQ(str, testcase->result);

		testcase++;
	}

	dns_test_end();
}

ATF_TC(isc_gost_private);
ATF_TC_HEAD(isc_gost_private, tc) {
  atf_tc_set_md_var(tc, "descr", "GOST R 34.10-2001 private key");
}
ATF_TC_BODY(isc_gost_private, tc) {
	isc_result_t result;
	unsigned char privraw[31] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e
	};
#ifdef HAVE_OPENSSL_GOST
	unsigned char rbuf[32];
	unsigned char privasn1[70] = {
		0x30, 0x44, 0x02, 0x01, 0x00, 0x30, 0x1c, 0x06,
		0x06, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x13, 0x30,
		0x12, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02, 0x02,
		0x23, 0x01, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02,
		0x02, 0x1e, 0x01, 0x04, 0x21, 0x02, 0x1f, 0x01,
		0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
		0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
		0x1a, 0x1b, 0x1c, 0x1d, 0x1e
	};
	unsigned char abuf[71];
	unsigned char gost_dummy_key[71] = {
		0x30, 0x45, 0x02, 0x01, 0x00, 0x30, 0x1c, 0x06,
		0x06, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x13, 0x30,
		0x12, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02, 0x02,
		0x23, 0x01, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02,
		0x02, 0x1e, 0x01, 0x04, 0x22, 0x02, 0x20, 0x1b,
		0x3f, 0x94, 0xf7, 0x1a, 0x5f, 0x2f, 0xe7, 0xe5,
		0x74, 0x0b, 0x8c, 0xd4, 0xb7, 0x18, 0xdd, 0x65,
		0x68, 0x26, 0xd1, 0x54, 0xfb, 0x77, 0xba, 0x63,
		0x72, 0xd9, 0xf0, 0x63, 0x87, 0xe0, 0xd6
	};
	EVP_PKEY *pkey;
	EC_KEY *eckey;
	BIGNUM *privkey;
	const BIGNUM *privkey1;
	const unsigned char *p;
	int len;
	unsigned char *q;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* raw parse */
	privkey = BN_bin2bn(privraw, (int) sizeof(privraw), NULL);
	ATF_REQUIRE(privkey != NULL);
	p = gost_dummy_key;
	pkey = NULL;
	ATF_REQUIRE(d2i_PrivateKey(NID_id_GostR3410_2001, &pkey, &p,
				   (long) sizeof(gost_dummy_key)) != NULL);
	ATF_REQUIRE(pkey != NULL);
	ATF_REQUIRE(EVP_PKEY_bits(pkey) == 256);
	eckey = EVP_PKEY_get0(pkey);
	ATF_REQUIRE(eckey != NULL);
	ATF_REQUIRE(EC_KEY_set_private_key(eckey, privkey) == 1);
	BN_clear_free(privkey);

	/* asn1 tofile */
	len = i2d_PrivateKey(pkey, NULL);
	ATF_REQUIRE(len == 70);
	q = abuf;
	ATF_REQUIRE(i2d_PrivateKey(pkey, &q) == len);
	ATF_REQUIRE(memcmp(abuf, privasn1, len) == 0);
	EVP_PKEY_free(pkey);

	/* asn1 parse */
	p = privasn1;
	pkey = NULL;
	ATF_REQUIRE(d2i_PrivateKey(NID_id_GostR3410_2001, &pkey, &p,
				   (long) len) != NULL);
	ATF_REQUIRE(pkey != NULL);
	eckey = EVP_PKEY_get0(pkey);
	ATF_REQUIRE(eckey != NULL);
	privkey1 = EC_KEY_get0_private_key(eckey);
	len = BN_num_bytes(privkey1);
	ATF_REQUIRE(len == 31);
	ATF_REQUIRE(BN_bn2bin(privkey1, rbuf) == len);
	ATF_REQUIRE(memcmp(rbuf, privraw, len) == 0);

	dns_test_end();
#else
	CK_BBOOL truevalue = TRUE;
	CK_BBOOL falsevalue = FALSE;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_GOSTR3410;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_VALUE, privraw, sizeof(privraw) },
		{ CKA_GOSTR3410_PARAMS, pk11_gost_a_paramset,
		  (CK_ULONG) sizeof(pk11_gost_a_paramset) },
		{ CKA_GOSTR3411_PARAMS, pk11_gost_paramset,
		  (CK_ULONG) sizeof(pk11_gost_paramset) }
	};
	CK_MECHANISM mech = { CKM_GOSTR3410_WITH_GOSTR3411, NULL, 0 };
	CK_BYTE sig[64];
	CK_ULONG siglen;
	pk11_context_t pk11_ctx;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* create the private key */
	memset(&pk11_ctx, 0, sizeof(pk11_ctx));
	ATF_REQUIRE(pk11_get_session(&pk11_ctx, OP_GOST, ISC_TRUE,
				     ISC_FALSE, ISC_FALSE, NULL,
				     pk11_get_best_token(OP_GOST)) ==
		    ISC_R_SUCCESS);
	pk11_ctx.object = CK_INVALID_HANDLE;
	pk11_ctx.ontoken = ISC_FALSE;
	ATF_REQUIRE(pkcs_C_CreateObject(pk11_ctx.session, keyTemplate,
					(CK_ULONG) 9, &pk11_ctx.object) ==
		    CKR_OK);
	ATF_REQUIRE(pk11_ctx.object != CK_INVALID_HANDLE);

	/* sign something */
	ATF_REQUIRE(pkcs_C_SignInit(pk11_ctx.session, &mech,
				    pk11_ctx.object) == CKR_OK);
	siglen = 0;
	ATF_REQUIRE(pkcs_C_Sign(pk11_ctx.session, sig, 64,
				NULL, &siglen) == CKR_OK);
	ATF_REQUIRE(siglen == 64);
	ATF_REQUIRE(pkcs_C_Sign(pk11_ctx.session, sig, 64,
				sig, &siglen) == CKR_OK);
	ATF_REQUIRE(siglen == 64);

	dns_test_end();
#endif
};
#else
ATF_TC(untested);
ATF_TC_HEAD(untested, tc) {
	atf_tc_set_md_var(tc, "descr", "skipping gost test");
}
ATF_TC_BODY(untested, tc) {
	UNUSED(tc);
	atf_tc_skip("GOST not available");
}
#endif
/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
#if defined(HAVE_OPENSSL_GOST) || defined(HAVE_PKCS11_GOST)
	ATF_TP_ADD_TC(tp, isc_gost_md);
	ATF_TP_ADD_TC(tp, isc_gost_private);
#else
	ATF_TP_ADD_TC(tp, untested);
#endif
	return (atf_no_error());
}

