/*	$OpenBSD: crypto.c,v 1.5 2010/12/21 13:24:11 mikeb Exp $	*/
/*	$vantronix: crypto.c,v 1.18 2010/05/28 15:34:35 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <event.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/x509.h>

#include "iked.h"
#include "ikev2.h"

struct iked_hash *
hash_new(u_int8_t type, u_int16_t id)
{
	struct iked_hash	*hash;
	const EVP_MD		*md = NULL;
	HMAC_CTX		*ctx = NULL;
	int			 length = 0, fixedkey = 0, trunc = 0;

	switch (type) {
	case IKEV2_XFORMTYPE_PRF:
		switch (id) {
		case IKEV2_XFORMPRF_HMAC_MD5:
			md = EVP_md5();
			length = MD5_DIGEST_LENGTH;
			break;
		case IKEV2_XFORMPRF_HMAC_SHA1:
			md = EVP_sha1();
			length = SHA_DIGEST_LENGTH;
			break;
		case IKEV2_XFORMPRF_HMAC_SHA2_256:
			md = EVP_sha256();
			length = SHA256_DIGEST_LENGTH;
			break;
		case IKEV2_XFORMPRF_HMAC_SHA2_384:
			md = EVP_sha384();
			length = SHA384_DIGEST_LENGTH;
			break;
		case IKEV2_XFORMPRF_HMAC_SHA2_512:
			md = EVP_sha512();
			length = SHA512_DIGEST_LENGTH;
			break;
		case IKEV2_XFORMPRF_AES128_XCBC:
			fixedkey = 128 / 8;
			length = fixedkey;
			/* FALLTHROUGH */
		case IKEV2_XFORMPRF_HMAC_TIGER:
		case IKEV2_XFORMPRF_AES128_CMAC:
		default:
			log_debug("%s: prf %s not supported", __func__,
			    print_map(id, ikev2_xformprf_map));
			break;
		}
		break;
	case IKEV2_XFORMTYPE_INTEGR:
		switch (id) {
		case IKEV2_XFORMAUTH_HMAC_MD5_96:
			md = EVP_md5();
			length = MD5_DIGEST_LENGTH;
			trunc = 12;
			break;
		case IKEV2_XFORMAUTH_HMAC_SHA1_96:
			md = EVP_sha1();
			length = SHA_DIGEST_LENGTH;
			trunc = 12;
			break;
		case IKEV2_XFORMAUTH_HMAC_SHA2_256_128:
			md = EVP_sha256();
			length = SHA256_DIGEST_LENGTH;
			trunc = 16;
			break;
		case IKEV2_XFORMAUTH_HMAC_SHA2_384_192:
			md = EVP_sha384();
			length = SHA384_DIGEST_LENGTH;
			trunc = 24;
			break;
		case IKEV2_XFORMAUTH_HMAC_SHA2_512_256:
			md = EVP_sha512();
			length = SHA512_DIGEST_LENGTH;
			trunc = 32;
			break;
		case IKEV2_XFORMAUTH_NONE:
		case IKEV2_XFORMAUTH_DES_MAC:
		case IKEV2_XFORMAUTH_KPDK_MD5:
		case IKEV2_XFORMAUTH_AES_XCBC_96:
		case IKEV2_XFORMAUTH_HMAC_MD5_128:
		case IKEV2_XFORMAUTH_HMAC_SHA1_160:
		case IKEV2_XFORMAUTH_AES_CMAC_96:
		case IKEV2_XFORMAUTH_AES_128_GMAC:
		case IKEV2_XFORMAUTH_AES_192_GMAC:
		case IKEV2_XFORMAUTH_AES_256_GMAC:
		default:
			log_debug("%s: auth %s not supported", __func__,
			    print_map(id, ikev2_xformauth_map));
			break;
		}
		break;
	default:
		log_debug("%s: hash type %s not supported", __func__,
		    print_map(id, ikev2_xformtype_map));
		break;
	}
	if (md == NULL)
		return (NULL);

	if ((hash = calloc(1, sizeof(*hash))) == NULL) {
		log_debug("%s: alloc hash", __func__);
		return (NULL);
	}

	hash->hash_type = type;
	hash->hash_id = id;
	hash->hash_priv = md;
	hash->hash_ctx = NULL;
	hash->hash_trunc = trunc;
	hash->hash_length = length;
	hash->hash_fixedkey = fixedkey;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		log_debug("%s: alloc hash ctx", __func__);
		hash_free(hash);
		return (NULL);
	}

	HMAC_CTX_init(ctx);
	hash->hash_ctx = ctx;

	return (hash);
}

