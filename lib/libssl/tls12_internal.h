/* $OpenBSD: tls12_internal.h,v 1.7 2026/09/22 19:29:39 jsing Exp $ */
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

#include "bytestring.h"
#include "tls_internal.h"

__BEGIN_HIDDEN_DECLS

#define TLS12_HS_CLIENT			1
#define TLS12_HS_SERVER			2

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

#define TLS12_ALERT_CLOSE_NOTIFY			0
#define TLS12_ALERT_UNEXPECTED_MESSAGE			10
#define TLS12_ALERT_BAD_RECORD_MAC			20
#define TLS12_ALERT_RECORD_OVERFLOW			22
#define TLS12_ALERT_DECOMPRESSION_FAILURE		30
#define TLS12_ALERT_HANDSHAKE_FAILURE			40
#define TLS12_ALERT_BAD_CERTIFICATE			42
#define TLS12_ALERT_UNSUPPORTED_CERTIFICATE		43
#define TLS12_ALERT_CERTIFICATE_REVOKED			44
#define TLS12_ALERT_CERTIFICATE_EXPIRED			45
#define TLS12_ALERT_CERTIFICATE_UNKNOWN			46
#define TLS12_ALERT_ILLEGAL_PARAMETER			47
#define TLS12_ALERT_UNKNOWN_CA				48
#define TLS12_ALERT_ACCESS_DENIED			49
#define TLS12_ALERT_DECODE_ERROR			50
#define TLS12_ALERT_DECRYPT_ERROR			51
#define TLS12_ALERT_PROTOCOL_VERSION			70
#define TLS12_ALERT_INSUFFICIENT_SECURITY		71
#define TLS12_ALERT_INTERNAL_ERROR			80
#define TLS12_ALERT_USER_CANCELED			90
#define TLS12_ALERT_NO_RENEGOTIATION			100
#define TLS12_ALERT_UNSUPPORTED_EXTENSION		110

#define TLS12_INFO_HANDSHAKE_STARTED			SSL_CB_HANDSHAKE_START
#define TLS12_INFO_HANDSHAKE_COMPLETED			SSL_CB_HANDSHAKE_DONE
#define TLS12_INFO_ACCEPT_LOOP				SSL_CB_ACCEPT_LOOP
#define TLS12_INFO_CONNECT_LOOP				SSL_CB_CONNECT_LOOP
#define TLS12_INFO_ACCEPT_EXIT				SSL_CB_ACCEPT_EXIT
#define TLS12_INFO_CONNECT_EXIT				SSL_CB_CONNECT_EXIT

typedef void (*tls12_alert_cb)(uint8_t _alert_level, uint8_t _alert_desc,
    void *_cb_arg);
typedef void (*tls12_ccs_cb)(void *_cb_arg);
typedef void (*tls12_handshake_message_cb)(void *_cb_arg);
typedef void (*tls12_info_cb)(void *_cb_arg, int _state, int _ret);

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

ssize_t tls12_read_handshake_data(struct tls12_record_layer *rl, uint8_t *buf, size_t n);
ssize_t tls12_write_handshake_data(struct tls12_record_layer *rl, const uint8_t *buf,
    size_t n);

ssize_t tls12_send_alert(struct tls12_record_layer *rl, uint8_t alert_desc);

struct tls12_handshake_stage {
	uint8_t	hs_type;
	uint8_t	message_number;
};

struct ssl_handshake_tls12_st;

struct tls12_error {
	int code;
	int subcode;
	int errnum;
	const char *file;
	int line;
	char *msg;
};

struct tls12_ctx {
	struct tls12_error error;

	SSL *ssl;
	struct ssl_handshake_st *hs;
	uint8_t	mode;
	struct tls12_handshake_stage handshake_stage;
	int handshake_started;
	int handshake_completed;
	int need_flush;

	int close_notify_sent;
	int close_notify_recv;

	struct tls12_record_layer *rl;
	struct tls12_handshake_msg *hs_msg;
	uint8_t alert;

	tls12_alert_cb alert_sent_cb;
	tls12_alert_cb alert_recv_cb;
	tls12_ccs_cb ccs_sent_cb;
	tls12_ccs_cb ccs_recv_cb;
	tls12_handshake_message_cb handshake_message_sent_cb;
	tls12_handshake_message_cb handshake_message_recv_cb;
	tls12_info_cb info_cb;
};

