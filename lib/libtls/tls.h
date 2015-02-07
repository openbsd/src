/* $OpenBSD: tls.h,v 1.4 2015/02/07 06:19:26 jsing Exp $ */
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

#ifndef HEADER_TLS_H
#define HEADER_TLS_H

#define TLS_API	20141031

#define TLS_PROTOCOL_TLSv1_0	(1 << 1)
#define TLS_PROTOCOL_TLSv1_1	(1 << 2)
#define TLS_PROTOCOL_TLSv1_2	(1 << 3)
#define TLS_PROTOCOL_TLSv1 \
	(TLS_PROTOCOL_TLSv1_0|TLS_PROTOCOL_TLSv1_1|TLS_PROTOCOL_TLSv1_2)
#define TLS_PROTOCOLS_DEFAULT TLS_PROTOCOL_TLSv1

#define TLS_READ_AGAIN	-2
#define TLS_WRITE_AGAIN	-3

struct tls;
struct tls_config;

int tls_init(void);

const char *tls_error(struct tls *ctx);

struct tls_config *tls_config_new(void);
void tls_config_free(struct tls_config *config);

int tls_config_set_ca_file(struct tls_config *config, const char *ca_file);
int tls_config_set_ca_path(struct tls_config *config, const char *ca_path);
int tls_config_set_ca_mem(struct tls_config *config, const uint8_t *ca,
    size_t len);
int tls_config_set_cert_file(struct tls_config *config, const char *cert_file);
int tls_config_set_cert_mem(struct tls_config *config, const uint8_t *cert,
    size_t len);
int tls_config_set_ciphers(struct tls_config *config, const char *ciphers);
int tls_config_set_dheparams(struct tls_config *config, const char *params);
int tls_config_set_ecdhecurve(struct tls_config *config, const char *name);
int tls_config_set_key_file(struct tls_config *config, const char *key_file);
int tls_config_set_key_mem(struct tls_config *config, const uint8_t *key,
    size_t len);
void tls_config_set_protocols(struct tls_config *config, uint32_t protocols);
void tls_config_set_verify_depth(struct tls_config *config, int verify_depth);

void tls_config_clear_keys(struct tls_config *config);
void tls_config_insecure_noverifyhost(struct tls_config *config);
void tls_config_insecure_noverifycert(struct tls_config *config);
void tls_config_verify(struct tls_config *config);

struct tls *tls_client(void);
struct tls *tls_server(void);
int tls_configure(struct tls *ctx, struct tls_config *config);
void tls_reset(struct tls *ctx);
void tls_free(struct tls *ctx);

int tls_accept_socket(struct tls *ctx, struct tls **cctx, int socket);
int tls_connect(struct tls *ctx, const char *host, const char *port);
int tls_connect_fds(struct tls *ctx, int fd_read, int fd_write,
    const char *hostname);
int tls_connect_socket(struct tls *ctx, int s, const char *hostname);
int tls_read(struct tls *ctx, void *buf, size_t buflen, size_t *outlen);
int tls_write(struct tls *ctx, const void *buf, size_t buflen, size_t *outlen);
int tls_close(struct tls *ctx);

#endif /* HEADER_TLS_H */