struct ibuf *
hash_setkey(struct iked_hash *hash, void *key, size_t keylen)
{
	ibuf_release(hash->hash_key);
	if ((hash->hash_key = ibuf_new(key, keylen)) == NULL) {
		log_debug("%s: alloc hash key", __func__);
		return (NULL);
	}
	return (hash->hash_key);
}

void
hash_free(struct iked_hash *hash)
{
	if (hash == NULL)
		return;
	if (hash->hash_ctx != NULL) {
		HMAC_CTX_cleanup(hash->hash_ctx);
		free(hash->hash_ctx);
	}
	ibuf_release(hash->hash_key);
	free(hash);
}

void
hash_init(struct iked_hash *hash)
{
	HMAC_Init_ex(hash->hash_ctx, hash->hash_key->buf,
	    ibuf_length(hash->hash_key), hash->hash_priv, NULL);
}

void
hash_update(struct iked_hash *hash, void *buf, size_t len)
{
	HMAC_Update(hash->hash_ctx, buf, len);
}

void
hash_final(struct iked_hash *hash, void *buf, size_t *len)
{
	u_int length = 0;

	HMAC_Final(hash->hash_ctx, buf, &length);
	*len = (size_t)length;

	/* Truncate the result if required by the alg */
	if (hash->hash_trunc && *len > hash->hash_trunc)
		*len = hash->hash_trunc;
}

size_t
hash_length(struct iked_hash *hash)
{
	if (hash->hash_trunc)
		return (hash->hash_trunc);
	return (hash->hash_length);
}

size_t
hash_keylength(struct iked_hash *hash)
{
	return (hash->hash_length);
}

struct iked_cipher *
cipher_new(u_int8_t type, u_int16_t id, u_int16_t id_length)
{
	struct iked_cipher	*encr;
	const EVP_CIPHER	*cipher = NULL;
	EVP_CIPHER_CTX		*ctx = NULL;
	int			 length = 0, fixedkey = 0, ivlength = 0;

	switch (type) {
	case IKEV2_XFORMTYPE_ENCR:
		switch (id) {
		case IKEV2_XFORMENCR_3DES:
			cipher = EVP_des_ede3_cbc();
			length = EVP_CIPHER_block_size(cipher);
			fixedkey = EVP_CIPHER_key_length(cipher);
			ivlength = EVP_CIPHER_iv_length(cipher);
			break;
		case IKEV2_XFORMENCR_AES_CBC:
			switch (id_length) {
			case 128:
				cipher = EVP_aes_128_cbc();
				break;
			case 192:
				cipher = EVP_aes_192_cbc();
				break;
			case 256:
				cipher = EVP_aes_256_cbc();
				break;
			default:
				log_debug("%s: invalid key length %d"
				    " for cipher %s", __func__, id_length,
				    print_map(id, ikev2_xformencr_map));
				break;
			}
			if (cipher == NULL)
				break;
			length = EVP_CIPHER_block_size(cipher);
			ivlength = EVP_CIPHER_iv_length(cipher);
			fixedkey = EVP_CIPHER_key_length(cipher);
			break;
		case IKEV2_XFORMENCR_DES_IV64:
		case IKEV2_XFORMENCR_DES:
		case IKEV2_XFORMENCR_RC5:
		case IKEV2_XFORMENCR_IDEA:
		case IKEV2_XFORMENCR_CAST:
		case IKEV2_XFORMENCR_BLOWFISH:
		case IKEV2_XFORMENCR_3IDEA:
		case IKEV2_XFORMENCR_DES_IV32:
		case IKEV2_XFORMENCR_NULL:
		case IKEV2_XFORMENCR_AES_CTR:
			/* FALLTHROUGH */
		default:
			log_debug("%s: cipher %s not supported", __func__,
			    print_map(id, ikev2_xformencr_map));
			cipher = NULL;
			break;
		}
		break;
	default:
		log_debug("%s: cipher type %s not supported", __func__,
		    print_map(id, ikev2_xformtype_map));
		break;
	}
	if (cipher == NULL)
		return (NULL);

	if ((encr = calloc(1, sizeof(*encr))) == NULL) {
		log_debug("%s: alloc cipher", __func__);
		return (NULL);
	}

	encr->encr_id = id;
	encr->encr_priv = cipher;
	encr->encr_ctx = NULL;
	encr->encr_length = length;
	encr->encr_fixedkey = fixedkey;
	encr->encr_ivlength = ivlength ? ivlength : length;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		log_debug("%s: alloc cipher ctx", __func__);
		cipher_free(encr);
		return (NULL);
	}

	EVP_CIPHER_CTX_init(ctx);
	encr->encr_ctx = ctx;

	return (encr);
}

