/* $OpenBSD: dtlstest.c,v 1.2 2020/10/15 17:51:58 jsing Exp $ */
/*
 * Copyright (c) 2020 Joel Sing <jsing@openbsd.org>
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

#include <netinet/in.h>
#include <sys/limits.h>
#include <sys/socket.h>

#include <err.h>
#include <poll.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

const char *server_ca_file;
const char *server_cert_file;
const char *server_key_file;

char dtls_cookie[32];

int debug = 0;

static int
datagram_pair(int *client_sock, int *server_sock,
    struct sockaddr_in *server_sin)
{
	struct sockaddr_in sin;
	socklen_t sock_len;
	int cs = -1, ss = -1;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if ((ss = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		err(1, "server socket");
	if (bind(ss, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		err(1, "server bind");
	sock_len = sizeof(sin);
	if (getsockname(ss, (struct sockaddr *)&sin, &sock_len) == -1)
		err(1, "server getsockname");

	if ((cs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		err(1, "client socket");
	if (connect(cs, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		err(1, "client connect");

	*client_sock = cs;
	*server_sock = ss;
	memcpy(server_sin, &sin, sizeof(sin));

	return 1;
}

static int
poll_timeout(SSL *client, SSL *server)
{
	int client_timeout = 0, server_timeout = 0;
	struct timeval timeout;

	if (DTLSv1_get_timeout(client, &timeout))
		client_timeout = timeout.tv_sec * 1000 + timeout.tv_usec / 1000;

	if (DTLSv1_get_timeout(server, &timeout))
		server_timeout = timeout.tv_sec * 1000 + timeout.tv_usec / 1000;

	if (client_timeout <= 0)
		return server_timeout;
	if (client_timeout > 0 && server_timeout <= 0)
		return client_timeout;
	if (client_timeout < server_timeout)
		return client_timeout;

	return server_timeout;
}

static int
dtls_cookie_generate(SSL *ssl, unsigned char *cookie,
    unsigned int *cookie_len)
{
	arc4random_buf(dtls_cookie, sizeof(dtls_cookie));
	memcpy(cookie, dtls_cookie, sizeof(dtls_cookie));
	*cookie_len = sizeof(dtls_cookie);

	return 1;
}

static int
dtls_cookie_verify(SSL *ssl, const unsigned char *cookie,
    unsigned int cookie_len)
{
	return cookie_len == sizeof(dtls_cookie) &&
	    memcmp(cookie, dtls_cookie, sizeof(dtls_cookie)) == 0;
}

static SSL *
dtls_client(int sock, struct sockaddr_in *server_sin, long mtu)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	BIO *bio = NULL;

	if ((bio = BIO_new_dgram(sock, BIO_NOCLOSE)) == NULL)
		errx(1, "client bio");
	if (!BIO_socket_nbio(sock, 1))
		errx(1, "client nbio");
	if (!BIO_ctrl_set_connected(bio, 1, server_sin))
		errx(1, "client set connected");

	if ((ssl_ctx = SSL_CTX_new(DTLS_method())) == NULL)
		errx(1, "client context");
	SSL_CTX_set_read_ahead(ssl_ctx, 1);

	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "client ssl");

	SSL_set_bio(ssl, bio, bio);
	bio = NULL;

	if (mtu > 0) {
		SSL_set_options(ssl, SSL_OP_NO_QUERY_MTU);
		SSL_set_mtu(ssl, mtu);
	}

	SSL_CTX_free(ssl_ctx);
	BIO_free(bio);

	return ssl;
}

static SSL *
dtls_server(int sock, long options, long mtu)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	BIO *bio = NULL;

	if ((bio = BIO_new_dgram(sock, BIO_NOCLOSE)) == NULL)
		errx(1, "server bio");
	if (!BIO_socket_nbio(sock, 1))
		errx(1, "server nbio");

	if ((ssl_ctx = SSL_CTX_new(DTLS_method())) == NULL)
		errx(1, "server context");

	SSL_CTX_set_cookie_generate_cb(ssl_ctx, dtls_cookie_generate);
	SSL_CTX_set_cookie_verify_cb(ssl_ctx, dtls_cookie_verify);
	SSL_CTX_set_options(ssl_ctx, options);
	SSL_CTX_set_read_ahead(ssl_ctx, 1);

	if (SSL_CTX_use_certificate_file(ssl_ctx, server_cert_file,
	    SSL_FILETYPE_PEM) != 1) {
		fprintf(stderr, "FAIL: Failed to load server certificate");
		goto failure;
	}
	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, server_key_file,
	    SSL_FILETYPE_PEM) != 1) {
		fprintf(stderr, "FAIL: Failed to load server private key");
		goto failure;
	}

	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "server ssl");

	SSL_set_bio(ssl, bio, bio);
	bio = NULL;

	if (mtu > 0) {
		SSL_set_options(ssl, SSL_OP_NO_QUERY_MTU);
		SSL_set_mtu(ssl, mtu);
	}

 failure:
	SSL_CTX_free(ssl_ctx);
	BIO_free(bio);

	return ssl;
}

static int
ssl_error(SSL *ssl, const char *name, const char *desc, int ssl_ret,
    short *events)
{
	int ssl_err;

	ssl_err = SSL_get_error(ssl, ssl_ret);

	if (ssl_err == SSL_ERROR_WANT_READ) {
		*events = POLLIN;
	} else if (ssl_err == SSL_ERROR_WANT_WRITE) {
		*events = POLLOUT;
	} else if (ssl_err == SSL_ERROR_SYSCALL && errno == 0) {
		/* Yup, this is apparently a thing... */
	} else {
		fprintf(stderr, "FAIL: %s %s failed - ssl err = %d, errno = %d\n",
		    name, desc, ssl_err, errno);
		ERR_print_errors_fp(stderr);
		return 0;
	}

	return 1;
}

