/* $OpenBSD: tls_keypair.c,v 1.4 2018/02/08 10:19:31 jsing Exp $ */
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include <tls.h>

#include "tls_internal.h"

struct tls_keypair *
tls_keypair_new(void)
{
	return calloc(1, sizeof(struct tls_keypair));
}

void
tls_keypair_clear_key(struct tls_keypair *keypair)
{
	freezero(keypair->key_mem, keypair->key_len);
	keypair->key_mem = NULL;
	keypair->key_len = 0;
}

int
tls_keypair_set_cert_file(struct tls_keypair *keypair, struct tls_error *error,
    const char *cert_file)
{
	return tls_config_load_file(error, "certificate", cert_file,
	    &keypair->cert_mem, &keypair->cert_len);
}

int
tls_keypair_set_cert_mem(struct tls_keypair *keypair, const uint8_t *cert,
    size_t len)
{
	return tls_set_mem(&keypair->cert_mem, &keypair->cert_len, cert, len);
}

int
tls_keypair_set_key_file(struct tls_keypair *keypair, struct tls_error *error,
    const char *key_file)
{
	tls_keypair_clear_key(keypair);
	return tls_config_load_file(error, "key", key_file,
	    &keypair->key_mem, &keypair->key_len);
}

int
tls_keypair_set_key_mem(struct tls_keypair *keypair, const uint8_t *key,
    size_t len)
{
	tls_keypair_clear_key(keypair);
	return tls_set_mem(&keypair->key_mem, &keypair->key_len, key, len);
}

int
tls_keypair_set_ocsp_staple_file(struct tls_keypair *keypair,
    struct tls_error *error, const char *ocsp_file)
{
	return tls_config_load_file(error, "ocsp", ocsp_file,
	    &keypair->ocsp_staple, &keypair->ocsp_staple_len);
}

int
tls_keypair_set_ocsp_staple_mem(struct tls_keypair *keypair,
    const uint8_t *staple, size_t len)
{
	return tls_set_mem(&keypair->ocsp_staple, &keypair->ocsp_staple_len,
	    staple, len);
}

void
tls_keypair_clear(struct tls_keypair *keypair)
{
	tls_keypair_set_cert_mem(keypair, NULL, 0);
	tls_keypair_set_key_mem(keypair, NULL, 0);
	tls_keypair_set_ocsp_staple_mem(keypair, NULL, 0);

	free(keypair->pubkey_hash);
	keypair->pubkey_hash = NULL;
}

void
tls_keypair_free(struct tls_keypair *keypair)
{
	if (keypair == NULL)
		return;

	tls_keypair_clear(keypair);

	free(keypair);
}

int
tls_keypair_load_cert(struct tls_keypair *keypair, struct tls_error *error,
    X509 **cert)
{
	char *errstr = "unknown";
	BIO *cert_bio = NULL;
	int ssl_err;
	int rv = -1;

	X509_free(*cert);
	*cert = NULL;

	if (keypair->cert_mem == NULL) {
		tls_error_set(error, "keypair has no certificate");
		goto err;
	}
	if ((cert_bio = BIO_new_mem_buf(keypair->cert_mem,
	    keypair->cert_len)) == NULL) {
		tls_error_set(error, "failed to create certificate bio");
		goto err;
	}
	if ((*cert = PEM_read_bio_X509(cert_bio, NULL, tls_password_cb,
	    NULL)) == NULL) {
		if ((ssl_err = ERR_peek_error()) != 0)
		    errstr = ERR_error_string(ssl_err, NULL);
		tls_error_set(error, "failed to load certificate: %s", errstr);
		goto err;
	}

	rv = 0;

 err:
	BIO_free(cert_bio);

	return (rv);
}

int
tls_keypair_pubkey_hash(struct tls_keypair *keypair, struct tls_error *error,
    char **hash)
{
	X509 *cert = NULL;
	char d[EVP_MAX_MD_SIZE], *dhex = NULL;
	int dlen, rv = -1;

	free(*hash);
	*hash = NULL;

	if (tls_keypair_load_cert(keypair, error, &cert) == -1)
		goto err;

	if (X509_pubkey_digest(cert, EVP_sha256(), d, &dlen) != 1)
		goto err;

	if (tls_hex_string(d, dlen, &dhex, NULL) != 0)
		goto err;

	if (asprintf(hash, "SHA256:%s", dhex) == -1) {
		*hash = NULL;
		goto err;
	}

	rv = 0;

 err:
	X509_free(cert);
	free(dhex);

	return (rv);
}
