/* $OpenBSD: tls_internal.h,v 1.9 2015/02/07 09:50:09 jsing Exp $ */
/*
 * Copyright (c) 2014 Jeremie Courreges-Anglas <jca@openbsd.org>
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

#ifndef HEADER_TLS_INTERNAL_H
#define HEADER_TLS_INTERNAL_H

#include <openssl/ssl.h>

#define HTTPS_PORT "443"

#define _PATH_SSL_CA_FILE "/etc/ssl/cert.pem"

struct tls_config {
	const char *ca_file;
	const char *ca_path;
	char *ca_mem;
	size_t ca_len;
	const char *cert_file;
	char *cert_mem;
	size_t cert_len;
	const char *ciphers;
	int dheparams;
	int ecdhecurve;
	const char *key_file;
	char *key_mem;
	size_t key_len;
	uint32_t protocols;
	int verify_cert;
	int verify_host;
	int verify_depth;
};

#define TLS_CLIENT		(1 << 0)
#define TLS_SERVER		(1 << 1)
#define TLS_SERVER_CONN		(1 << 2)
#define TLS_CONNECTING		(1 << 3)

struct tls {
	struct tls_config *config;
	uint64_t flags;

	int err;
	char *errmsg;

	int socket;

	SSL *ssl_conn;
	SSL_CTX *ssl_ctx;
};

struct tls *tls_new(void);
struct tls *tls_server_conn(struct tls *ctx);

int tls_check_hostname(struct tls *ctx, X509 *cert, const char *host);
int tls_configure_keypair(struct tls *ctx);
int tls_configure_server(struct tls *ctx);
int tls_configure_ssl(struct tls *ctx);
int tls_host_port(const char *hostport, char **host, char **port);
int tls_set_error(struct tls *ctx, char *fmt, ...)
    __attribute__((__format__ (printf, 2, 3)))
    __attribute__((__nonnull__ (2)));
int tls_ssl_error(struct tls *ctx, int ssl_ret, const char *prefix);

#endif /* HEADER_TLS_INTERNAL_H */
