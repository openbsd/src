/*	$OpenBSD: evp_pkey_check.c,v 1.4 2023/03/02 20:18:40 tb Exp $ */
/*
 * Copyright (c) 2021-2022 Theo Buehler <tb@openbsd.org>
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

#include <stdio.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#define EVP_TEST_RSA_BITS	2048

static int
evp_pkey_check_rsa(void)
{
	EVP_PKEY_CTX *pkey_ctx = NULL;
	EVP_PKEY *pkey = NULL;
	RSA *rsa = NULL;
	BIGNUM *rsa_d;
	int ret;
	int fail_soft = 0;
	int failed = 1;

	/*
	 * Generate a run-off-the-mill RSA key.
	 */

	if ((pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL)) == NULL) {
		fprintf(stderr, "%s: EVP_PKEY_CTX_new_id()\n", __func__);
		goto err;
	}
	if (EVP_PKEY_keygen_init(pkey_ctx) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_keygen_init\n", __func__);
		goto err;
	}
	if (!EVP_PKEY_CTX_set_rsa_keygen_bits(pkey_ctx, EVP_TEST_RSA_BITS)) {
		fprintf(stderr, "%s: EVP_PKEY_CTX_set_rsa_keygen_bits\n",
		    __func__);
		goto err;
	}
	if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_keygen\n", __func__);
		goto err;
	}

	/* At this point, no pkey is set on pkey_ctx, we should fail with 0. */
	if (EVP_PKEY_check(pkey_ctx) != 0) {
		fprintf(stderr, "%s: EVP_PKEY_check() succeeded without pkey\n",
		    __func__);
		ERR_print_errors_fp(stderr);
		fail_soft = 1;
	}

	ERR_clear_error();

	/*
	 * Create a new EVP_PKEY_CTX with pkey set.
	 */

	EVP_PKEY_CTX_free(pkey_ctx);
	if ((pkey_ctx = EVP_PKEY_CTX_new(pkey, NULL)) == NULL) {
		fprintf(stderr, "%s: EVP_PKEY_CTX_new\n", __func__);
		goto err;
	}

	/* The freshly generated pkey is set on pkey_ctx. We should succeed. */
	if ((ret = EVP_PKEY_check(pkey_ctx)) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_check(), generated pkey: %d\n",
		    __func__, ret);
		ERR_print_errors_fp(stderr);
		ERR_clear_error();
		fail_soft = 1;
	}

	/* Public key checking for RSA is not supported. */
	if (EVP_PKEY_public_check(pkey_ctx) != -2) {
		fprintf(stderr,
		    "%s: EVP_PKEY_public_check() supported for RSA?\n",
		    __func__);
		goto err;
	}
	ERR_clear_error();

	/* Parameter checking for RSA is not supported. */
	if (EVP_PKEY_param_check(pkey_ctx) != -2) {
		fprintf(stderr,
		    "%s: EVP_PKEY_param_check() supported for RSA?\n",
		    __func__);
		goto err;
	}
	ERR_clear_error();

	/*
	 * Now modify the RSA key a bit. The check should then fail.
	 */

	if ((rsa = EVP_PKEY_get0_RSA(pkey)) == NULL) {
		fprintf(stderr, "%s: EVP_PKEY_get0_RSA\n", __func__);
		goto err;
	}
	/* We're lazy and modify rsa->d directly, hence the ugly cast. */
	if ((rsa_d = (BIGNUM *)RSA_get0_d(rsa)) == NULL) {
		fprintf(stderr, "%s: RSA_get0_d()\n", __func__);
		goto err;
	}
	if (!BN_add_word(rsa_d, 2)) {
		fprintf(stderr, "%s: BN_add_word\n", __func__);
		goto err;
	}

	/* Since (d+2) * e != 1 mod (p-1)*(q-1), we should fail */
	if (EVP_PKEY_check(pkey_ctx) == 1) {
		fprintf(stderr, "%s: EVP_PKEY_check success with modified d\n",
		    __func__);
		fail_soft = 1;
	}

	if (ERR_peek_error() == 0) {
		fprintf(stderr, "%s: expected some RSA errors\n", __func__);
		fail_soft = 1;
	}
	ERR_clear_error();

	failed = 0;

 err:
	EVP_PKEY_CTX_free(pkey_ctx);
	EVP_PKEY_free(pkey);

	return failed | fail_soft;
}

