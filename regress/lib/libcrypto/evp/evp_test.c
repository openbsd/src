/*	$OpenBSD: evp_test.c,v 1.11 2023/12/10 19:20:06 tb Exp $ */
/*
 * Copyright (c) 2022 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2023 Theo Buehler <tb@openbsd.org>
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/ossl_typ.h>

static int
evp_asn1_method_test(void)
{
	const EVP_PKEY_ASN1_METHOD *method;
	int count, pkey_id, i;
	int failed = 1;

	if ((count = EVP_PKEY_asn1_get_count()) < 1) {
		fprintf(stderr, "FAIL: failed to get pkey asn1 method count\n");
		goto failure;
	}
	for (i = 0; i < count; i++) {
		if ((method = EVP_PKEY_asn1_get0(i)) == NULL) {
			fprintf(stderr, "FAIL: failed to get pkey %d\n", i);
			goto failure;
		}
	}

	if ((method = EVP_PKEY_asn1_find(NULL, EVP_PKEY_RSA)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA method\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA);
		goto failure;
	}

	if ((method = EVP_PKEY_asn1_find(NULL, EVP_PKEY_RSA_PSS)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA-PSS method\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA-PSS method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA_PSS) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA_PSS);
		goto failure;
	}

	if ((method = EVP_PKEY_asn1_find_str(NULL, "RSA", -1)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA method by str\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA);
		goto failure;
	}

	if ((method = EVP_PKEY_asn1_find_str(NULL, "RSA-PSS", -1)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA-PSS method\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA-PSS method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA_PSS) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA_PSS);
		goto failure;
	}

	failed = 0;

 failure:

	return failed;
}

static int
evp_pkey_method_test(void)
{
	const EVP_PKEY_METHOD *method;
	int pkey_id;
	int failed = 1;

	if ((method = EVP_PKEY_meth_find(EVP_PKEY_RSA)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA method\n");
		goto failure;
	}
	EVP_PKEY_meth_get0_info(&pkey_id, NULL, method);
	if (pkey_id != EVP_PKEY_RSA) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA);
		goto failure;
	}

	if ((method = EVP_PKEY_meth_find(EVP_PKEY_RSA_PSS)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA-PSS method\n");
		goto failure;
	}
	EVP_PKEY_meth_get0_info(&pkey_id, NULL, method);
	if (pkey_id != EVP_PKEY_RSA_PSS) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA_PSS);
		goto failure;
	}

	failed = 0;

 failure:

	return failed;
}

static const struct evp_iv_len_test {
	const EVP_CIPHER *(*cipher)(void);
	int iv_len;
	int setlen;
	int expect;
} evp_iv_len_tests[] = {
	{
		.cipher = EVP_aes_128_ccm,
		.iv_len = 7,
		.setlen = 11,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_128_ccm,
		.iv_len = 7,
		.setlen = 6,
		.expect = 0,
	},
	{
		.cipher = EVP_aes_128_ccm,
		.iv_len = 7,
		.setlen = 13,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_128_ccm,
		.iv_len = 7,
		.setlen = 14,
		.expect = 0,
	},

	{
		.cipher = EVP_aes_192_ccm,
		.iv_len = 7,
		.setlen = 11,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_192_ccm,
		.iv_len = 7,
		.setlen = 6,
		.expect = 0,
	},
	{
		.cipher = EVP_aes_192_ccm,
		.iv_len = 7,
		.setlen = 13,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_192_ccm,
		.iv_len = 7,
		.setlen = 14,
		.expect = 0,
	},

	{
		.cipher = EVP_aes_256_ccm,
		.iv_len = 7,
		.setlen = 11,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_256_ccm,
		.iv_len = 7,
		.setlen = 6,
		.expect = 0,
	},
	{
		.cipher = EVP_aes_256_ccm,
		.iv_len = 7,
		.setlen = 13,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_256_ccm,
		.iv_len = 7,
		.setlen = 14,
		.expect = 0,
	},

	{
		.cipher = EVP_aes_128_gcm,
		.iv_len = 12,
		.setlen = 16,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_128_gcm,
		.iv_len = 12,
		.setlen = 0,
		.expect = 0,
	},
	{
		.cipher = EVP_aes_128_gcm,
		.iv_len = 12,
		.setlen = 1,
		.expect = 1,
	},
	/* XXX - GCM IV length isn't capped... */
	{
		.cipher = EVP_aes_128_gcm,
		.iv_len = 12,
		.setlen = 1024 * 1024,
		.expect = 1,
	},

	{
		.cipher = EVP_aes_192_gcm,
		.iv_len = 12,
		.setlen = 16,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_192_gcm,
		.iv_len = 12,
		.setlen = 0,
		.expect = 0,
	},
	{
		.cipher = EVP_aes_192_gcm,
		.iv_len = 12,
		.setlen = 1,
		.expect = 1,
	},
	/* XXX - GCM IV length isn't capped... */
	{
		.cipher = EVP_aes_128_gcm,
		.iv_len = 12,
		.setlen = 1024 * 1024,
		.expect = 1,
	},

	{
		.cipher = EVP_aes_256_gcm,
		.iv_len = 12,
		.setlen = 16,
		.expect = 1,
	},
	{
		.cipher = EVP_aes_256_gcm,
		.iv_len = 12,
		.setlen = 0,
		.expect = 0,
	},
	{
		.cipher = EVP_aes_256_gcm,
		.iv_len = 12,
		.setlen = 1,
		.expect = 1,
	},
	/* XXX - GCM IV length isn't capped... */
	{
		.cipher = EVP_aes_128_gcm,
		.iv_len = 12,
		.setlen = 1024 * 1024,
		.expect = 1,
	},

	{
		.cipher = EVP_aes_128_ecb,
		.iv_len = 0,
		.setlen = 11,
		.expect = 0,
	},

	{
		.cipher = EVP_chacha20_poly1305,
		.iv_len = 12,
		.setlen = 11,
		.expect = 1,
	},
	{
		.cipher = EVP_chacha20_poly1305,
		.iv_len = 12,
		.setlen = 12,
		.expect = 1,
	},
	{
		.cipher = EVP_chacha20_poly1305,
		.iv_len = 12,
		.setlen = 13,
		.expect = 0,
	},
	{
		.cipher = EVP_chacha20_poly1305,
		.iv_len = 12,
		.setlen = 1,
		.expect = 1,
	},
	{
		.cipher = EVP_chacha20_poly1305,
		.iv_len = 12,
		.setlen = 0,
		.expect = 0,
	},
};

