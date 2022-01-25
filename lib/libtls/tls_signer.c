/* $OpenBSD: tls_signer.c,v 1.1 2022/01/25 21:51:24 eric Exp $ */
/*
 * Copyright (c) 2021 Eric Faurot <eric@openbsd.org>
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

#include <openssl/err.h>

#include "tls.h"
#include "tls_internal.h"

struct tls_signer_key {
	char *hash;
	RSA *rsa;
	EC_KEY *ecdsa;
	struct tls_signer_key *next;
};

struct tls_signer {
	struct tls_error error;
	struct tls_signer_key *keys;
};

static pthread_mutex_t signer_method_lock = PTHREAD_MUTEX_INITIALIZER;

struct tls_signer *
tls_signer_new(void)
{
	struct tls_signer *signer;

	if ((signer = calloc(1, sizeof(*signer))) == NULL)
		return (NULL);

	return (signer);
}

void
tls_signer_free(struct tls_signer *signer)
{
	struct tls_signer_key *skey;

	if (signer == NULL)
		return;

	tls_error_clear(&signer->error);

	while (signer->keys) {
		skey = signer->keys;
		signer->keys = skey->next;
		RSA_free(skey->rsa);
		EC_KEY_free(skey->ecdsa);
		free(skey->hash);
		free(skey);
	}

	free(signer);
}

const char *
tls_signer_error(struct tls_signer *signer)
{
	return (signer->error.msg);
}

int
tls_signer_add_keypair_mem(struct tls_signer *signer, const uint8_t *cert,
    size_t cert_len, const uint8_t *key, size_t key_len)
{
	struct tls_signer_key *skey = NULL;
	char *errstr = "unknown";
	int ssl_err;
	EVP_PKEY *pkey = NULL;
	X509 *x509 = NULL;
	BIO *bio = NULL;
	char *hash = NULL;

	/* Compute certificate hash */
	if ((bio = BIO_new_mem_buf(cert, cert_len)) == NULL) {
		tls_error_setx(&signer->error,
		    "failed to create certificate bio");
		goto err;
	}
	if ((x509 = PEM_read_bio_X509(bio, NULL, tls_password_cb,
	    NULL)) == NULL) {
		if ((ssl_err = ERR_peek_error()) != 0)
			errstr = ERR_error_string(ssl_err, NULL);
		tls_error_setx(&signer->error, "failed to load certificate: %s",
		    errstr);
		goto err;
	}
	if (tls_cert_pubkey_hash(x509, &hash) == -1) {
		tls_error_setx(&signer->error,
		    "failed to get certificate hash");
		goto err;
	}

	X509_free(x509);
	x509 = NULL;
	BIO_free(bio);
	bio = NULL;

	/* Read private key */
	if ((bio = BIO_new_mem_buf(key, key_len)) == NULL) {
		tls_error_setx(&signer->error, "failed to create key bio");
		goto err;
	}
	if ((pkey = PEM_read_bio_PrivateKey(bio, NULL, tls_password_cb,
	    NULL)) == NULL) {
		tls_error_setx(&signer->error, "failed to read private key");
		goto err;
	}

	if ((skey = calloc(1, sizeof(*skey))) == NULL) {
		tls_error_set(&signer->error, "failed to create key entry");
		goto err;
	}
	skey->hash = hash;
	if ((skey->rsa = EVP_PKEY_get1_RSA(pkey)) == NULL &&
	    (skey->ecdsa = EVP_PKEY_get1_EC_KEY(pkey)) == NULL) {
		tls_error_setx(&signer->error, "unknown key type");
		goto err;
	}

	skey->next = signer->keys;
	signer->keys = skey;
	EVP_PKEY_free(pkey);
	BIO_free(bio);

	return (0);

 err:
	EVP_PKEY_free(pkey);
	X509_free(x509);
	BIO_free(bio);
	free(hash);
	free(skey);

	return (-1);
}

int
tls_signer_add_keypair_file(struct tls_signer *signer, const char *cert_file,
    const char *key_file)
{
	char *cert = NULL, *key = NULL;
	size_t cert_len, key_len;
	int rv = -1;

	if (tls_config_load_file(&signer->error, "certificate", cert_file,
	    &cert, &cert_len) == -1)
		goto err;

	if (tls_config_load_file(&signer->error, "key", key_file, &key,
	    &key_len) == -1)
		goto err;

	rv = tls_signer_add_keypair_mem(signer, cert, cert_len, key, key_len);

 err:
	free(cert);
	free(key);

	return (rv);
}

static int
tls_sign_rsa(struct tls_signer *signer, struct tls_signer_key *skey,
    const uint8_t *dgst, size_t dgstlen, uint8_t **psig, size_t *psiglen,
    int padding)
{

	char *buf;
	int siglen, r;

	*psig = NULL;
	*psiglen = 0;

	siglen = RSA_size(skey->rsa);
	if (siglen <= 0) {
		tls_error_setx(&signer->error, "incorrect RSA_size: %d",
		    siglen);
		return (-1);
	}

	if ((buf = malloc(siglen)) == NULL) {
		tls_error_set(&signer->error, "RSA sign");
		return (-1);
	}

	r = RSA_private_encrypt((int)dgstlen, dgst, buf, skey->rsa, padding);
	if (r == -1) {
		tls_error_setx(&signer->error, "RSA_private_encrypt failed");
		free(buf);
		return (-1);
	}

	*psig = buf;
	*psiglen = (size_t)r;

	return (0);
}