struct ibuf *
cipher_setkey(struct iked_cipher *encr, void *key, size_t keylen)
{
	ibuf_release(encr->encr_key);
	if ((encr->encr_key = ibuf_new(key, keylen)) == NULL) {
		log_debug("%s: alloc cipher key", __func__);
		return (NULL);
	}
	return (encr->encr_key);
}

struct ibuf *
cipher_setiv(struct iked_cipher *encr, void *iv, size_t len)
{
	ibuf_release(encr->encr_iv);
	if (iv != NULL) {
		if (len < encr->encr_ivlength) {
			log_debug("%s: invalid IV length %d", __func__, len);
			return (NULL);
		}
		encr->encr_iv = ibuf_new(iv, encr->encr_ivlength);
	} else {
		/* Get new random IV */
		encr->encr_iv = ibuf_random(encr->encr_ivlength);
	}
	if (encr->encr_iv == NULL) {
		log_debug("%s: failed to set IV", __func__);
		return (NULL);
	}
	return (encr->encr_iv);
}

void
cipher_free(struct iked_cipher *encr)
{
	if (encr == NULL)
		return;
	if (encr->encr_ctx != NULL) {
		EVP_CIPHER_CTX_cleanup(encr->encr_ctx);
		free(encr->encr_ctx);
	}
	ibuf_release(encr->encr_key);
	free(encr);
}

void
cipher_init(struct iked_cipher *encr, int enc)
{
	EVP_CipherInit_ex(encr->encr_ctx, encr->encr_priv, NULL,
	    ibuf_data(encr->encr_key), ibuf_data(encr->encr_iv), enc);
	EVP_CIPHER_CTX_set_padding(encr->encr_ctx, 0);
}

void
cipher_init_encrypt(struct iked_cipher *encr)
{
	cipher_init(encr, 1);
}

void
cipher_init_decrypt(struct iked_cipher *encr)
{
	cipher_init(encr, 0);
}

void
cipher_update(struct iked_cipher *encr, void *in, size_t inlen,
    void *out, size_t *outlen)
{
	int	 olen;

	olen = 0;
	if (!EVP_CipherUpdate(encr->encr_ctx, out, &olen, in, inlen)) {
		ca_sslerror();
		*outlen = 0;
		return;
	}
	*outlen = (size_t)olen;
}

void
cipher_final(struct iked_cipher *encr, void *out, size_t *outlen)
{
	int	 olen;

	olen = 0;
	if (!EVP_CipherFinal_ex(encr->encr_ctx, out, &olen)) {
		ca_sslerror();
		*outlen = 0;
		return;
	}
	*outlen = (size_t)olen;
}

size_t
cipher_length(struct iked_cipher *encr)
{
	return (encr->encr_length);
}

size_t
cipher_keylength(struct iked_cipher *encr)
{
	if (encr->encr_fixedkey)
		return (encr->encr_fixedkey);

	/* Might return zero */
	return (ibuf_length(encr->encr_key));
}

size_t
cipher_ivlength(struct iked_cipher *encr)
{
	return (encr->encr_ivlength);
}