#define N_EVP_IV_LEN_TESTS \
    (sizeof(evp_iv_len_tests) / sizeof(evp_iv_len_tests[0]))

static int
evp_pkey_iv_len_testcase(const struct evp_iv_len_test *test)
{
	const EVP_CIPHER *cipher = test->cipher();
	const char *name;
	EVP_CIPHER_CTX *ctx;
	int ret;
	int failure = 1;

	assert(cipher != NULL);
	name = OBJ_nid2ln(EVP_CIPHER_nid(cipher));
	assert(name != NULL);

	if ((ctx = EVP_CIPHER_CTX_new()) == NULL) {
		fprintf(stderr, "FAIL: %s: EVP_CIPHER_CTX_new()\n", name);
		goto failure;
	}

	if ((ret = EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL)) <= 0) {
		fprintf(stderr, "FAIL: %s: EVP_EncryptInit_ex:"
		    " want %d, got %d\n", name, 1, ret);
		goto failure;
	}
	if ((ret = EVP_CIPHER_CTX_iv_length(ctx)) != test->iv_len) {
		fprintf(stderr, "FAIL: %s EVP_CIPHER_CTX_iv_length (before set)"
		    " want %d, got %d\n", name, test->iv_len, ret);
		goto failure;
	}
	if ((ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
	    test->setlen, NULL)) != test->expect) {
		fprintf(stderr, "FAIL: %s EVP_CIPHER_CTX_ctrl"
		    " want %d, got %d\n", name, test->expect, ret);
		goto failure;
	}
	if (test->expect == 0)
		goto done;
	if ((ret = EVP_CIPHER_CTX_iv_length(ctx)) != test->setlen) {
		fprintf(stderr, "FAIL: %s EVP_CIPHER_CTX_iv_length (after set)"
		    " want %d, got %d\n", name, test->setlen, ret);
		goto failure;
	}

 done:
	failure = 0;

 failure:
	EVP_CIPHER_CTX_free(ctx);

	return failure;
}