static int
do_connect(SSL *ssl, const char *name, int *done, short *events)
{
	int ssl_ret;

	if ((ssl_ret = SSL_connect(ssl)) == 1) {
		fprintf(stderr, "INFO: %s connect done\n", name);
		*done = 1;
		return 1;
	}

	return ssl_error(ssl, name, "connect", ssl_ret, events);
}

static int
do_accept(SSL *ssl, const char *name, int *done, short *events)
{
	int ssl_ret;

	if ((ssl_ret = SSL_accept(ssl)) == 1) {
		fprintf(stderr, "INFO: %s accept done\n", name);
		*done = 1;
		return 1;
	}

	return ssl_error(ssl, name, "accept", ssl_ret, events);
}

static int
do_shutdown(SSL *ssl, const char *name, int *done, short *events)
{
	int ssl_ret;

	ssl_ret = SSL_shutdown(ssl);
	if (ssl_ret == 1) {
		fprintf(stderr, "INFO: %s shutdown done\n", name);
		*done = 1;
		return 1;
	}
	return ssl_error(ssl, name, "shutdown", ssl_ret, events);
}

typedef int (*ssl_func)(SSL *ssl, const char *name, int *done, short *events);

static int
do_client_server_loop(SSL *client, ssl_func client_func, SSL *server,
    ssl_func server_func, struct pollfd pfd[2])
{
	int client_done = 0, server_done = 0;
	int i = 0;

	pfd[0].revents = POLLIN;
	pfd[1].revents = POLLIN;

	do {
		if (!client_done) {
			if (debug)
				fprintf(stderr, "DEBUG: client loop\n");
			if (DTLSv1_handle_timeout(client) > 0)
				fprintf(stderr, "INFO: client timeout\n");
			if (!client_func(client, "client", &client_done,
			    &pfd[0].events))
				return 0;
			if (client_done)
				pfd[0].events = 0;
		}
		if (!server_done) {
			if (debug)
				fprintf(stderr, "DEBUG: server loop\n");
			if (DTLSv1_handle_timeout(server) > 0)
				fprintf(stderr, "INFO: server timeout\n");
			if (!server_func(server, "server", &server_done,
			    &pfd[1].events))
				return 0;
			if (server_done)
				pfd[1].events = 0;
		}
		if (poll(pfd, 2, poll_timeout(client, server)) == -1)
			err(1, "poll");

	} while (i++ < 100 && (!client_done || !server_done));

	if (!client_done || !server_done)
		fprintf(stderr, "FAIL: gave up\n");

	return client_done && server_done;
}

struct dtls_test {
	const unsigned char *desc;
	const long mtu;
	const long ssl_options;
};

static struct dtls_test dtls_tests[] = {
	{
		.desc = "DTLS without cookies",
		.ssl_options = 0,
	},
	{
		.desc = "DTLS with cookies",
		.ssl_options = SSL_OP_COOKIE_EXCHANGE,
	},
	{
		.desc = "DTLS with low MTU",
		.mtu = 256,
	},
	{
		.desc = "DTLS with low MTU and cookies",
		.mtu = 256,
		.ssl_options = SSL_OP_COOKIE_EXCHANGE,
	},
};

#define N_DTLS_TESTS (sizeof(dtls_tests) / sizeof(*dtls_tests))

static int
dtlstest(struct dtls_test *dt)
{
	SSL *client = NULL, *server = NULL;
	struct sockaddr_in server_sin;
	struct pollfd pfd[2];
	int client_sock = -1;
	int server_sock = -1;
	int failed = 1;

	fprintf(stderr, "\n== Testing %s... ==\n", dt->desc);

	if (!datagram_pair(&client_sock, &server_sock, &server_sin))
		goto failure;

	if ((client = dtls_client(client_sock, &server_sin, dt->mtu)) == NULL)
		goto failure;
	if ((server = dtls_server(server_sock, dt->ssl_options, dt->mtu)) == NULL)
		goto failure;

	pfd[0].fd = client_sock;
	pfd[0].events = POLLOUT;
	pfd[1].fd = server_sock;
	pfd[1].events = POLLIN;

	if (!do_client_server_loop(client, do_connect, server, do_accept, pfd)) {
		fprintf(stderr, "FAIL: client and server handshake failed\n");
		goto failure;
	}

	/* XXX - do reads and writes. */

	pfd[0].events = POLLOUT;
	pfd[1].events = POLLOUT;

	if (!do_client_server_loop(client, do_shutdown, server, do_shutdown, pfd)) {
		fprintf(stderr, "FAIL: client and server shutdown failed\n");
		goto failure;
	}

	fprintf(stderr, "INFO: Done!\n");

	failed = 0;

 failure:
	if (client_sock != -1)
		close(client_sock);
	if (server_sock != -1)
		close(server_sock);

	SSL_free(client);
	SSL_free(server);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;
	size_t i;

	if (argc != 4) {
		fprintf(stderr, "usage: %s keyfile certfile cafile\n",
		    argv[0]);
		exit(1);
	}

	server_key_file = argv[1];
	server_cert_file = argv[2];
	server_ca_file = argv[3];

	for (i = 0; i < N_DTLS_TESTS; i++)
		failed |= dtlstest(&dtls_tests[i]);

	return failed;
}
