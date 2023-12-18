/* $OpenBSD: evp_pbe.c,v 1.34 2023/12/18 13:12:43 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2006 The OpenSSL Project.  All rights reserved.
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

#include <stdio.h>
#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

#include "evp_local.h"

/* Password based encryption (PBE) functions */

struct pbe_config {
	int pbe_nid;
	int cipher_nid;
	int md_nid;
	EVP_PBE_KEYGEN *keygen;
};

static const struct pbe_config pbe_outer[] = {
	{
		.pbe_nid = NID_pbeWithMD2AndDES_CBC,
		.cipher_nid = NID_des_cbc,
		.md_nid = NID_md2,
		.keygen = PKCS5_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbeWithMD5AndDES_CBC,
		.cipher_nid = NID_des_cbc,
		.md_nid = NID_md5,
		.keygen = PKCS5_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbeWithSHA1AndRC2_CBC,
		.cipher_nid = NID_rc2_64_cbc,
		.md_nid = NID_sha1,
		.keygen = PKCS5_PBE_keyivgen,
	},
#ifndef OPENSSL_NO_HMAC
	{
		.pbe_nid = NID_id_pbkdf2,
		.cipher_nid = -1,
		.md_nid = -1,
		.keygen = PKCS5_v2_PBKDF2_keyivgen,
	},
#endif
	{
		.pbe_nid = NID_pbe_WithSHA1And128BitRC4,
		.cipher_nid = NID_rc4,
		.md_nid = NID_sha1,
		.keygen = PKCS12_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbe_WithSHA1And40BitRC4,
		.cipher_nid = NID_rc4_40,
		.md_nid = NID_sha1,
		.keygen = PKCS12_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
		.cipher_nid = NID_des_ede3_cbc,
		.md_nid = NID_sha1,
		.keygen = PKCS12_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbe_WithSHA1And2_Key_TripleDES_CBC,
		.cipher_nid = NID_des_ede_cbc,
		.md_nid = NID_sha1,
		.keygen = PKCS12_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbe_WithSHA1And128BitRC2_CBC,
		.cipher_nid = NID_rc2_cbc,
		.md_nid = NID_sha1,
		.keygen = PKCS12_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbe_WithSHA1And40BitRC2_CBC,
		.cipher_nid = NID_rc2_40_cbc,
		.md_nid = NID_sha1,
		.keygen = PKCS12_PBE_keyivgen,
	},
#ifndef OPENSSL_NO_HMAC
	{
		.pbe_nid = NID_pbes2,
		.cipher_nid = -1,
		.md_nid = -1,
		.keygen = PKCS5_v2_PBE_keyivgen,
	},
#endif
	{
		.pbe_nid = NID_pbeWithMD2AndRC2_CBC,
		.cipher_nid = NID_rc2_64_cbc,
		.md_nid = NID_md2,
		.keygen = PKCS5_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbeWithMD5AndRC2_CBC,
		.cipher_nid = NID_rc2_64_cbc,
		.md_nid = NID_md5,
		.keygen = PKCS5_PBE_keyivgen,
	},
	{
		.pbe_nid = NID_pbeWithSHA1AndDES_CBC,
		.cipher_nid = NID_des_cbc,
		.md_nid = NID_sha1,
		.keygen = PKCS5_PBE_keyivgen,
	},
};

#define N_PBE_OUTER (sizeof(pbe_outer) / sizeof(pbe_outer[0]))

static const struct pbe_config pbe_prf[] = {
	{
		.pbe_nid = NID_hmacWithSHA1,
		.cipher_nid = -1,
		.md_nid = NID_sha1,
	},
	{
		.pbe_nid = NID_hmacWithMD5,
		.cipher_nid = -1,
		.md_nid = NID_md5,
	},
	{
		.pbe_nid = NID_hmacWithSHA224,
		.cipher_nid = -1,
		.md_nid = NID_sha224,
	},
	{
		.pbe_nid = NID_hmacWithSHA256,
		.cipher_nid = -1,
		.md_nid = NID_sha256,
	},
	{
		.pbe_nid = NID_hmacWithSHA384,
		.cipher_nid = -1,
		.md_nid = NID_sha384,
	},
	{
		.pbe_nid = NID_hmacWithSHA512,
		.cipher_nid = -1,
		.md_nid = NID_sha512,
	},
	{
		.pbe_nid = NID_id_HMACGostR3411_94,
		.cipher_nid = -1,
		.md_nid = NID_id_GostR3411_94,
	},
	{
		.pbe_nid = NID_id_tc26_hmac_gost_3411_12_256,
		.cipher_nid = -1,
		.md_nid = NID_id_tc26_gost3411_2012_256,
	},
	{
		.pbe_nid = NID_id_tc26_hmac_gost_3411_12_512,
		.cipher_nid = -1,
		.md_nid = NID_id_tc26_gost3411_2012_512,
	},
};

