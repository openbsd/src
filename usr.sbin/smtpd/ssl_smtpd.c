/*	$OpenBSD: ssl_smtpd.c,v 1.8 2015/01/16 06:40:21 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <ctype.h>
#include <event.h>
#include <fcntl.h>
#include <limits.h>
#include <imsg.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/err.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"


void *
ssl_mta_init(void *pkiname, char *cert, off_t cert_len)
{
	SSL_CTX	*ctx = NULL;
	SSL	*ssl = NULL;

	ctx = ssl_ctx_create(pkiname, cert, cert_len);

	if ((ssl = SSL_new(ctx)) == NULL)
		goto err;
	if (!SSL_set_ssl_method(ssl, SSLv23_client_method()))
		goto err;

	SSL_CTX_free(ctx);
	return (void *)(ssl);

err:
	if (ssl != NULL)
		SSL_free(ssl);
	if (ctx != NULL)
		SSL_CTX_free(ctx);
	ssl_error("ssl_mta_init");
	return (NULL);
}

/* dummy_verify */
static int
dummy_verify(int ok, X509_STORE_CTX *store)
{
	/*
	 * We *want* SMTP to request an optional client certificate, however we don't want the
	 * verification to take place in the SMTP process. This dummy verify will allow us to
	 * asynchronously verify in the lookup process.
	 */
	return 1;
}

void *
ssl_smtp_init(void *ssl_ctx, void *sni, void *arg)
{
	SSL	*ssl = NULL;
	int	(*cb)(SSL *,int *,void *) = sni;

	log_debug("debug: session_start_ssl: switching to SSL");

	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, dummy_verify);

	if (cb) {
		SSL_CTX_set_tlsext_servername_callback(ssl_ctx, cb);
		SSL_CTX_set_tlsext_servername_arg(ssl_ctx, arg);
	}

	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		goto err;
	if (!SSL_set_ssl_method(ssl, SSLv23_server_method()))
		goto err;

	return (void *)(ssl);

err:
	if (ssl != NULL)
		SSL_free(ssl);
	ssl_error("ssl_smtp_init");
	return (NULL);
}