static int
evp_pkey_check_ec(void)
{
	EVP_PKEY_CTX *pkey_ctx = NULL;
	EVP_PKEY *pkey = NULL;
	EC_KEY *eckey = NULL;
	BIGNUM *private_key = NULL;
	EC_GROUP *group;
	const EC_POINT *generator;
	BIGNUM *cofactor = NULL, *order = NULL;
	int ret;
	int fail_soft = 0;
	int failed = 1;

	/*
	 * Generate an elliptic curve key on secp384r1
	 */

	if ((pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL)) == NULL) {
		fprintf(stderr, "%s: EVP_PKEY_CTX_new_id\n", __func__);
		goto err;
	}
	if (EVP_PKEY_keygen_init(pkey_ctx) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_keygen_init\n", __func__);
		goto err;
	}
	if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pkey_ctx,
	    NID_secp384r1) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_CTX_set_ec_paramgen_curve_nid\n",
		    __func__);
		goto err;
	}
	if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_keygen\n", __func__);
		goto err;
	}

	/* At this point, no pkey is set on pkey_ctx, we should fail with 0. */
	if (EVP_PKEY_check(pkey_ctx) != 0) {
		fprintf(stderr, "%s: EVP_PKEY_check() succeeded without pkey\n",
		    __func__);
		ERR_print_errors_fp(stderr);
		fail_soft = 1;
	}

	ERR_clear_error();

	/*
	 * Create a new EVP_PKEY_CTX with pkey set.
	 */

	EVP_PKEY_CTX_free(pkey_ctx);
	if ((pkey_ctx = EVP_PKEY_CTX_new(pkey, NULL)) == NULL) {
		fprintf(stderr, "%s: EVP_PKEY_CTX_new\n", __func__);
		goto err;
	}

	/* The freshly generated pkey is set on pkey_ctx. We should succeed. */
	if ((ret = EVP_PKEY_check(pkey_ctx)) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_check(), generated pkey: %d\n",
		    __func__, ret);
		ERR_print_errors_fp(stderr);
		ERR_clear_error();
		fail_soft = 1;
	}

	/* We should also succeed the public check. */
	if ((ret = EVP_PKEY_public_check(pkey_ctx)) <= 0) {
		fprintf(stderr,
		    "%s: EVP_PKEY_public_check(), generated pkey: %d\n",
		    __func__, ret);
		ERR_print_errors_fp(stderr);
		ERR_clear_error();
		fail_soft = 1;
	}

	/* We should also succeed the parameter check. */
	if ((ret = EVP_PKEY_param_check(pkey_ctx)) <= 0) {
		fprintf(stderr,
		    "%s: EVP_PKEY_param_check(), generated pkey: %d\n",
		    __func__, ret);
		ERR_print_errors_fp(stderr);
		ERR_clear_error();
		fail_soft = 1;
	}

	/*
	 * Modify the private key slightly.
	 */

	if ((eckey = EVP_PKEY_get0_EC_KEY(pkey)) == NULL) {
		fprintf(stderr, "%s: EVP_PKEY_get0_EC_KEY\n", __func__);
		goto err;
	}

	/* We're lazy and modify the private key directly. */
	if ((private_key = (BIGNUM *)EC_KEY_get0_private_key(eckey)) == NULL) {
		fprintf(stderr, "%s: EC_KEY_get0_private_key\n", __func__);
		goto err;
	}

	/*
	 * The private key is a random number in [1, order). Preserve this
	 * property by adding 1 if it is equal to 1 and subtracting 1 otherwise.
	 */
	if (BN_cmp(private_key, BN_value_one()) == 0) {
		if (!BN_add_word(private_key, 1)) {
			fprintf(stderr, "%s: BN_add_word\n", __func__);
			goto err;
		}
	} else {
		if (!BN_sub_word(private_key, 1)) {
			fprintf(stderr, "%s: BN_sub_word\n", __func__);
			goto err;
		}
	}

	/* Generator times private key will no longer be equal to public key. */
	if (EVP_PKEY_check(pkey_ctx) == 1) {
		fprintf(stderr, "%s: EVP_PKEY_check succeeded unexpectedly\n",
		    __func__);
		fail_soft = 1;
	}

	if (ERR_peek_error() == 0) {
		fprintf(stderr, "%s: expected a private key error\n", __func__);
		fail_soft = 1;
	}
	ERR_clear_error();

	/* EVP_PKEY_public_check checks the private key (sigh), so we fail. */
	if (EVP_PKEY_public_check(pkey_ctx) == 1) {
		fprintf(stderr,
		    "%s: EVP_PKEY_public_check succeeded unexpectedly\n",
		    __func__);
		fail_soft = 1;
	}

	/* We should still succeed the parameter check. */
	if ((ret = EVP_PKEY_param_check(pkey_ctx)) <= 0) {
		fprintf(stderr,
		    "%s: EVP_PKEY_param_check(), modified privkey pkey: %d\n",
		    __func__, ret);
		ERR_print_errors_fp(stderr);
		ERR_clear_error();
		fail_soft = 1;
	}

	/* Now set the private key to NULL. The API will think malloc failed. */
	if (EC_KEY_set_private_key(eckey, NULL) != 0) {
		fprintf(stderr, "%s: EC_KEY_set_private_key succeeded?!",
		    __func__);
		goto err;
	}

	/*
	 * EVP_PKEY_public_check now only checks that the public key is on the
	 * curve. We should succeed again.
	 */

	if ((ret = EVP_PKEY_public_check(pkey_ctx)) <= 0) {
		fprintf(stderr, "%s: EVP_PKEY_check(), generated pkey: %d\n",
		    __func__, ret);
		fail_soft = 1;
	}

	ERR_clear_error();

	/*
	 * Now let's modify the group to trip the parameter check.
	 */

	if ((group = (EC_GROUP *)EC_KEY_get0_group(eckey)) == NULL) {
		fprintf(stderr, "%s: EC_KEY_get0_group() failed\n", __func__);
		goto err;
	}

	if ((generator = EC_GROUP_get0_generator(group)) == NULL) {
		fprintf(stderr, "%s: EC_GROUP_get0_generator() failed\n",
		    __func__);
		goto err;
	}

	if ((order = BN_new()) == NULL) {
		fprintf(stderr, "%s: order = BN_new() failed\n", __func__);
		goto err;
	}
	if ((cofactor = BN_new()) == NULL) {
		fprintf(stderr, "%s: cofactor = BN_new() failed\n", __func__);
		goto err;
	}

	if (!EC_GROUP_get_order(group, order, NULL)) {
		fprintf(stderr, "%s: EC_GROUP_get_order() failed\n", __func__);
		goto err;
	}
	if (!EC_GROUP_get_cofactor(group, cofactor, NULL)) {
		fprintf(stderr, "%s: EC_GROUP_get_cofactor() failed\n",
		    __func__);
		goto err;
	}

	/* Decrement order so order * generator != (point at infinity). */
	if (!BN_sub_word(order, 1)) {
		fprintf(stderr, "%s: BN_sub_word() failed\n", __func__);
		goto err;
	}

	/* Now set this nonsense on the group. */
	if (!EC_GROUP_set_generator(group, generator, order, cofactor)) {
		fprintf(stderr, "%s: EC_GROUP_set_generator() failed\n",
		    __func__);
		goto err;
	}

	/* We should now fail the parameter check. */
	if (EVP_PKEY_param_check(pkey_ctx) == 1) {
		fprintf(stderr,
		    "%s: EVP_PKEY_param_check(), succeeded unexpectedly\n",
		    __func__);
		fail_soft = 1;
	}

	if (ERR_peek_error() == 0) {
		fprintf(stderr, "%s: expected a group order error\n", __func__);
		fail_soft = 1;
	}
	ERR_clear_error();

	failed = 0;

 err:
	EVP_PKEY_CTX_free(pkey_ctx);
	EVP_PKEY_free(pkey);
	BN_free(order);
	BN_free(cofactor);

	return failed | fail_soft;
}

int
main(void)
{
	int failed = 0;

	failed |= evp_pkey_check_rsa();
	failed |= evp_pkey_check_ec();

	return failed;
}
