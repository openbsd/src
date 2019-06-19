/*	$OpenBSD: exptest.c,v 1.7 2018/11/08 22:20:25 jsing Exp $	*/
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>

int BN_mod_exp_ct(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx);
int BN_mod_exp_nonct(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx);
int BN_mod_exp_mont_ct(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
int BN_mod_exp_mont_nonct(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);

#define NUM_BITS	(BN_BITS*2)

/*
 * Test that r == 0 in test_exp_mod_zero(). Returns one on success,
 * returns zero and prints debug output otherwise.
 */
static int a_is_zero_mod_one(const char *method, const BIGNUM *r,
							 const BIGNUM *a) {
	if (!BN_is_zero(r)) {
		fprintf(stderr, "%s failed:\n", method);
		fprintf(stderr, "a ** 0 mod 1 = r (should be 0)\n");
		fprintf(stderr, "a = ");
		BN_print_fp(stderr, a);
		fprintf(stderr, "\nr = ");
		BN_print_fp(stderr, r);
		fprintf(stderr, "\n");
		return 0;
	}
	return 1;
}

/*
 * test_exp_mod_zero tests that x**0 mod 1 == 0. It returns zero on success.
 */
static int test_exp_mod_zero(void)
{
	BIGNUM a, p, m;
	BIGNUM r;
	BN_ULONG one_word = 1;
	BN_CTX *ctx = BN_CTX_new();
	int ret = 1, failed = 0;

	BN_init(&m);
	BN_one(&m);

	BN_init(&a);
	BN_one(&a);

	BN_init(&p);
	BN_zero(&p);

	BN_init(&r);

	if (!BN_rand(&a, 1024, 0, 0))
		goto err;

	if (!BN_mod_exp(&r, &a, &p, &m, ctx))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp", &r, &a))
		failed = 1;

	if (!BN_mod_exp_ct(&r, &a, &p, &m, ctx))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp_ct", &r, &a))
		failed = 1;

	if (!BN_mod_exp_nonct(&r, &a, &p, &m, ctx))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp_nonct", &r, &a))
		failed = 1;

	if (!BN_mod_exp_recp(&r, &a, &p, &m, ctx))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp_recp", &r, &a))
		failed = 1;

	if (!BN_mod_exp_simple(&r, &a, &p, &m, ctx))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp_simple", &r, &a))
		failed = 1;

	if (!BN_mod_exp_mont(&r, &a, &p, &m, ctx, NULL))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp_mont", &r, &a))
		failed = 1;

	if (!BN_mod_exp_mont_ct(&r, &a, &p, &m, ctx, NULL))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp_mont_ct", &r, &a))
		failed = 1;

	if (!BN_mod_exp_mont_nonct(&r, &a, &p, &m, ctx, NULL))
		goto err;

	if (!a_is_zero_mod_one("BN_mod_exp_mont_nonct", &r, &a))
		failed = 1;

	if (!BN_mod_exp_mont_consttime(&r, &a, &p, &m, ctx, NULL)) {
		goto err;
	}

	if (!a_is_zero_mod_one("BN_mod_exp_mont_consttime", &r, &a))
		failed = 1;

	/*
	 * A different codepath exists for single word multiplication
	 * in non-constant-time only.
	 */
	if (!BN_mod_exp_mont_word(&r, one_word, &p, &m, ctx, NULL))
		goto err;

	if (!BN_is_zero(&r)) {
		fprintf(stderr, "BN_mod_exp_mont_word failed:\n");
		fprintf(stderr, "1 ** 0 mod 1 = r (should be 0)\n");
		fprintf(stderr, "r = ");
		BN_print_fp(stderr, &r);
		fprintf(stderr, "\n");
		return 0;
	}

	ret = failed;

 err:
	BN_free(&r);
	BN_free(&a);
	BN_free(&p);
	BN_free(&m);
	BN_CTX_free(ctx);

	return ret;
}

int main(int argc, char *argv[])
{
	BIGNUM *r_mont, *r_mont_const, *r_recp, *r_simple;
	BIGNUM *r_mont_ct, *r_mont_nonct, *a, *b, *m;
	BN_CTX *ctx;
	BIO *out = NULL;
	unsigned char c;
	int i, ret;

	ERR_load_BN_strings();

	if ((ctx = BN_CTX_new()) == NULL)
		goto err;
	if ((r_mont = BN_new()) == NULL)
		goto err;
	if ((r_mont_const = BN_new()) == NULL)
		goto err;
	if ((r_mont_ct = BN_new()) == NULL)
		goto err;
	if ((r_mont_nonct = BN_new()) == NULL)
		goto err;
	if ((r_recp = BN_new()) == NULL)
		goto err;
	if ((r_simple = BN_new()) == NULL)
		goto err;
	if ((a = BN_new()) == NULL)
		goto err;
	if ((b = BN_new()) == NULL)
		goto err;
	if ((m = BN_new()) == NULL)
		goto err;

	if ((out = BIO_new(BIO_s_file())) == NULL)
		exit(1);
	BIO_set_fp(out, stdout, BIO_NOCLOSE);

	for (i = 0; i < 200; i++) {
		arc4random_buf(&c, 1);
		c = (c % BN_BITS) - BN_BITS2;
		BN_rand(a, NUM_BITS + c, 0, 0);

		arc4random_buf(&c, 1);
		c = (c % BN_BITS) - BN_BITS2;
		BN_rand(b, NUM_BITS + c, 0, 0);

		arc4random_buf(&c, 1);
		c = (c % BN_BITS) - BN_BITS2;
		BN_rand(m, NUM_BITS + c, 0, 1);

		BN_mod(a, a, m, ctx);
		BN_mod(b, b, m, ctx);

		ret = BN_mod_exp_mont(r_mont, a, b, m, ctx, NULL);
		if (ret <= 0) {
			printf("BN_mod_exp_mont() problems\n");
			goto err;
		}

		ret = BN_mod_exp_mont_ct(r_mont_ct, a, b, m, ctx, NULL);
		if (ret <= 0) {
			printf("BN_mod_exp_mont_ct() problems\n");
			goto err;
		}

		ret = BN_mod_exp_mont_nonct(r_mont_nonct, a, b, m, ctx, NULL);
		if (ret <= 0) {
			printf("BN_mod_exp_mont_nonct() problems\n");
			goto err;
		}

		ret = BN_mod_exp_recp(r_recp, a, b, m, ctx);
		if (ret <= 0) {
			printf("BN_mod_exp_recp() problems\n");
			goto err;
		}

		ret = BN_mod_exp_simple(r_simple, a, b, m, ctx);
		if (ret <= 0) {
			printf("BN_mod_exp_simple() problems\n");
			goto err;
		}

		ret = BN_mod_exp_mont_consttime(r_mont_const, a, b, m, ctx, NULL);
		if (ret <= 0) {
			printf("BN_mod_exp_mont_consttime() problems\n");
			goto err;
		}

		if (BN_cmp(r_simple, r_mont) == 0 &&
		    BN_cmp(r_simple, r_recp) == 0 &&
		    BN_cmp(r_simple, r_mont_const) == 0) {
			printf(".");
			fflush(stdout);
		} else {
			if (BN_cmp(r_simple, r_mont) != 0)
				printf("\nsimple and mont results differ\n");
			if (BN_cmp(r_simple, r_mont_const) != 0)
				printf("\nsimple and mont const time results differ\n");
			if (BN_cmp(r_simple, r_recp) != 0)
				printf("\nsimple and recp results differ\n");
			if (BN_cmp(r_mont, r_mont_ct) != 0)
				printf("\nmont_ct and mont results differ\n");
			if (BN_cmp(r_mont_ct, r_mont_nonct) != 0)
				printf("\nmont_ct and mont_nonct results differ\n");

			printf("a (%3d) = ", BN_num_bits(a));
			BN_print(out, a);
			printf("\nb (%3d) = ", BN_num_bits(b));
			BN_print(out, b);
			printf("\nm (%3d) = ", BN_num_bits(m));
			BN_print(out, m);
			printf("\nsimple   =");
			BN_print(out, r_simple);
			printf("\nrecp	 =");
			BN_print(out, r_recp);
			printf("\nmont	 =");
			BN_print(out, r_mont);
			printf("\nmont_ct  =");
			BN_print(out, r_mont_const);
			printf("\n");
			exit(1);
		}
	}
	BN_free(r_mont);
	BN_free(r_mont_const);
	BN_free(r_mont_ct);
	BN_free(r_mont_nonct);
	BN_free(r_recp);
	BN_free(r_simple);
	BN_free(a);
	BN_free(b);
	BN_free(m);
	BN_CTX_free(ctx);
	ERR_remove_thread_state(NULL);
	CRYPTO_mem_leaks(out);
	BIO_free(out);
	printf("\n");

	if (test_exp_mod_zero() != 0)
		goto err;

	printf("done\n");

	return (0);

 err:
	ERR_load_crypto_strings();
	ERR_print_errors(out);
	return (1);
}