#define N_PBE_PRF (sizeof(pbe_prf) / sizeof(pbe_prf[0]))

int
EVP_PBE_find(int type, int pbe_nid, int *out_cipher_nid, int *out_md_nid,
    EVP_PBE_KEYGEN **out_keygen)
{
	const struct pbe_config *pbe = NULL;
	size_t i;

	if (out_cipher_nid != NULL)
		*out_cipher_nid = NID_undef;
	if (out_md_nid != NULL)
		*out_md_nid = NID_undef;
	if (out_keygen != NULL)
		*out_keygen = NULL;

	if (pbe_nid == NID_undef)
		return 0;

	if (type == EVP_PBE_TYPE_OUTER) {
		for (i = 0; i < N_PBE_OUTER; i++) {
			if (pbe_nid == pbe_outer[i].pbe_nid) {
				pbe = &pbe_outer[i];
				break;
			}
		}
	} else if (type == EVP_PBE_TYPE_PRF) {
		for (i = 0; i < N_PBE_PRF; i++) {
			if (pbe_nid == pbe_prf[i].pbe_nid) {
				pbe = &pbe_prf[i];
				break;
			}
		}
	}
	if (pbe == NULL)
		return 0;

	if (out_cipher_nid != NULL)
		*out_cipher_nid = pbe->cipher_nid;
	if (out_md_nid != NULL)
		*out_md_nid = pbe->md_nid;
	if (out_keygen != NULL)
		*out_keygen = pbe->keygen;

	return 1;
}

int
EVP_PBE_CipherInit(ASN1_OBJECT *pbe_obj, const char *pass, int passlen,
    ASN1_TYPE *param, EVP_CIPHER_CTX *ctx, int en_de)
{
	const EVP_CIPHER *cipher = NULL;
	const EVP_MD *md = NULL;
	int pbe_nid, cipher_nid, md_nid;
	EVP_PBE_KEYGEN *keygen;

	if ((pbe_nid = OBJ_obj2nid(pbe_obj)) == NID_undef) {
		EVPerror(EVP_R_UNKNOWN_PBE_ALGORITHM);
		return 0;
	}
	if (!EVP_PBE_find(EVP_PBE_TYPE_OUTER, pbe_nid, &cipher_nid, &md_nid,
	    &keygen)) {
		EVPerror(EVP_R_UNKNOWN_PBE_ALGORITHM);
		ERR_asprintf_error_data("NID=%d", pbe_nid);
		return 0;
	}

	if (pass == NULL)
		passlen = 0;
	if (passlen == -1)
		passlen = strlen(pass);

	if (cipher_nid != -1) {
		if ((cipher = EVP_get_cipherbynid(cipher_nid)) == NULL) {
			EVPerror(EVP_R_UNKNOWN_CIPHER);
			return 0;
		}
	}
	if (md_nid != -1) {
		if ((md = EVP_get_digestbynid(md_nid)) == NULL) {
			EVPerror(EVP_R_UNKNOWN_DIGEST);
			return 0;
		}
	}

	if (!keygen(ctx, pass, passlen, param, cipher, md, en_de)) {
		EVPerror(EVP_R_KEYGEN_FAILURE);
		return 0;
	}

	return 1;
}

int
EVP_PBE_alg_add_type(int pbe_type, int pbe_nid, int cipher_nid, int md_nid,
    EVP_PBE_KEYGEN *keygen)
{
	EVPerror(ERR_R_DISABLED);
	return 0;
}

int
EVP_PBE_alg_add(int nid, const EVP_CIPHER *cipher, const EVP_MD *md,
    EVP_PBE_KEYGEN *keygen)
{
	EVPerror(ERR_R_DISABLED);
	return 0;
}

void
EVP_PBE_cleanup(void)
{
}