/*
 * Handshake Messages.
 */
struct tls12_handshake_msg;

struct tls12_handshake_msg *tls12_handshake_msg_new(void);
void tls12_handshake_msg_free(struct tls12_handshake_msg *msg);
void tls12_handshake_msg_data(struct tls12_handshake_msg *msg, CBS *cbs);
uint8_t tls12_handshake_msg_type(struct tls12_handshake_msg *msg);
int tls12_handshake_msg_content(struct tls12_handshake_msg *msg, CBS *cbs);
int tls12_handshake_msg_start(struct tls12_handshake_msg *msg, CBB *body,
    uint8_t msg_type);
int tls12_handshake_msg_finish(struct tls12_handshake_msg *msg);
int tls12_handshake_msg_recv(struct tls12_handshake_msg *msg,
    struct tls12_record_layer *rl);
int tls12_handshake_msg_send(struct tls12_handshake_msg *msg,
    struct tls12_record_layer *rl);

/*
 * Legacy interfaces.
 */
ssize_t tls12_legacy_wire_read_cb(void *buf, size_t n, void *arg);
ssize_t tls12_legacy_wire_write_cb(const void *buf, size_t n, void *arg);

int tls12_exporter(SSL *s, const uint8_t *label, size_t label_len,
    const uint8_t *context_value, size_t context_value_len, int use_context,
    uint8_t *out, size_t out_len);

/*
 * Message Types - RFC 5246 section 7.4.
 */
#define TLS12_MT_HELLO_REQUEST			0
#define TLS12_MT_CLIENT_HELLO			1
#define TLS12_MT_SERVER_HELLO			2
#define	TLS12_MT_NEW_SESSION_TICKET		4
#define TLS12_MT_CERTIFICATE			11
#define TLS12_MT_SERVER_KEY_EXCHANGE		12
#define TLS12_MT_CERTIFICATE_REQUEST		13
#define TLS12_MT_SERVER_HELLO_DONE		14
#define TLS12_MT_CERTIFICATE_VERIFY		15
#define TLS12_MT_CLIENT_KEY_EXCHANGE		16
#define TLS12_MT_FINISHED			20

int tls12_handshake_msg_record(struct tls12_ctx *ctx);
int tls12_handshake_perform(struct tls12_ctx *ctx);

int tls12_client_init(struct tls12_ctx *ctx);
int tls12_server_init(struct tls12_ctx *ctx);
int tls12_client_connect(struct tls12_ctx *ctx);
int tls12_server_accept(struct tls12_ctx *ctx);

int tls12_client_hello_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_client_hello_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_client_certificate_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_client_certificate_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_client_certificate_verify_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_client_certificate_verify_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_client_key_exchange_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_client_key_exchange_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_client_finished_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_client_finished_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_hello_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_hello_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_server_certificate_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_certificate_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_server_certificate_request_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_certificate_request_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_server_certificate_verify_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_server_certificate_verify_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_key_exchange_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_server_key_exchange_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_hello_done_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_server_hello_done_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_new_session_ticket_send(struct tls12_ctx *ctx, CBB *cbb);
int tls12_server_new_session_ticket_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_finished_recv(struct tls12_ctx *ctx, CBS *cbs);
int tls12_server_finished_send(struct tls12_ctx *ctx, CBB *cbb);

void tls12_error_clear(struct tls12_error *error);

int tls12_error_set(struct tls12_error *error, int code, int subcode,
    const char *file, int line, const char *fmt, ...);
int tls12_error_setx(struct tls12_error *error, int code, int subcode,
    const char *file, int line, const char *fmt, ...);

#define tls12_set_error(ctx, code, subcode, fmt, ...) \
	tls12_error_set(&(ctx)->error, (code), (subcode), OPENSSL_FILE, OPENSSL_LINE, \
	    (fmt), __VA_ARGS__)
#define tls12_set_errorx(ctx, code, subcode, fmt, ...) \
	tls12_error_setx(&(ctx)->error, (code), (subcode), OPENSSL_FILE, OPENSSL_LINE, \
	    (fmt), __VA_ARGS__)

__END_HIDDEN_DECLS

#endif
