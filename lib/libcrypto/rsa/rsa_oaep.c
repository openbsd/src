/* $OpenBSD: rsa_oaep.c,v 1.32 2019/10/09 16:17:59 jsing Exp $ */
/*
 * Copyright 1999-2018 The OpenSSL Project Authors. All Rights Reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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

/* EME-OAEP as defined in RFC 2437 (PKCS #1 v2.0) */

/* See Victor Shoup, "OAEP reconsidered," Nov. 2000,
 * <URL: http://www.shoup.net/papers/oaep.ps.Z>
 * for problems with the security proof for the
 * original OAEP scheme, which EME-OAEP is based on.
 *
 * A new proof can be found in E. Fujisaki, T. Okamoto,
 * D. Pointcheval, J. Stern, "RSA-OEAP is Still Alive!",
 * Dec. 2000, <URL: http://eprint.iacr.org/2000/061/>.
 * The new proof has stronger requirements for the
 * underlying permutation: "partial-one-wayness" instead
 * of one-wayness.  For the RSA function, this is
 * an equivalent notion.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include "rsa_locl.h"

int
RSA_padding_add_PKCS1_OAEP(unsigned char *to, int tlen,
    const unsigned char *from, int flen, const unsigned char *param, int plen)
{
	return RSA_padding_add_PKCS1_OAEP_mgf1(to, tlen, from, flen, param,
	    plen, NULL, NULL);
}

int
RSA_padding_add_PKCS1_OAEP_mgf1(unsigned char *to, int tlen,
    const unsigned char *from, int flen, const unsigned char *param, int plen,
    const EVP_MD *md, const EVP_MD *mgf1md)
{
	int i, emlen = tlen - 1;
	unsigned char *db, *seed;
	unsigned char *dbmask = NULL;
	unsigned char seedmask[EVP_MAX_MD_SIZE];
	int mdlen, dbmask_len = 0;
	int rv = 0;

	if (md == NULL)
		md = EVP_sha1();
	if (mgf1md == NULL)
		mgf1md = md;

	if ((mdlen = EVP_MD_size(md)) <= 0)
		goto err;

	if (flen > emlen - 2 * mdlen - 1) {
		RSAerror(RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
		goto err;
	}

	if (emlen < 2 * mdlen + 1) {
		RSAerror(RSA_R_KEY_SIZE_TOO_SMALL);
		goto err;
	}

	to[0] = 0;
	seed = to + 1;
	db = to + mdlen + 1;

	if (!EVP_Digest((void *)param, plen, db, NULL, md, NULL))
		goto err;

	memset(db + mdlen, 0, emlen - flen - 2 * mdlen - 1);
	db[emlen - flen - mdlen - 1] = 0x01;
	memcpy(db + emlen - flen - mdlen, from, flen);
	arc4random_buf(seed, mdlen);

	dbmask_len = emlen - mdlen;
	if ((dbmask = malloc(dbmask_len)) == NULL) {
		RSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if (PKCS1_MGF1(dbmask, dbmask_len, seed, mdlen, mgf1md) < 0)
		goto err;
	for (i = 0; i < dbmask_len; i++)
		db[i] ^= dbmask[i];
	if (PKCS1_MGF1(seedmask, mdlen, db, dbmask_len, mgf1md) < 0)
		goto err;
	for (i = 0; i < mdlen; i++)
		seed[i] ^= seedmask[i];

	rv = 1;

 err:
	explicit_bzero(seedmask, sizeof(seedmask));
	freezero(dbmask, dbmask_len);

	return rv;
}

int
RSA_padding_check_PKCS1_OAEP(unsigned char *to, int tlen,
    const unsigned char *from, int flen, int num, const unsigned char *param,
    int plen)
{
	return RSA_padding_check_PKCS1_OAEP_mgf1(to, tlen, from, flen, num,
	    param, plen, NULL, NULL);
}

int
RSA_padding_check_PKCS1_OAEP_mgf1(unsigned char *to, int tlen,
    const unsigned char *from, int flen, int num, const unsigned char *param,
    int plen, const EVP_MD *md, const EVP_MD *mgf1md)
{
	int i, dblen, mlen = -1;
	const unsigned char *maskeddb;
	int lzero;
	unsigned char *db = NULL;
	unsigned char seed[EVP_MAX_MD_SIZE], phash[EVP_MAX_MD_SIZE];
	unsigned char *padded_from;
	int bad = 0;
	int mdlen;

	if (md == NULL)
		md = EVP_sha1();
	if (mgf1md == NULL)
		mgf1md = md;

	if ((mdlen = EVP_MD_size(md)) <= 0)
		goto err;

	if (--num < 2 * mdlen + 1)
		/*
		 * 'num' is the length of the modulus, i.e. does not depend
		 * on the particular ciphertext.
		 */
		goto decoding_err;

	lzero = num - flen;
	if (lzero < 0) {
		/*
		 * signalling this error immediately after detection might allow
		 * for side-channel attacks (e.g. timing if 'plen' is huge
		 * -- cf. James H. Manger, "A Chosen Ciphertext Attack on RSA
		 * Optimal Asymmetric Encryption Padding (OAEP) [...]",
		 * CRYPTO 2001), so we use a 'bad' flag
		 */
		bad = 1;
		lzero = 0;
		flen = num; /* don't overflow the memcpy to padded_from */
	}

	dblen = num - mdlen;
	if ((db = malloc(dblen + num)) == NULL) {
		RSAerror(ERR_R_MALLOC_FAILURE);
		return -1;
	}

	/*
	 * Always do this zero-padding copy (even when lzero == 0)
	 * to avoid leaking timing info about the value of lzero.
	 */
	padded_from = db + dblen;
	memset(padded_from, 0, lzero);
	memcpy(padded_from + lzero, from, flen);

	maskeddb = padded_from + mdlen;

	if (PKCS1_MGF1(seed, mdlen, maskeddb, dblen, mgf1md))
		goto err;
	for (i = 0; i < mdlen; i++)
		seed[i] ^= padded_from[i];
	if (PKCS1_MGF1(db, dblen, seed, mdlen, mgf1md))
		goto err;
	for (i = 0; i < dblen; i++)
		db[i] ^= maskeddb[i];

	if (!EVP_Digest((void *)param, plen, phash, NULL, md, NULL))
		goto err;

	if (timingsafe_memcmp(db, phash, mdlen) != 0 || bad)
		goto decoding_err;
	else {
		for (i = mdlen; i < dblen; i++)
			if (db[i] != 0x00)
				break;
		if (i == dblen || db[i] != 0x01)
			goto decoding_err;
		else {
			/* everything looks OK */

			mlen = dblen - ++i;
			if (tlen < mlen) {
				RSAerror(RSA_R_DATA_TOO_LARGE);
				mlen = -1;
			} else
				memcpy(to, db + i, mlen);
		}
	}
	free(db);
	return mlen;

 decoding_err:
	/*
	 * To avoid chosen ciphertext attacks, the error message should not
	 * reveal which kind of decoding error happened
	 */
	RSAerror(RSA_R_OAEP_DECODING_ERROR);
 err:
	free(db);
	return -1;
}