static int
evp_pkey_iv_len_test(void)
{
	size_t i;
	int failure = 0;

	for (i = 0; i < N_EVP_IV_LEN_TESTS; i++)
		failure |= evp_pkey_iv_len_testcase(&evp_iv_len_tests[i]);

	return failure;
}

struct do_all_arg {
	const char *previous;
	int failure;
};

static void
evp_do_all_cb_common(const char *descr, const void *ptr, const char *from,
    const char *to, struct do_all_arg *arg)
{
	const char *previous = arg->previous;

	assert(from != NULL);
	arg->previous = from;

	if (ptr == NULL && to == NULL) {
		arg->failure |= 1;
		fprintf(stderr, "FAIL: %s %s: method and alias both NULL\n",
		    descr, from);
	}
	if (ptr != NULL && to != NULL) {
		arg->failure |= 1;
		fprintf(stderr, "FAIL: %s %s has method and alias \"%s\"\n",
		    descr, from, to);
	}

	if (previous == NULL)
		return;

	if (strcmp(previous, from) >= 0) {
		arg->failure |= 1;
		fprintf(stderr, "FAIL: %ss %s and %s out of order\n", descr,
		    previous, from);
	}
}

static void
evp_cipher_do_all_cb(const EVP_CIPHER *cipher, const char *from, const char *to,
    void *arg)
{
	evp_do_all_cb_common("cipher", cipher, from, to, arg);
}

static void
evp_md_do_all_cb(const EVP_MD *md, const char *from, const char *to, void *arg)
{
	evp_do_all_cb_common("digest", md, from, to, arg);
}

static int
evp_do_all_test(void)
{
	struct do_all_arg arg;
	int failure = 0;

	memset(&arg, 0, sizeof(arg));
	/* XXX - replace with EVP_CIPHER_do_all() after next bump. */
	EVP_CIPHER_do_all_sorted(evp_cipher_do_all_cb, &arg);
	failure |= arg.failure;

	memset(&arg, 0, sizeof(arg));
	/* XXX - replace with EVP_MD_do_all() after next bump. */
	EVP_MD_do_all_sorted(evp_md_do_all_cb, &arg);
	failure |= arg.failure;

	return failure;
}

static void
evp_cipher_aliases_cb(const EVP_CIPHER *cipher, const char *from, const char *to,
    void *arg)
{
	struct do_all_arg *do_all = arg;
	const EVP_CIPHER *from_cipher, *to_cipher;

	if (to == NULL)
		return;

	from_cipher = EVP_get_cipherbyname(from);
	to_cipher = EVP_get_cipherbyname(to);

	if (from_cipher != NULL && from_cipher == to_cipher)
		return;

	fprintf(stderr, "FAIL: cipher mismatch from \"%s\" to \"%s\": "
	    "from: %p, to: %p\n", from, to, from_cipher, to_cipher);
	do_all->failure |= 1;
}

static void
evp_digest_aliases_cb(const EVP_MD *digest, const char *from, const char *to,
    void *arg)
{
	struct do_all_arg *do_all = arg;
	const EVP_MD *from_digest, *to_digest;

	if (to == NULL)
		return;

	from_digest = EVP_get_digestbyname(from);
	to_digest = EVP_get_digestbyname(to);

	if (from_digest != NULL && from_digest == to_digest)
		return;

	fprintf(stderr, "FAIL: digest mismatch from \"%s\" to \"%s\": "
	    "from: %p, to: %p\n", from, to, from_digest, to_digest);
	do_all->failure |= 1;
}

static int
evp_aliases_test(void)
{
	struct do_all_arg arg;
	int failure = 0;

	memset(&arg, 0, sizeof(arg));
	EVP_CIPHER_do_all(evp_cipher_aliases_cb, &arg);
	failure |= arg.failure;

	memset(&arg, 0, sizeof(arg));
	EVP_MD_do_all(evp_digest_aliases_cb, &arg);
	failure |= arg.failure;

	return failure;
}

