/* $OpenBSD: tls12_internal.h,v 1.4 2026/09/22 03:45:18 jsing Exp $ */
/*
 * Copyright (c) 2022 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HEADER_TLS12_INTERNAL_H
#define HEADER_TLS12_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <openssl/ssl.h>

__BEGIN_HIDDEN_DECLS

#define TLS12_IO_SUCCESS		 1
#define TLS12_IO_EOF			 0
#define TLS12_IO_FAILURE		-1
#define TLS12_IO_ALERT			-2
#define TLS12_IO_WANT_POLLIN		-3
#define TLS12_IO_WANT_POLLOUT		-4
#define TLS12_IO_WANT_RETRY		-5 /* Retry the previous call immediately. */
#define TLS12_IO_USE_LEGACY		-6
#define TLS12_IO_RECORD_VERSION		-7
#define TLS12_IO_RECORD_OVERFLOW	-8

struct tls12_record_layer;

struct tls12_record_layer *tls12_record_layer_new(void);
void tls12_record_layer_free(struct tls12_record_layer *rl);
void tls12_record_layer_alert(struct tls12_record_layer *rl,
    uint8_t *alert_desc);
int tls12_record_layer_write_overhead(struct tls12_record_layer *rl,
    size_t *overhead);
int tls12_record_layer_read_protected(struct tls12_record_layer *rl);
int tls12_record_layer_write_protected(struct tls12_record_layer *rl);
void tls12_record_layer_set_aead(struct tls12_record_layer *rl,
    const EVP_AEAD *aead);
void tls12_record_layer_set_cipher_hash(struct tls12_record_layer *rl,
    const EVP_CIPHER *cipher, const EVP_MD *handshake_hash,
    const EVP_MD *mac_hash);
void tls12_record_layer_set_version(struct tls12_record_layer *rl,
    uint16_t version);
void tls12_record_layer_set_initial_epoch(struct tls12_record_layer *rl,
    uint16_t epoch);
struct tls_content *tls12_record_layer_rcontent(struct tls12_record_layer *rl);
uint16_t tls12_record_layer_read_epoch(struct tls12_record_layer *rl);
uint16_t tls12_record_layer_write_epoch(struct tls12_record_layer *rl);
int tls12_record_layer_use_write_epoch(struct tls12_record_layer *rl,
    uint16_t epoch);
void tls12_record_layer_write_epoch_done(struct tls12_record_layer *rl,
    uint16_t epoch);
void tls12_record_layer_clear_read_state(struct tls12_record_layer *rl);
void tls12_record_layer_clear_write_state(struct tls12_record_layer *rl);
void tls12_record_layer_reflect_seq_num(struct tls12_record_layer *rl);
int tls12_record_layer_change_read_cipher_state(struct tls12_record_layer *rl,
    CBS *mac_key, CBS *key, CBS *iv);
int tls12_record_layer_change_write_cipher_state(struct tls12_record_layer *rl,
    CBS *mac_key, CBS *key, CBS *iv);
int tls12_record_layer_open_record(struct tls12_record_layer *rl, CBS *cbs);
int tls12_record_layer_seal_record(struct tls12_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len,
    CBB *out);

/*
 * Legacy interfaces.
 */
ssize_t tls12_legacy_wire_read_cb(void *buf, size_t n, void *arg);
ssize_t tls12_legacy_wire_write_cb(const void *buf, size_t n, void *arg);

int tls12_exporter(SSL *s, const uint8_t *label, size_t label_len,
    const uint8_t *context_value, size_t context_value_len, int use_context,
    uint8_t *out, size_t out_len);

__END_HIDDEN_DECLS

#endif
