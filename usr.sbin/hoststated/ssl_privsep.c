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

/*
 * SSL operations needed when running in a privilege separated environment.
 * Adapted from openssl's ssl_rsa.c by Pierre-Yves Ritschard .
 */

#include <stdio.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

int	 ssl_ctx_use_private_key(SSL_CTX *, int, int);
int	 ssl_ctx_use_certificate_chain(SSL_CTX *, int);

int
ssl_ctx_use_private_key(SSL_CTX *ctx, int fd, int type)
{
	int 		 j;
	int		 ret;
	FILE		*fp;
	BIO		*in;
	EVP_PKEY	*pkey;

	ret = 0;
	pkey = NULL;
	if ((fp = fdopen(fd, "r")) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY_FILE, ERR_R_SYS_LIB);
		return (ret);
	}
	if ((in = BIO_new_fp(fp, BIO_NOCLOSE)) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY_FILE, ERR_R_BUF_LIB);
		goto end;
	}

	if (type == SSL_FILETYPE_PEM) {
		j = ERR_R_PEM_LIB;
		pkey = PEM_read_bio_PrivateKey(in, NULL,
		    ctx->default_passwd_callback,
		    ctx->default_passwd_callback_userdata);
	} else {
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY_FILE,
		    SSL_R_BAD_SSL_FILETYPE);
		goto end;
	}
	if (pkey == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_PRIVATEKEY_FILE, j);
		goto end;
	}
	ret = SSL_CTX_use_PrivateKey(ctx, pkey);
	EVP_PKEY_free(pkey);
end:
	if (in != NULL)
		BIO_free(in);
	return(ret);
}


int
ssl_ctx_use_certificate_chain(SSL_CTX *ctx, int fd)
{
	int	 ret;
	BIO	*in;
	X509	*x;
	FILE	*fp;

	ret = 0;
	x = NULL;
	if ((fp = fdopen(fd, "r")) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_SYS_LIB);
		return (ret);
	}
	if ((in = BIO_new_fp(fp, BIO_NOCLOSE)) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_BUF_LIB);
		goto end;
	}

	x = PEM_read_bio_X509(in, NULL,
	    ctx->default_passwd_callback,
	    ctx->default_passwd_callback_userdata);
	if (x == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_PEM_LIB);
		goto end;
	}

	ret = SSL_CTX_use_certificate(ctx, x);
	if (ERR_peek_error() != 0)
		ret = 0;
	if (ret) {
		/* If we could set up our certificate, now proceed to
		 * the CA certificates.
		 */
		X509		*ca;
		int		 r;
		unsigned long	 err;
		
		if (ctx->extra_certs != NULL) {
			sk_X509_pop_free(ctx->extra_certs, X509_free);
			ctx->extra_certs = NULL;
		}

		while ((ca = PEM_read_bio_X509(in, NULL,
		    ctx->default_passwd_callback,
		    ctx->default_passwd_callback_userdata)) != NULL) {
			r = SSL_CTX_add_extra_chain_cert(ctx, ca);
			if (!r) {
				X509_free(ca);
				ret = 0;
				goto end;
			}
		}
		err = ERR_peek_last_error();
		if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
		    ERR_GET_REASON(err) == PEM_R_NO_START_LINE)
			ERR_clear_error();
		else 
			ret = 0;
	}

end:
	if (x != NULL)
		X509_free(x);
	if (in != NULL)
		BIO_free(in);
	return (ret);
}
