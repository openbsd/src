/* $OpenBSD: ecs_sign.c,v 1.7 2019/01/19 01:07:00 tb Exp $ */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

#include "ecs_locl.h"
#include "ec_lcl.h"

ECDSA_SIG *
ECDSA_do_sign(const unsigned char *dgst, int dlen, EC_KEY *eckey)
{
	return ECDSA_do_sign_ex(dgst, dlen, NULL, NULL, eckey);
}

ECDSA_SIG *
ECDSA_do_sign_ex(const unsigned char *dgst, int dlen, const BIGNUM *kinv,
    const BIGNUM *rp, EC_KEY *eckey)
{
	if (eckey->meth->sign_sig != NULL)
		return eckey->meth->sign_sig(dgst, dlen, kinv, rp, eckey);
	ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
	return 0;
}

int
ECDSA_sign(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
    unsigned int *siglen, EC_KEY *eckey)
{
	return ECDSA_sign_ex(type, dgst, dlen, sig, siglen, NULL, NULL, eckey);
}

int
ECDSA_sign_ex(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
    unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey)
{
	if (eckey->meth->sign != NULL)
		return eckey->meth->sign(type, dgst, dlen, sig, siglen, kinv, r, eckey);
	ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
	return 0;
}

int
ECDSA_sign_setup(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	if (eckey->meth->sign_setup != NULL)
		return eckey->meth->sign_setup(eckey, ctx_in, kinvp, rp);
	ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
	return 0;
}