size_t
cipher_outlength(struct iked_cipher *encr, size_t inlen)
{
	return (roundup(inlen, encr->encr_length));
}

struct iked_dsa *
dsa_new(u_int16_t id, struct iked_hash *prf, int sign)
{
	struct iked_dsa		*dsap = NULL, dsa;

	bzero(&dsa, sizeof(dsa));

	switch (id) {
	case IKEV2_AUTH_RSA_SIG:
		/*
		 * XXX RFC4306 is not very clear about this and the
		 * XXX informational RFC4718 says that we should use
		 * XXX SHA1 here, but shouldn't we use the negotiated PRF
		 * XXX alg instead?
		 */
		if ((dsa.dsa_priv =
		    EVP_get_digestbyname("sha1WithRSAEncryption")) == NULL)
			fatalx("dsa_new: cipher not available");
		break;
	case IKEV2_AUTH_SHARED_KEY_MIC:
		if (prf == NULL || prf->hash_priv == NULL)
			fatalx("dsa_new: invalid PRF");
		dsa.dsa_priv = prf->hash_priv;
		dsa.dsa_hmac = 1;
		break;
	case IKEV2_AUTH_DSS_SIG:
		dsa.dsa_priv = EVP_dss1();
		break;
	case IKEV2_AUTH_ECDSA_256:
		dsa.dsa_priv = EVP_sha256();
		break;
	case IKEV2_AUTH_ECDSA_384:
		dsa.dsa_priv = EVP_sha384();
		break;
	case IKEV2_AUTH_ECDSA_512:
		dsa.dsa_priv = EVP_sha512();
		break;
	default:
		log_debug("%s: auth method %s not supported", __func__,
		    print_map(id, ikev2_auth_map));
		break;
	}

	if ((dsap = calloc(1, sizeof(*dsap))) == NULL) {
		log_debug("%s: alloc dsa ctx", __func__);

		return (NULL);
	}
	memcpy(dsap, &dsa, sizeof(*dsap));

	dsap->dsa_method = id;
	dsap->dsa_sign = sign;

	if (dsap->dsa_hmac) {
		if ((dsap->dsa_ctx = calloc(1, sizeof(HMAC_CTX))) == NULL) {
			log_debug("%s: alloc hash ctx", __func__);
			dsa_free(dsap);
			return (NULL);
		}
		HMAC_CTX_init((HMAC_CTX *)dsap->dsa_ctx);
	} else {
		if ((dsap->dsa_ctx = EVP_MD_CTX_create()) == NULL) {
			log_debug("%s: alloc digest ctx", __func__);
			dsa_free(dsap);
			return (NULL);
		}
	}

	return (dsap);
}

struct iked_dsa *
dsa_sign_new(u_int16_t id, struct iked_hash *prf)
{
	return (dsa_new(id, prf, 1));
}

struct iked_dsa *
dsa_verify_new(u_int16_t id, struct iked_hash *prf)
{
	return (dsa_new(id, prf, 0));
}

void
dsa_free(struct iked_dsa *dsa)
{
	if (dsa == NULL)
		return;
	if (dsa->dsa_hmac) {
		HMAC_CTX_cleanup((HMAC_CTX *)dsa->dsa_ctx);
		free(dsa->dsa_ctx);
	} else {
		EVP_MD_CTX_destroy((EVP_MD_CTX *)dsa->dsa_ctx);
		if (dsa->dsa_key)
			EVP_PKEY_free(dsa->dsa_key);
		if (dsa->dsa_cert)
			X509_free(dsa->dsa_cert);
	}

	ibuf_release(dsa->dsa_keydata);
}