static void
obj_name_cb(const OBJ_NAME *obj_name, void *do_all_arg)
{
	struct do_all_arg *arg = do_all_arg;
	struct do_all_arg arg_copy = *arg;
	const char *previous = arg->previous;
	const char *descr = "OBJ_NAME unknown";

	assert(obj_name->name != NULL);
	arg->previous = obj_name->name;

	if (obj_name->type == OBJ_NAME_TYPE_CIPHER_METH) {
		descr = "OBJ_NAME cipher";

		if (obj_name->alias == 0) {
			const EVP_CIPHER *cipher;

			if ((cipher = EVP_get_cipherbyname(obj_name->name)) !=
			    (const EVP_CIPHER *)obj_name->data) {
				arg->failure |= 1;
				fprintf(stderr, "FAIL: %s by name %p != %p\n",
				    descr, cipher, obj_name->data);
			}

			evp_do_all_cb_common(descr, obj_name->data,
			    obj_name->name, NULL, &arg_copy);
		} else if (obj_name->alias == OBJ_NAME_ALIAS) {
			evp_cipher_aliases_cb(NULL, obj_name->name,
			    obj_name->data, &arg_copy);
		} else {
			fprintf(stderr, "FAIL %s %s: unexpected alias value %d\n",
			    descr, obj_name->name, obj_name->alias);
			arg->failure |= 1;
		}
	} else if (obj_name->type == OBJ_NAME_TYPE_MD_METH) {
		descr = "OBJ_NAME digest";

		if (obj_name->alias == 0) {
			const EVP_MD *evp_md;

			if ((evp_md = EVP_get_digestbyname(obj_name->name)) !=
			    (const EVP_MD *)obj_name->data) {
				arg->failure |= 1;
				fprintf(stderr, "FAIL: %s by name %p != %p\n",
				    descr, evp_md, obj_name->data);
			}

			evp_do_all_cb_common(descr, obj_name->data,
			    obj_name->name, NULL, &arg_copy);
		} else if (obj_name->alias == OBJ_NAME_ALIAS) {
			evp_digest_aliases_cb(NULL, obj_name->name,
			    obj_name->data, &arg_copy);
		} else {
			fprintf(stderr, "FAIL: %s %s: unexpected alias value %d\n",
			    descr, obj_name->name, obj_name->alias);
			arg->failure |= 1;
		}
	} else {
		fprintf(stderr, "FAIL: unexpected OBJ_NAME type %d\n",
		    obj_name->type);
		arg->failure |= 1;
	}

	if (previous != NULL && strcmp(previous, obj_name->name) >= 0) {
		arg->failure |= 1;
		fprintf(stderr, "FAIL: %ss %s and %s out of order\n", descr,
		    previous, obj_name->name);
	}

	arg->failure |= arg_copy.failure;
}

static int
obj_name_do_all_test(void)
{
	struct do_all_arg arg;
	int failure = 0;

	memset(&arg, 0, sizeof(arg));
	/* XXX - replace with OBJ_NAME_do_all() after next bump. */
	OBJ_NAME_do_all_sorted(OBJ_NAME_TYPE_CIPHER_METH, obj_name_cb, &arg);
	failure |= arg.failure;

	memset(&arg, 0, sizeof(arg));
	/* XXX - replace with OBJ_NAME_do_all() after next bump. */
	OBJ_NAME_do_all_sorted(OBJ_NAME_TYPE_MD_METH, obj_name_cb, &arg);
	failure |= arg.failure;

	memset(&arg, 0, sizeof(arg));
	/* XXX - replace with OBJ_NAME_do_all() after next bump. */
	OBJ_NAME_do_all_sorted(OBJ_NAME_TYPE_PKEY_METH, obj_name_cb, &arg);
	failure |= arg.failure;

	return failure;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= evp_asn1_method_test();
	failed |= evp_pkey_method_test();
	failed |= evp_pkey_iv_len_test();
	failed |= evp_do_all_test();
	failed |= evp_aliases_test();
	failed |= obj_name_do_all_test();

	OPENSSL_cleanup();

	return failed;
}
