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

#ifndef HEADER_RESSL_H
#define HEADER_RESSL_H

struct ressl;
struct ressl_config;

int ressl_init(void);

const char *ressl_error(struct ressl *ctx);

struct ressl_config *ressl_config_new(void);
void ressl_config_free(struct ressl_config *config);

void ressl_config_ca_file(struct ressl_config *config, char *ca_file);
void ressl_config_ca_path(struct ressl_config *config, char *ca_path);
void ressl_config_ciphers(struct ressl_config *config, char *ciphers);
void ressl_config_verify_depth(struct ressl_config *config, int verify_depth);

void ressl_config_insecure(struct ressl_config *config);
void ressl_config_secure(struct ressl_config *config);

struct ressl *ressl_new(struct ressl_config *config);
void ressl_reset(struct ressl *ctx);
void ressl_free(struct ressl *ctx);

int ressl_connect(struct ressl *ctx, const char *host, const char *port);
int ressl_connect_socket(struct ressl *ctx, int s, const char *hostname);
int ressl_read(struct ressl *ctx, char *buf, size_t buflen, size_t *outlen);
int ressl_write(struct ressl *ctx, const char *buf, size_t buflen,
    size_t *outlen);
int ressl_close(struct ressl *ctx);

#endif /* HEADER_RESSL_H */