int
PKCS1_MGF1(unsigned char *mask, long len, const unsigned char *seed,
    long seedlen, const EVP_MD *dgst)
{
	long i, outlen = 0;
	unsigned char cnt[4];
	EVP_MD_CTX c;
	unsigned char md[EVP_MAX_MD_SIZE];
	int mdlen;
	int rv = -1;

	EVP_MD_CTX_init(&c);
	mdlen = EVP_MD_size(dgst);
	if (mdlen < 0)
		goto err;
	for (i = 0; outlen < len; i++) {
		cnt[0] = (unsigned char)((i >> 24) & 255);
		cnt[1] = (unsigned char)((i >> 16) & 255);
		cnt[2] = (unsigned char)((i >> 8)) & 255;
		cnt[3] = (unsigned char)(i & 255);
		if (!EVP_DigestInit_ex(&c, dgst, NULL) ||
		    !EVP_DigestUpdate(&c, seed, seedlen) ||
		    !EVP_DigestUpdate(&c, cnt, 4))
			goto err;
		if (outlen + mdlen <= len) {
			if (!EVP_DigestFinal_ex(&c, mask + outlen, NULL))
				goto err;
			outlen += mdlen;
		} else {
			if (!EVP_DigestFinal_ex(&c, md, NULL))
				goto err;
			memcpy(mask + outlen, md, len - outlen);
			outlen = len;
		}
	}
	rv = 0;
 err:
	EVP_MD_CTX_cleanup(&c);
	return rv;
}