struct ibuf *
dsa_setkey(struct iked_dsa *dsa, void *key, size_t keylen, u_int8_t type)
{
	BIO		*rawcert = NULL;
	X509		*cert = NULL;
	RSA		*rsa = NULL;
	EVP_PKEY	*pkey = NULL;

	ibuf_release(dsa->dsa_keydata);
	if ((dsa->dsa_keydata = ibuf_new(key, keylen)) == NULL) {
		log_debug("%s: alloc signature key", __func__);
		return (NULL);
	}

	if ((rawcert = BIO_new_mem_buf(key, keylen)) == NULL)
		goto err;

	switch (type) {
	case IKEV2_CERT_X509_CERT:
		if ((cert = d2i_X509_bio(rawcert, NULL)) == NULL)
			goto sslerr;
		if ((pkey = X509_get_pubkey(cert)) == NULL)
			goto sslerr;
		dsa->dsa_cert = cert;
		dsa->dsa_key = pkey;
		break;
	case IKEV2_CERT_RSA_KEY:
		if (dsa->dsa_sign) {
			if ((rsa = d2i_RSAPrivateKey_bio(rawcert,
			    NULL)) == NULL)
				goto sslerr;
		} else {
			if ((rsa = d2i_RSAPublicKey_bio(rawcert,
			    NULL)) == NULL)
				goto sslerr;
		}

		if ((pkey = EVP_PKEY_new()) == NULL)
			goto sslerr;
		if (!EVP_PKEY_set1_RSA(pkey, rsa))
			goto sslerr;

		dsa->dsa_cert = NULL;
		dsa->dsa_key = pkey;
		break;
	default:
		if (dsa->dsa_hmac)
			break;
		log_debug("%s: unsupported key type", __func__);
		goto err;
	}

	return (dsa->dsa_keydata);

 sslerr:
	ca_sslerror();
 err:
	log_debug("%s: error", __func__);

	if (rsa != NULL)
		RSA_free(rsa);
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
	if (cert != NULL)
		X509_free(cert);
	if (rawcert != NULL)
		BIO_free(rawcert);
	ibuf_release(dsa->dsa_keydata);
	return (NULL);
}

int
dsa_init(struct iked_dsa *dsa)
{
	int	 ret;

	if (dsa->dsa_hmac) {
		HMAC_Init_ex(dsa->dsa_ctx, ibuf_data(dsa->dsa_keydata),
		    ibuf_length(dsa->dsa_keydata), dsa->dsa_priv, NULL);
		return (0);
	}

	if (dsa->dsa_sign)
		ret = EVP_SignInit_ex(dsa->dsa_ctx, dsa->dsa_priv, NULL);
	else
		ret = EVP_VerifyInit_ex(dsa->dsa_ctx, dsa->dsa_priv, NULL);

	return (ret ? 0 : -1);
}

int
dsa_update(struct iked_dsa *dsa, const void *buf, size_t len)
{
	int	ret = 1;

	if (dsa->dsa_hmac)
		HMAC_Update(dsa->dsa_ctx, buf, len);
	else if (dsa->dsa_sign)
		ret = EVP_SignUpdate(dsa->dsa_ctx, buf, len);
	else
		ret = EVP_VerifyUpdate(dsa->dsa_ctx, buf, len);

	return (ret ? 0 : -1);
}

size_t
dsa_length(struct iked_dsa *dsa)
{
	if (dsa->dsa_hmac)
		return (EVP_MD_size(dsa->dsa_priv));
	return (EVP_PKEY_size(dsa->dsa_key));
}

ssize_t
dsa_sign_final(struct iked_dsa *dsa, void *buf, size_t len)
{
	u_int		siglen;

	if (len < dsa_length(dsa))
		return (-1);

	if (dsa->dsa_hmac)
		HMAC_Final(dsa->dsa_ctx, buf, &siglen);
	else {
		if (!EVP_SignFinal(dsa->dsa_ctx, buf, &siglen,
		    dsa->dsa_key))
			return (-1);
	}

	return (siglen);
}

ssize_t
dsa_verify_final(struct iked_dsa *dsa, void *buf, size_t len)
{
	u_int8_t	 sig[EVP_MAX_MD_SIZE];
	u_int		 siglen = sizeof(sig);

	if (dsa->dsa_hmac) {
		HMAC_Final(dsa->dsa_ctx, sig, &siglen);
		if (siglen != len || memcmp(buf, sig, siglen) != 0)
			return (-1);
	} else {
		if (!EVP_VerifyFinal(dsa->dsa_ctx, buf, len,
		    dsa->dsa_key)) {
			ca_sslerror();
			return (-1);
		}
	}

	return (0);
}