static int
tls_sign_ecdsa(struct tls_signer *signer, struct tls_signer_key *skey,
    const uint8_t *dgst, size_t dgstlen, uint8_t **psig, size_t *psiglen)
{
	unsigned char *sig;
	unsigned int siglen;

	*psig = NULL;
	*psiglen = 0;

	siglen = ECDSA_size(skey->ecdsa);
	if (siglen == 0) {
		tls_error_setx(&signer->error, "incorrect ECDSA_size: %u",
		    siglen);
		return (-1);
	}
	if ((sig = malloc(siglen)) == NULL) {
		tls_error_set(&signer->error, "ECDSA sign");
		return (-1);
	}

	if (!ECDSA_sign(0, dgst, dgstlen, sig, &siglen, skey->ecdsa)) {
		tls_error_setx(&signer->error, "ECDSA_sign failed");
		free(sig);
		return (-1);
	}

	*psig = sig;
	*psiglen = siglen;

	return (0);
}

int
tls_signer_sign(struct tls_signer *signer, const char *hash,
    const uint8_t *dgst, size_t dgstlen, uint8_t **psig, size_t *psiglen,
    int padding)
{
	struct tls_signer_key *skey;

	for (skey = signer->keys; skey; skey = skey->next)
		if (!strcmp(hash, skey->hash))
			break;

	if (skey == NULL) {
		tls_error_setx(&signer->error, "key not found");
		return (-1);
	}

	if (skey->rsa != NULL)
		return tls_sign_rsa(signer, skey, dgst, dgstlen, psig, psiglen,
		    padding);

	if (skey->ecdsa != NULL)
		return tls_sign_ecdsa(signer, skey, dgst, dgstlen, psig, psiglen);

	tls_error_setx(&signer->error, "unknown key type");
	return (-1);
}

static int
tls_rsa_priv_enc(int srclen, const unsigned char *src, unsigned char *to,
    RSA *rsa, int padding)
{
	struct tls_config *config;
	const char *hash;
	size_t tolen;

	hash = RSA_get_ex_data(rsa, 0);
	config = RSA_get_ex_data(rsa, 1);

	if (hash == NULL || config == NULL)
		return (-1);

	if (config->sign_cb(config->sign_cb_arg, hash, (const uint8_t *)src,
	    srclen, (uint8_t *)to, &tolen, padding) == -1)
		return (-1);

	if (tolen > INT_MAX)
		return (-1);

	return ((int)tolen);
}

RSA_METHOD *
tls_signer_rsa_method(void)
{
	static RSA_METHOD *rsa_method = NULL;

	pthread_mutex_lock(&signer_method_lock);

	if (rsa_method != NULL)
		goto out;

	rsa_method = RSA_meth_new("libtls RSA method", 0);
	if (rsa_method == NULL)
		goto out;

	RSA_meth_set_priv_enc(rsa_method, tls_rsa_priv_enc);

 out:
	pthread_mutex_unlock(&signer_method_lock);

	return (rsa_method);
}

static ECDSA_SIG *
tls_ecdsa_do_sign(const unsigned char *dgst, int dgst_len, const BIGNUM *inv,
    const BIGNUM *rp, EC_KEY *eckey)
{
	struct tls_config *config;
	ECDSA_SIG *sig = NULL;
	const unsigned char *tsigbuf;
	const char *hash;
	char *sigbuf;
	size_t siglen;

	hash = ECDSA_get_ex_data(eckey, 0);
	config = ECDSA_get_ex_data(eckey, 1);

	if (hash == NULL || config == NULL)
		return (NULL);

	siglen = ECDSA_size(eckey);
	if ((sigbuf = malloc(siglen)) == NULL)
		return (NULL);

	if (config->sign_cb(config->sign_cb_arg, hash, dgst, dgst_len, sigbuf,
	    &siglen, 0) != -1) {
		tsigbuf = sigbuf;
		sig = d2i_ECDSA_SIG(NULL, &tsigbuf, siglen);
	}
	free(sigbuf);

	return (sig);
}

ECDSA_METHOD *
tls_signer_ecdsa_method(void)
{
	static ECDSA_METHOD *ecdsa_method = NULL;

	pthread_mutex_lock(&signer_method_lock);

	if (ecdsa_method != NULL)
		goto out;

	ecdsa_method = calloc(1, sizeof(*ecdsa_method));
	if (ecdsa_method == NULL)
		goto out;

	ecdsa_method->ecdsa_do_sign = tls_ecdsa_do_sign;
	ecdsa_method->name = strdup("libtls ECDSA method");
	if (ecdsa_method->name == NULL) {
		free(ecdsa_method);
		ecdsa_method = NULL;
	}

 out:
	pthread_mutex_unlock(&signer_method_lock);

	return (ecdsa_method);
}
