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

#ifndef HEADER_RESSL_INTERNAL_H
#define HEADER_RESSL_INTERNAL_H

#include <openssl/ssl.h>

#define HTTPS_PORT "443"

#define _PATH_SSL_CA_FILE "/etc/ssl/cert.pem"

struct ressl_config {
	const char *ca_file;
	const char *ca_path;
	const char *cert_file;
	const char *ciphers;
	const char *key_file;
	int verify;
	int verify_depth;
};

#define RESSL_CLIENT		(1 << 0)
#define RESSL_SERVER		(1 << 1)
#define RESSL_SERVER_CONN	(1 << 2)

struct ressl {
	struct ressl_config *config;
	uint64_t flags;

	int err;
	char *errmsg;

	int socket;

	SSL *ssl_conn;
	SSL_CTX *ssl_ctx;
};

struct ressl *ressl_new(void);
struct ressl *ressl_server_conn(struct ressl *ctx);

int ressl_check_hostname(X509 *cert, const char *host);
int ressl_configure_keypair(struct ressl *ctx);
int ressl_configure_server(struct ressl *ctx);
int ressl_host_port(const char *hostport, char **host, char **port);
int ressl_set_error(struct ressl *ctx, char *fmt, ...);

#endif /* HEADER_RESSL_INTERNAL_H */
