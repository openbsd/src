/*      $OpenBSD: ssl_privsep.c,v 1.8 2015/01/16 15:08:52 reyk Exp $    */

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

#include <sys/types.h>
#include <sys/uio.h>

#include <unistd.h>
#include <stdio.h>

#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

int	 ssl_ctx_use_private_key(SSL_CTX *, char *, off_t);
int	 ssl_ctx_load_verify_memory(SSL_CTX *, char *, off_t);
int	 ssl_by_mem_ctrl(X509_LOOKUP *, int, const char *, long, char **);

X509_LOOKUP_METHOD x509_mem_lookup = {
	"Load cert from memory",
	NULL,			/* new */
	NULL,			/* free */
	NULL,			/* init */
	NULL,			/* shutdown */
	ssl_by_mem_ctrl,	/* ctrl */
	NULL,			/* get_by_subject */
	NULL,			/* get_by_issuer_serial */
	NULL,			/* get_by_fingerprint */
	NULL,			/* get_by_alias */
};

#define X509_L_ADD_MEM	3

int
ssl_ctx_load_verify_memory(SSL_CTX *ctx, char *buf, off_t len)
{
	X509_LOOKUP		*lu;
	struct iovec		 iov;

	if ((lu = X509_STORE_add_lookup(ctx->cert_store,
	    &x509_mem_lookup)) == NULL)
		return (0);

	iov.iov_base = buf;
	iov.iov_len = len;

	if (!ssl_by_mem_ctrl(lu, X509_L_ADD_MEM,
	    (const char *)&iov, X509_FILETYPE_PEM, NULL))
		return (0);

	return (1);
}

int
ssl_by_mem_ctrl(X509_LOOKUP *lu, int cmd, const char *buf,
    long type, char **ret)
{
	STACK_OF(X509_INFO)	*inf;
	const struct iovec	*iov;
	X509_INFO		*itmp;
	BIO			*in = NULL;
	int			 i, count = 0;

	iov = (const struct iovec *)buf;

	if (type != X509_FILETYPE_PEM)
		goto done;

	if ((in = BIO_new_mem_buf(iov->iov_base, iov->iov_len)) == NULL)
		goto done;

	if ((inf = PEM_X509_INFO_read_bio(in, NULL, NULL, NULL)) == NULL)
		goto done;

	for (i = 0; i < sk_X509_INFO_num(inf); i++) {
		itmp = sk_X509_INFO_value(inf, i);
		if (itmp->x509) {
			X509_STORE_add_cert(lu->store_ctx, itmp->x509);
			count++;
		}
		if (itmp->crl) {
			X509_STORE_add_crl(lu->store_ctx, itmp->crl);
			count++;
		}
	}
	sk_X509_INFO_pop_free(inf, X509_INFO_free);

 done:
	if (!count)
		X509err(X509_F_X509_LOAD_CERT_CRL_FILE,ERR_R_PEM_LIB);

	if (in != NULL)
		BIO_free(in);
	return (count);
}
