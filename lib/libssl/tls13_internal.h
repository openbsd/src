/* $OpenBSD: tls13_internal.h,v 1.5 2018/11/09 23:56:20 jsing Exp $ */
/*
 * Copyright (c) 2018 Bob Beck <beck@openbsd.org>
 * Copyright (c) 2018 Theo Buehler <tb@openbsd.org>
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

#ifndef HEADER_TLS13_INTERNAL_H
#define HEADER_TLS13_INTERNAL_H

#include <openssl/evp.h>

__BEGIN_HIDDEN_DECLS

struct tls13_secret {
	uint8_t *data;
	size_t len;
};

/* RFC 8446 Section 7.1  Page 92 */
struct tls13_secrets {
	const EVP_MD *digest;
	int resumption;
	int init_done;
	int early_done;
	int handshake_done;
	int schedule_done;
	int insecure; /* Set by tests */
	struct tls13_secret zeros;
	struct tls13_secret empty_hash;
	struct tls13_secret extracted_early;
	struct tls13_secret binder_key;
	struct tls13_secret client_early_traffic;
	struct tls13_secret early_exporter_master;
	struct tls13_secret derived_early;
	struct tls13_secret extracted_handshake;
	struct tls13_secret client_handshake_traffic;
	struct tls13_secret server_handshake_traffic;
	struct tls13_secret derived_handshake;
	struct tls13_secret extracted_master;
	struct tls13_secret client_application_traffic;
	struct tls13_secret server_application_traffic;
	struct tls13_secret exporter_master;
	struct tls13_secret resumption_master;
};

struct tls13_secrets *tls13_secrets_create(const EVP_MD *digest,
    int resumption);
void tls13_secrets_destroy(struct tls13_secrets *secrets);

int tls13_hkdf_expand_label(struct tls13_secret *out, const EVP_MD *digest,
    const struct tls13_secret *secret, const char *label,
    const struct tls13_secret *context);

int tls13_derive_early_secrets(struct tls13_secrets *secrets, uint8_t *psk,
    size_t psk_len, const struct tls13_secret *context);
int tls13_derive_handshake_secrets(struct tls13_secrets *secrets,
    const uint8_t *ecdhe, size_t ecdhe_len, const struct tls13_secret *context);
int tls13_derive_application_secrets(struct tls13_secrets *secrets,
    const struct tls13_secret *context);

struct tls13_ctx;

/*
 * RFC 8446, Section B.3
 *
 * Values listed as "_RESERVED" were used in previous versions of TLS and are
 * listed here for completeness.  TLS 1.3 implementations MUST NOT send them but
 * might receive them from older TLS implementations.
 */
#define	TLS13_MT_HELLO_REQUEST_RESERVED		0
#define	TLS13_MT_CLIENT_HELLO			1
#define	TLS13_MT_SERVER_HELLO			2
#define	TLS13_MT_HELLO_VERIFY_REQUEST_RESERVED	3
#define	TLS13_MT_NEW_SESSION_TICKET		4
#define	TLS13_MT_END_OF_EARLY_DATA		5
#define	TLS13_MT_HELLO_RETRY_REQUEST_RESERVED	6
#define	TLS13_MT_ENCRYPTED_EXTENSIONS		8
#define	TLS13_MT_CERTIFICATE			11
#define	TLS13_MT_SERVER_KEY_EXCHANGE_RESERVED	12
#define	TLS13_MT_CERTIFICATE_REQUEST		13
#define	TLS13_MT_SERVER_HELLO_DONE_RESERVED	14
#define	TLS13_MT_CERTIFICATE_VERIFY		15
#define	TLS13_MT_CLIENT_KEY_EXCHANGE_RESERVED	16
#define	TLS13_MT_FINISHED			20
#define	TLS13_MT_CERTIFICATE_URL_RESERVED	21
#define	TLS13_MT_CERTIFICATE_STATUS_RESERVED	22
#define	TLS13_MT_SUPPLEMENTAL_DATA_RESERVED	23
#define	TLS13_MT_KEY_UPDATE			24
#define	TLS13_MT_MESSAGE_HASH			254

int tls13_client_hello_send(struct tls13_ctx *ctx);
int tls13_client_hello_recv(struct tls13_ctx *ctx);
int tls13_client_hello_retry_send(struct tls13_ctx *ctx);
int tls13_client_hello_retry_recv(struct tls13_ctx *ctx);
int tls13_client_end_of_early_data_send(struct tls13_ctx *ctx);
int tls13_client_end_of_early_data_recv(struct tls13_ctx *ctx);
int tls13_client_certificate_send(struct tls13_ctx *ctx);
int tls13_client_certificate_recv(struct tls13_ctx *ctx);
int tls13_client_certificate_verify_send(struct tls13_ctx *ctx);
int tls13_client_certificate_verify_recv(struct tls13_ctx *ctx);
int tls13_client_finished_recv(struct tls13_ctx *ctx);
int tls13_client_finished_send(struct tls13_ctx *ctx);
int tls13_client_key_update_send(struct tls13_ctx *ctx);
int tls13_client_key_update_recv(struct tls13_ctx *ctx);
int tls13_server_hello_recv(struct tls13_ctx *ctx);
int tls13_server_hello_send(struct tls13_ctx *ctx);
int tls13_server_new_session_ticket_recv(struct tls13_ctx *ctx);
int tls13_server_new_session_ticket_send(struct tls13_ctx *ctx);
int tls13_server_encrypted_extensions_recv(struct tls13_ctx *ctx);
int tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx);
int tls13_server_certificate_recv(struct tls13_ctx *ctx);
int tls13_server_certificate_send(struct tls13_ctx *ctx);
int tls13_server_certificate_request_recv(struct tls13_ctx *ctx);
int tls13_server_certificate_request_send(struct tls13_ctx *ctx);
int tls13_server_certificate_verify_send(struct tls13_ctx *ctx);
int tls13_server_certificate_verify_recv(struct tls13_ctx *ctx);
int tls13_server_finished_recv(struct tls13_ctx *ctx);
int tls13_server_finished_send(struct tls13_ctx *ctx);
int tls13_server_key_update_recv(struct tls13_ctx *ctx);
int tls13_server_key_update_send(struct tls13_ctx *ctx);
int tls13_server_message_hash_recv(struct tls13_ctx *ctx);
int tls13_server_message_hash_send(struct tls13_ctx *ctx);

__END_HIDDEN_DECLS

#endif
