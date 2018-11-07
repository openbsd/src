/*	$OpenBSD: client.c,v 1.1.1.1 2018/11/07 01:08:49 bluhm Exp $	*/
/*
 * Copyright (c) 2018 Alexander Bluhm <bluhm@openbsd.org>
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
#include <sys/socket.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "util.h"

void __dead usage(void);

void __dead
usage(void)
{
	fprintf(stderr, "usage: client host port");
	exit(2);
}

int
main(int argc, char *argv[])
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;
	SSL *ssl;
	BIO *bio;
	SSL_SESSION *session;
	int error;
	char buf[256];
	char *host_port, *host, *port;

	if (argc == 3) {
		host = argv[1];
		port = argv[2];
	} else {
		usage();
	}
	if (asprintf(&host_port, strchr(host, ':') ? "[%s]:%s" : "%s:%s",
	    host, port) == -1)
		err(1, "asprintf host port");

	SSL_library_init();
	SSL_load_error_strings();

	/* setup method and context */
	method = SSLv23_client_method();
	if (method == NULL)
		err_ssl(1, "SSLv23_client_method");
	ctx = SSL_CTX_new(method);
	if (ctx == NULL)
		err_ssl(1, "SSL_CTX_new");

	/* setup ssl and bio for socket operations */
	ssl = SSL_new(ctx);
	if (ssl == NULL)
		err_ssl(1, "SSL_new");
	bio = BIO_new_connect(host_port);
	if (bio == NULL)
		err_ssl(1, "BIO_new_connect");

	print_ciphers(SSL_get_ciphers(ssl));

	/* connect */
	if (BIO_do_connect(bio) <= 0)
		err_ssl(1, "BIO_do_connect");
	printf("connect ");
	print_sockname(bio);
	printf("connect ");
	print_peername(bio);

	/* do ssl client handshake */
	SSL_set_bio(ssl, bio, bio);
	if ((error = SSL_connect(ssl)) <= 0)
		err_ssl(1, "SSL_connect %d", error);

	/* print session statistics */
	session = SSL_get_session(ssl);
	if (session == NULL)
		err_ssl(1, "SSL_get_session");
	if (SSL_SESSION_print_fp(stdout, session) <= 0)
		err_ssl(1, "SSL_SESSION_print_fp");

	/* read server greeting and write client hello over TLS connection */
	if ((error = SSL_read(ssl, buf, 9)) <= 0)
		err_ssl(1, "SSL_read %d", error);
	if (error != 9)
		errx(1, "read not 9 bytes greeting: %d", error);
	buf[9] = '\0';
	printf("<<< %s", buf);
	if (fflush(stdout) != 0)
		err(1, "fflush stdout");
	strlcpy(buf, "hello\n", sizeof(buf));
	printf(">>> %s", buf);
	if (fflush(stdout) != 0)
		err(1, "fflush stdout");
	if ((error = SSL_write(ssl, buf, 6)) <= 0)
		err_ssl(1, "SSL_write %d", error);
	if (error != 6)
		errx(1, "write not 6 bytes hello: %d", error);

	/* shutdown connection */
	if ((error = SSL_shutdown(ssl)) < 0)
		err_ssl(1, "SSL_shutdown unidirectional %d", error);
	if (error <= 0) {
		if ((error = SSL_shutdown(ssl)) <= 0)
			err_ssl(1, "SSL_shutdown bidirectional %d", error);
	}

	/* cleanup and free resources */
	SSL_free(ssl);
	SSL_CTX_free(ctx);

	printf("success\n");

	return 0;
}
