
/* This file has been explicitly broken by ryker for OpenBSD, July
 * 1, 1998. In spite of the title, there is no implementation of the
 * RSA algorithm left in this file. All these routines will return an
 * error and fail when called. They exist as stubs and can be
 * ressurected from the bit bucket by someone in the free world once
 * the RSA algorithm is no longer subject to patent problems. Eric
 * Young's original copyright is below. 
 */

/* crypto/rsa/rsa_eay.c */
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
#include "cryptlib.h"
#include "bn.h"
#include "rsa.h"
#include "rand.h"

#ifndef NOPROTO
static int RSA_eay_public_encrypt(int flen, unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
static int RSA_eay_private_encrypt(int flen, unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
static int RSA_eay_public_decrypt(int flen, unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
static int RSA_eay_private_decrypt(int flen, unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
static int RSA_eay_mod_exp(BIGNUM *r0, BIGNUM *i, RSA *rsa);
static int RSA_eay_init(RSA *rsa);
static int RSA_eay_finish(RSA *rsa);
#else
static int RSA_eay_public_encrypt();
static int RSA_eay_private_encrypt();
static int RSA_eay_public_decrypt();
static int RSA_eay_private_decrypt();
static int RSA_eay_mod_exp();
static int RSA_eay_init();
static int RSA_eay_finish();
#endif

static RSA_METHOD rsa_pkcs1_eay_meth={
	"Eric Young's PKCS#1 RSA",
	RSA_eay_public_encrypt,
	RSA_eay_public_decrypt,
	RSA_eay_private_encrypt,
	RSA_eay_private_decrypt,
	RSA_eay_mod_exp,
	BN_mod_exp_mont,
	RSA_eay_init,
	RSA_eay_finish,
	0,
	NULL,
	};

RSA_METHOD *RSA_PKCS1_SSLeay()
	{
	return(&rsa_pkcs1_eay_meth);
	}

static int RSA_eay_public_encrypt(flen, from, to, rsa, padding)
int flen;
unsigned char *from;
unsigned char *to;
RSA *rsa;
int padding;
	{
	BIGNUM *f=NULL,*ret=NULL;
	int i,j,k,num=0,r= -1;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;

	/* Body of this routine removed for OpenBSD - will return
	 * when the RSA patent expires
	 */

err:
	if (ctx != NULL) BN_CTX_free(ctx);
	if (f != NULL) BN_free(f);
	if (ret != NULL) BN_free(ret);
	if (buf != NULL) 
		{
		memset(buf,0,num);
		Free(buf);
		}
	return(r);
	}

static int RSA_eay_private_encrypt(flen, from, to, rsa, padding)
int flen;
unsigned char *from;
unsigned char *to;
RSA *rsa;
int padding;
	{
	BIGNUM *f=NULL,*ret=NULL;
	int i,j,k,num=0,r= -1;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;

	/* Body of this routine removed for OpenBSD - will return
	 * when the RSA patent expires
	 */

err:
	if (ctx != NULL) BN_CTX_free(ctx);
	if (ret != NULL) BN_free(ret);
	if (f != NULL) BN_free(f);
	if (buf != NULL)
		{
		memset(buf,0,num);
		Free(buf);
		}
	return(r);
	}

static int RSA_eay_private_decrypt(flen, from, to, rsa,padding)
int flen;
unsigned char *from;
unsigned char *to;
RSA *rsa;
int padding;
	{
	BIGNUM *f=NULL,*ret=NULL;
	int j,num=0,r= -1;
	unsigned char *p;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;

	/* Body of this routine removed for OpenBSD - will return
	 * when the RSA patent expires
	 */

err:
	if (ctx != NULL) BN_CTX_free(ctx);
	if (f != NULL) BN_free(f);
	if (ret != NULL) BN_free(ret);
	if (buf != NULL)
		{
		memset(buf,0,num);
		Free(buf);
		}
	return(r);
	}

static int RSA_eay_public_decrypt(flen, from, to, rsa, padding)
int flen;
unsigned char *from;
unsigned char *to;
RSA *rsa;
int padding;
	{
	BIGNUM *f=NULL,*ret=NULL;
	int i,num=0,r= -1;
	unsigned char *p;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;


	/* Body of this routine removed for OpenBSD - will return
	 * when the RSA patent expires
	 */

err:
	if (ctx != NULL) BN_CTX_free(ctx);
	if (f != NULL) BN_free(f);
	if (ret != NULL) BN_free(ret);
	if (buf != NULL)
		{
		memset(buf,0,num);
		Free(buf);
		}
	return(r);
	}

static int RSA_eay_mod_exp(r0, I, rsa)
BIGNUM *r0;
BIGNUM *I;
RSA *rsa;
	{
	BIGNUM *r1=NULL,*m1=NULL;
	int ret=0;
	BN_CTX *ctx;

	if ((ctx=BN_CTX_new()) == NULL) goto err;
	m1=BN_new();
	r1=BN_new();
	if ((m1 == NULL) || (r1 == NULL)) goto err;

	/* Body of this routine removed for OpenBSD - will return
	 * when the RSA patent expires
	 */
err:
	if (m1 != NULL) BN_free(m1);
	if (r1 != NULL) BN_free(r1);
	BN_CTX_free(ctx);
	return(ret);
	}

static int RSA_eay_init(rsa)
RSA *rsa;
	{
	rsa->flags|=RSA_FLAG_CACHE_PUBLIC|RSA_FLAG_CACHE_PRIVATE;
	return(1);
	}

static int RSA_eay_finish(rsa)
RSA *rsa;
	{
	if (rsa->method_mod_n != NULL)
		BN_MONT_CTX_free((BN_MONT_CTX *)rsa->method_mod_n);
	if (rsa->method_mod_p != NULL)
		BN_MONT_CTX_free((BN_MONT_CTX *)rsa->method_mod_p);
	if (rsa->method_mod_q != NULL)
		BN_MONT_CTX_free((BN_MONT_CTX *)rsa->method_mod_q);
	return(1);
	}


