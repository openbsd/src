/* $OpenBSD: x509.c,v 1.2 2005/05/28 08:07:45 marius Exp $ */

/*
 * x509.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@arbor.net>
 * Copyright (c) 2001 Arbor Networks, Inc.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The names of the copyright holders may not be used to endorse or
 *      promote products derived from this software without specific
 *      prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *   THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Vendor: x509.c,v 1.2 2005/04/01 16:47:31 dugsong Exp $
 */

#include <sys/types.h>
#include <sys/uio.h>

#include <openssl/ssl.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "key.h"
#include "extern.h"
#include "x509.h"

#define X509_CERT_MAGIC	"-----BEGIN CERTIFICATE-----"
#define X509_RSA_MAGIC	"-----BEGIN RSA PRIVATE KEY-----"
#define X509_DSA_MAGIC	"-----BEGIN DSA PRIVATE KEY-----"

int
x509_load_public(struct key *k, struct iovec *iov)
{
	BIO *bio;
	X509 *cert;
	EVP_PKEY *evp;
	
	if (strncmp((char *)iov->iov_base, X509_CERT_MAGIC,
	    strlen(X509_CERT_MAGIC)) != 0)
		return (-1);
	
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		return (-1);
	
	if (BIO_write(bio, iov->iov_base, iov->iov_len + 1) <= 0) {
		BIO_free(bio);
		return (-1);
	}
	cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);

	if (cert == NULL)
		return (-1);

	evp = X509_get_pubkey(cert);
	
	if (evp->type == EVP_PKEY_RSA) {
		k->type = KEY_RSA;
		k->data = (void *)RSAPublicKey_dup(evp->pkey.rsa);
	} else if (evp->type == EVP_PKEY_DSA) {
		k->type = KEY_DSA;
		k->data = (void *)evp->pkey.dsa;
		evp->pkey.dsa = NULL;			/* XXX */
	} else {
		X509_free(cert);
		return (-1);
	}
	X509_free(cert);
	
	return (0);
}

int
x509_load_private(struct key *k, struct iovec *iov)
{
	BIO *bio;
	EVP_PKEY *evp;
	
	if (strncmp((char *)iov->iov_base, X509_RSA_MAGIC,
	        strlen(X509_RSA_MAGIC)) != 0 &&
	    strncmp((char *)iov->iov_base, X509_DSA_MAGIC,
		strlen(X509_DSA_MAGIC)) != 0) {
		return (-1);
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		return (-1);
	
	if (BIO_write(bio, iov->iov_base, iov->iov_len + 1) <= 0) {
		BIO_free(bio);
		return (-1);
	}

	evp = PEM_read_bio_PrivateKey(bio, NULL, sign_passwd_cb, NULL);

	BIO_free(bio);

	if (evp == NULL)
		return (-1);

	if (evp->type == EVP_PKEY_RSA) {
		k->type = KEY_RSA;
		k->data = (void *)evp->pkey.rsa;
		evp->pkey.rsa = NULL;			/* XXX */
	} else if (evp->type == EVP_PKEY_DSA) {
		k->type = KEY_DSA;
		k->data = (void *)evp->pkey.dsa;
		evp->pkey.dsa = NULL;			/* XXX */
	} else {
		EVP_PKEY_free(evp);
		return (-1);
	}
	EVP_PKEY_free(evp);
	
	return (0);
}
