/*	$OpenBSD: bn_mod_exp.c,v 1.5 2022/12/02 18:31:40 tb Exp $	*/
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

#include <openssl/bn.h>
#include <openssl/err.h>

#include "bn_local.h"

#define NUM_BITS	(BN_BITS*2)

int
main(int argc, char *argv[])
{
	BIGNUM *r_mont, *r_mont_const, *r_recp, *r_simple;
	BIGNUM *r_mont_ct, *r_mont_nonct, *a, *b, *m;
	BN_CTX *ctx;
	unsigned char c;
	int i, ret;

	ERR_load_BN_strings();

	if ((ctx = BN_CTX_new()) == NULL)
		goto err;

	BN_CTX_start(ctx);

	if ((r_mont = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((r_mont_const = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((r_mont_ct = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((r_mont_nonct = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((r_recp = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((r_simple = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((a = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((b = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((m = BN_CTX_get(ctx)) == NULL)
		goto err;

	for (i = 0; i < 200; i++) {
		arc4random_buf(&c, 1);
		c = (c % BN_BITS) - BN_BITS2;
		if (!BN_rand(a, NUM_BITS + c, 0, 0))
			goto err;

		arc4random_buf(&c, 1);
		c = (c % BN_BITS) - BN_BITS2;
		if (!BN_rand(b, NUM_BITS + c, 0, 0))
			goto err;

		arc4random_buf(&c, 1);
		c = (c % BN_BITS) - BN_BITS2;
		if (!BN_rand(m, NUM_BITS + c, 0, 1))
			goto err;

		if (!BN_mod(a, a, m, ctx))
			goto err;
		if (!BN_mod(b, b, m, ctx))
			goto err;

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

		if (BN_cmp(r_simple, r_mont) != 0 ||
		    BN_cmp(r_simple, r_recp) != 0 ||
		    BN_cmp(r_simple, r_mont_const) != 0) {
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
			BN_print_fp(stdout, a);
			printf("\nb (%3d) = ", BN_num_bits(b));
			BN_print_fp(stdout, b);
			printf("\nm (%3d) = ", BN_num_bits(m));
			BN_print_fp(stdout, m);
			printf("\nsimple   =");
			BN_print_fp(stdout, r_simple);
			printf("\nrecp	 =");
			BN_print_fp(stdout, r_recp);
			printf("\nmont	 =");
			BN_print_fp(stdout, r_mont);
			printf("\nmont_ct  =");
			BN_print_fp(stdout, r_mont_const);
			printf("\n");
			exit(1);
		}
	}

	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	ERR_remove_thread_state(NULL);

	return (0);

 err:
	ERR_load_crypto_strings();
	ERR_print_errors_fp(stdout);
	return (1);
}
