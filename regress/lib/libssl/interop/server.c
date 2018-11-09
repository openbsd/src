/*	$OpenBSD: server.c,v 1.4 2018/11/09 06:30:41 bluhm Exp $	*/
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
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "util.h"

void __dead usage(void);

void __dead
usage(void)
{
	fprintf(stderr,
	    "usage: server [-vv] [-C CA] [-c crt -k key] [host port]");
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
	int error, verify = 0;
	char buf[256], ch;
	char *ca = NULL, *crt = NULL, *key = NULL;
	char *host_port, *host = "127.0.0.1", *port = "0";

	while ((ch = getopt(argc, argv, "C:c:k:v")) != -1) {
		switch (ch) {
		case 'C':
			ca = optarg;
			break;
		case 'c':
			crt = optarg;
			break;
		case 'k':
			key = optarg;
			break;
		case 'v':
			/* use twice to force client cert */
			verify++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 2) {
		host = argv[0];
		port = argv[1];
	} else if (argc != 0) {
		usage();
	}
	if (asprintf(&host_port, strchr(host, ':') ? "[%s]:%s" : "%s:%s",
	    host, port) == -1)
		err(1, "asprintf host port");
	if ((crt == NULL && key != NULL) || (crt != NULL && key == NULL))
		errx(1, "certificate and private key must be used together");
	if (crt == NULL && asprintf(&crt, "%s.crt", host) == -1)
		err(1, "asprintf crt");
	if (key == NULL && asprintf(&key, "%s.key", host) == -1)
		err(1, "asprintf key");

	SSL_library_init();
	SSL_load_error_strings();
	print_version();

	/* setup method and context */
#if OPENSSL_VERSION_NUMBER >= 0x1010000f
	method = TLS_server_method();
	if (method == NULL)
		err_ssl(1, "TLS_server_method");
#else
	method = SSLv23_server_method();
	if (method == NULL)
		err_ssl(1, "SSLv23_server_method");
#endif
	ctx = SSL_CTX_new(method);
	if (ctx == NULL)
		err_ssl(1, "SSL_CTX_new");

	/* needed when linking with OpenSSL 1.0.2p */
	if (SSL_CTX_set_ecdh_auto(ctx, 1) <= 0)
		err_ssl(1, "SSL_CTX_set_ecdh_auto");

	/* load server certificate */
	if (SSL_CTX_use_certificate_file(ctx, crt, SSL_FILETYPE_PEM) <= 0)
		err_ssl(1, "SSL_CTX_use_certificate_file");
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0)
		err_ssl(1, "SSL_CTX_use_PrivateKey_file");
	if (SSL_CTX_check_private_key(ctx) <= 0)
		err_ssl(1, "SSL_CTX_check_private_key");

	/* request client certificate and verify it */
	if (ca != NULL) {
		STACK_OF(X509_NAME) *x509stack;

		x509stack = SSL_load_client_CA_file(ca);
		if (x509stack == NULL)
			err_ssl(1, "SSL_load_client_CA_file");
		SSL_CTX_set_client_CA_list(ctx, x509stack);
		if (SSL_CTX_load_verify_locations(ctx, ca, NULL) <= 0)
			err_ssl(1, "SSL_CTX_load_verify_locations");
	}
	SSL_CTX_set_verify(ctx,
	    verify == 0 ?  SSL_VERIFY_NONE :
	    verify == 1 ?  SSL_VERIFY_PEER :
	    SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
	    verify_callback);

	/* setup ssl and bio for socket operations */
	ssl = SSL_new(ctx);
	if (ssl == NULL)
		err_ssl(1, "SSL_new");
	bio = BIO_new_accept(host_port);
	if (bio == NULL)
		err_ssl(1, "BIO_new_accept");
	print_ciphers(SSL_get_ciphers(ssl));

	/* bind, listen */
	if (BIO_do_accept(bio) <= 0)
		err_ssl(1, "BIO_do_accept setup");
	printf("listen ");
	print_sockname(bio);

	/* fork to background, set timeout, and accept */
	if (daemon(1, 1) == -1)
		err(1, "daemon");
	if ((int)alarm(60) == -1)
		err(1, "alarm");
	if (BIO_do_accept(bio) <= 0)
		err_ssl(1, "BIO_do_accept wait");
	bio = BIO_pop(bio);
	printf("accept ");
	print_sockname(bio);
	printf("accept ");
	print_peername(bio);

	/* do ssl server handshake */
	SSL_set_bio(ssl, bio, bio);
	if ((error = SSL_accept(ssl)) <= 0)
		err_ssl(1, "SSL_accept %d", error);

	/* print session statistics */
	session = SSL_get_session(ssl);
	if (session == NULL)
		err_ssl(1, "SSL_get_session");
	if (SSL_SESSION_print_fp(stdout, session) <= 0)
		err_ssl(1, "SSL_SESSION_print_fp");

	/* write server greeting and read client hello over TLS connection */
	strlcpy(buf, "greeting\n", sizeof(buf));
	printf(">>> %s", buf);
	if (fflush(stdout) != 0)
		err(1, "fflush stdout");
	if ((error = SSL_write(ssl, buf, 9)) <= 0)
		err_ssl(1, "SSL_write %d", error);
	if (error != 9)
		errx(1, "write not 9 bytes greeting: %d", error);
	if ((error = SSL_read(ssl, buf, 6)) <= 0)
		err_ssl(1, "SSL_read %d", error);
	if (error != 6)
		errx(1, "read not 6 bytes hello: %d", error);
	buf[6] = '\0';
	printf("<<< %s", buf);
	if (fflush(stdout) != 0)
		err(1, "fflush stdout");

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
