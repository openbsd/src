/* $OpenBSD: ecdsa.h,v 1.16 2023/06/19 09:12:41 tb Exp $ */
/*
 * Written by Nils Larsch for the OpenSSL project
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
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
 *    licensing@OpenSSL.org.
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
#ifndef HEADER_ECDSA_H
#define HEADER_ECDSA_H

#include <openssl/opensslconf.h>

#ifdef OPENSSL_NO_ECDSA
#error ECDSA is disabled.
#endif

#include <openssl/bn.h>
#include <openssl/ec.h>

#include <openssl/ossl_typ.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ECDSA_SIG_st ECDSA_SIG;

struct ecdsa_method {
	const char *name;
	ECDSA_SIG *(*ecdsa_do_sign)(const unsigned char *dgst, int dgst_len,
	    const BIGNUM *inv, const BIGNUM *rp, EC_KEY *eckey);
	int (*ecdsa_sign_setup)(EC_KEY *eckey, BN_CTX *ctx, BIGNUM **kinv,
	    BIGNUM **r);
	int (*ecdsa_do_verify)(const unsigned char *dgst, int dgst_len,
	    const ECDSA_SIG *sig, EC_KEY *eckey);
	int flags;
	char *app_data;
};

/*
 * If this flag is set, the ECDSA method is FIPS compliant and can be used
 * in FIPS mode. This is set in the validated module method. If an
 * application sets this flag in its own methods it is its responsibility
 * to ensure the result is compliant.
 */

#define ECDSA_FLAG_FIPS_METHOD  0x1

ECDSA_SIG *ECDSA_SIG_new(void);
void ECDSA_SIG_free(ECDSA_SIG *sig);
int i2d_ECDSA_SIG(const ECDSA_SIG *sig, unsigned char **pp);
ECDSA_SIG *d2i_ECDSA_SIG(ECDSA_SIG **sig, const unsigned char **pp, long len);
void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps);

const BIGNUM *ECDSA_SIG_get0_r(const ECDSA_SIG *sig);
const BIGNUM *ECDSA_SIG_get0_s(const ECDSA_SIG *sig);
int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s);

ECDSA_SIG *ECDSA_do_sign(const unsigned char *dgst, int dgst_len,
    EC_KEY *eckey);
ECDSA_SIG *ECDSA_do_sign_ex(const unsigned char *dgst, int dgstlen,
    const BIGNUM *kinv, const BIGNUM *rp, EC_KEY *eckey);
int ECDSA_do_verify(const unsigned char *dgst, int dgst_len,
    const ECDSA_SIG *sig, EC_KEY* eckey);

const ECDSA_METHOD *ECDSA_OpenSSL(void);
void ECDSA_set_default_method(const ECDSA_METHOD *meth);
const ECDSA_METHOD *ECDSA_get_default_method(void);
int ECDSA_set_method(EC_KEY *eckey, const ECDSA_METHOD *meth);
int ECDSA_size(const EC_KEY *eckey);

int ECDSA_sign_setup(EC_KEY *eckey, BN_CTX *ctx, BIGNUM **kinv,
    BIGNUM **rp);
int ECDSA_sign(int type, const unsigned char *dgst, int dgstlen,
    unsigned char *sig, unsigned int *siglen, EC_KEY *eckey);
int ECDSA_sign_ex(int type, const unsigned char *dgst, int dgstlen,
    unsigned char *sig, unsigned int *siglen, const BIGNUM *kinv,
    const BIGNUM *rp, EC_KEY *eckey);
int ECDSA_verify(int type, const unsigned char *dgst, int dgstlen,
    const unsigned char *sig, int siglen, EC_KEY *eckey);

int ECDSA_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
    CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func);
int ECDSA_set_ex_data(EC_KEY *d, int idx, void *arg);
void *ECDSA_get_ex_data(EC_KEY *d, int idx);

/* XXX should be in ec.h, but needs ECDSA_SIG */
void EC_KEY_METHOD_set_sign(EC_KEY_METHOD *meth,
    int (*sign)(int type, const unsigned char *dgst,
	int dlen, unsigned char *sig, unsigned int *siglen,
	const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey),
    int (*sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in,
	BIGNUM **kinvp, BIGNUM **rp),
    ECDSA_SIG *(*sign_sig)(const unsigned char *dgst,
	int dgst_len, const BIGNUM *in_kinv, const BIGNUM *in_r,
	EC_KEY *eckey));
void EC_KEY_METHOD_set_verify(EC_KEY_METHOD *meth,
    int (*verify)(int type, const unsigned char *dgst, int dgst_len,
	const unsigned char *sigbuf, int sig_len, EC_KEY *eckey),
    int (*verify_sig)(const unsigned char *dgst, int dgst_len,
	const ECDSA_SIG *sig, EC_KEY *eckey));
void EC_KEY_METHOD_get_sign(const EC_KEY_METHOD *meth,
    int (**psign)(int type, const unsigned char *dgst,
	int dlen, unsigned char *sig, unsigned int *siglen,
	const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey),
    int (**psign_setup)(EC_KEY *eckey, BN_CTX *ctx_in,
	BIGNUM **kinvp, BIGNUM **rp),
    ECDSA_SIG *(**psign_sig)(const unsigned char *dgst,
	int dgst_len, const BIGNUM *in_kinv, const BIGNUM *in_r,
	EC_KEY *eckey));
void EC_KEY_METHOD_get_verify(const EC_KEY_METHOD *meth,
    int (**pverify)(int type, const unsigned char *dgst, int dgst_len,
	const unsigned char *sigbuf, int sig_len, EC_KEY *eckey),
    int (**pverify_sig)(const unsigned char *dgst, int dgst_len,
	const ECDSA_SIG *sig, EC_KEY *eckey));

void ERR_load_ECDSA_strings(void);

/* Error codes for the ECDSA functions. */

/* Function codes. */
#define ECDSA_F_ECDSA_CHECK				 104
#define ECDSA_F_ECDSA_DATA_NEW_METHOD			 100
#define ECDSA_F_ECDSA_DO_SIGN				 101
#define ECDSA_F_ECDSA_DO_VERIFY				 102
#define ECDSA_F_ECDSA_SIGN_SETUP			 103

/* Reason codes. */
#define ECDSA_R_BAD_SIGNATURE				 100
#define ECDSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE		 101
#define ECDSA_R_ERR_EC_LIB				 102
#define ECDSA_R_MISSING_PARAMETERS			 103
#define ECDSA_R_NEED_NEW_SETUP_VALUES			 106
#define ECDSA_R_NON_FIPS_METHOD				 107
#define ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED		 104
#define ECDSA_R_SIGNATURE_MALLOC_FAILED			 105

#ifdef  __cplusplus
}
#endif
#endif
