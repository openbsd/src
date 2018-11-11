/*	$OpenBSD: tls13_handshake.c,v 1.7 2018/11/11 06:49:35 beck Exp $	*/
/*
 * Copyright (c) 2018 Theo Buehler <tb@openbsd.org>
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

#include <stddef.h>

#include "tls13_internal.h"

/* Based on RFC 8446 and inspired by s2n's TLS 1.2 state machine. */

/* Record types */
#define TLS13_HANDSHAKE		1
#define TLS13_APPLICATION_DATA	2

/* Indexing into the state machine */
struct tls13_handshake {
	uint8_t			hs_type;
#define INITIAL				0x00
#define NEGOTIATED			0x01
#define WITH_CERT_REQ			0x02
#define WITH_HELLO_RET_REQ		0x04
#define WITH_PSK			0x08
	int			message_number;
};

struct tls13_ctx {
	uint8_t			mode;
	struct tls13_handshake	handshake;
};

struct tls13_handshake_action {
	uint8_t			record_type;
	uint8_t			handshake_type;

	uint8_t			sender;
#define TLS13_HS_CLIENT		1
#define TLS13_HS_SERVER		2
#define TLS13_HS_BOTH		(TLS13_HS_CLIENT | TLS13_HS_SERVER)

	int (*send)(struct tls13_ctx *ctx);
	int (*recv)(struct tls13_ctx *ctx);
};

enum tls13_message_type tls13_handshake_active_state(struct tls13_ctx *ctx);

int tls13_connect(struct tls13_ctx *ctx);
int tls13_accept(struct tls13_ctx *ctx);

int tls13_handshake_advance_state_machine(struct tls13_ctx *ctx);

int tls13_handshake_send_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action);
int tls13_handshake_recv_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action);

enum tls13_message_type {
	INVALID,
	CLIENT_HELLO,
	CLIENT_HELLO_RETRY,
	CLIENT_END_OF_EARLY_DATA,
	CLIENT_CERTIFICATE,
	CLIENT_CERTIFICATE_VERIFY,
	CLIENT_FINISHED,
	CLIENT_KEY_UPDATE,
	SERVER_HELLO,
	SERVER_NEW_SESSION_TICKET,
	SERVER_ENCRYPTED_EXTENSIONS,
	SERVER_CERTIFICATE,
	SERVER_CERTIFICATE_VERIFY,
	SERVER_CERTIFICATE_REQUEST,
	SERVER_FINISHED,
	SERVER_KEY_UPDATE,
	SERVER_MESSAGE_HASH,
	APPLICATION_DATA,
};

struct tls13_handshake_action state_machine[] = {
	[CLIENT_HELLO] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_CLIENT_HELLO,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_hello_send,
		.recv = tls13_client_hello_recv,
	},
	[CLIENT_HELLO_RETRY] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_CLIENT_HELLO,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_hello_retry_send,
		.recv = tls13_client_hello_retry_recv,
	},
	[CLIENT_END_OF_EARLY_DATA] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_END_OF_EARLY_DATA,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_end_of_early_data_send,
		.recv = tls13_client_end_of_early_data_recv,
	},
	[CLIENT_CERTIFICATE] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_CERTIFICATE,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_certificate_send,
		.recv = tls13_client_certificate_recv,
	},
	[CLIENT_CERTIFICATE_VERIFY] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_CERTIFICATE_VERIFY,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_certificate_verify_send,
		.recv = tls13_client_certificate_verify_recv,
	},
	[CLIENT_FINISHED] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_FINISHED,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_finished_send,
		.recv = tls13_client_finished_recv,
	},
	[CLIENT_KEY_UPDATE] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_KEY_UPDATE,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_key_update_send,
		.recv = tls13_client_key_update_recv,
	},
	[SERVER_HELLO] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_SERVER_HELLO,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_hello_send,
		.recv = tls13_server_hello_recv,
	},
	[SERVER_NEW_SESSION_TICKET] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_NEW_SESSION_TICKET,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_new_session_ticket_send,
		.recv = tls13_server_new_session_ticket_recv,
	},
	[SERVER_ENCRYPTED_EXTENSIONS] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_ENCRYPTED_EXTENSIONS,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_encrypted_extensions_send,
		.recv = tls13_server_encrypted_extensions_recv,
	},
	[SERVER_CERTIFICATE] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_CERTIFICATE,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_certificate_send,
		.recv = tls13_server_certificate_recv,
	},
	[SERVER_CERTIFICATE_REQUEST] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_CERTIFICATE,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_certificate_request_send,
		.recv = tls13_server_certificate_request_recv,
	},
	[SERVER_CERTIFICATE_VERIFY] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_CERTIFICATE_VERIFY,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_certificate_verify_send,
		.recv = tls13_server_certificate_verify_recv,
	},
	[SERVER_FINISHED] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_FINISHED,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_finished_send,
		.recv = tls13_server_finished_recv,
	},
	[SERVER_KEY_UPDATE] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_KEY_UPDATE,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_key_update_send,
		.recv = tls13_server_key_update_recv,
	},
	[SERVER_MESSAGE_HASH] = {
		.record_type = TLS13_HANDSHAKE,
		.handshake_type = TLS13_MT_MESSAGE_HASH,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_message_hash_send,
		.recv = tls13_server_message_hash_recv,
	},
	[APPLICATION_DATA] = {
		.record_type = TLS13_APPLICATION_DATA,
		.handshake_type = 0,
		.sender = TLS13_HS_BOTH,
		.send = NULL,
		.recv = NULL,
	},
};

static enum tls13_message_type handshakes[][16] = {
	[INITIAL] = {
		CLIENT_HELLO,
		SERVER_HELLO,
	},
	[NEGOTIATED] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_HELLO_RET_REQ] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_CERT_REQ] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE_REQUEST,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_HELLO_RET_REQ | WITH_CERT_REQ] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE_REQUEST,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_PSK] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_FINISHED,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_HELLO_RET_REQ | WITH_PSK] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_FINISHED,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
};

enum tls13_message_type
tls13_handshake_active_state(struct tls13_ctx *ctx)
{
	struct tls13_handshake hs = ctx->handshake;
	return handshakes[hs.hs_type][hs.message_number];
}

struct tls13_handshake_action *
tls13_handshake_active_action(struct tls13_ctx *ctx)
{
	enum tls13_message_type mt = tls13_handshake_active_state(ctx);
	return &state_machine[mt];
}

int
tls13_connect(struct tls13_ctx *ctx)
{
	struct tls13_handshake_action *action;

	ctx->mode = TLS13_HS_CLIENT;

	for (;;) {
		if ((action = tls13_handshake_active_action(ctx)) == NULL)
			return -1;

		if (action->sender == TLS13_HS_BOTH)
			return 1;

		if (action->sender == TLS13_HS_CLIENT) {
			if (!tls13_handshake_send_action(ctx, action))
				return 0;
		} else {
			if (!tls13_handshake_recv_action(ctx, action))
				return 0;
		}

		if (!tls13_handshake_advance_state_machine(ctx))
			return 0;
	}
}

int
tls13_accept(struct tls13_ctx *ctx)
{
	struct tls13_handshake_action *action;

	ctx->mode = TLS13_HS_SERVER;

	for (;;) {
		if ((action = tls13_handshake_active_action(ctx)) == NULL)
			return -1;

		if (action->sender == TLS13_HS_BOTH)
			return 1;

		if (action->sender == TLS13_HS_SERVER) {
			if (!tls13_handshake_send_action(ctx, action))
				return 0;
		} else {
			if (!tls13_handshake_recv_action(ctx, action))
				return 0;
		}

		if (!tls13_handshake_advance_state_machine(ctx))
			return 0;
	}

	return 1;
}

int
tls13_handshake_advance_state_machine(struct tls13_ctx *ctx)
{
	ctx->handshake.message_number++;
	return 1;
}

int
tls13_handshake_send_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action)
{
	return action->send(ctx);
}

int
tls13_handshake_recv_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action)
{
	return action->recv(ctx);
}

int
tls13_client_hello_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_hello_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_hello_retry_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_hello_retry_recv(struct tls13_ctx *ctx)
{
	return 1;
}


int
tls13_client_end_of_early_data_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_end_of_early_data_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_certificate_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_certificate_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_certificate_verify_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_certificate_verify_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_finished_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_finished_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_key_update_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_client_key_update_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_hello_recv(struct tls13_ctx *ctx)
{
	ctx->handshake.hs_type |= NEGOTIATED;

	return 1;
}

int
tls13_server_hello_send(struct tls13_ctx *ctx)
{
	ctx->handshake.hs_type |= NEGOTIATED;

	return 1;
}

int
tls13_server_new_session_ticket_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_new_session_ticket_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_encrypted_extensions_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_certificate_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_certificate_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_certificate_request_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_certificate_request_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_certificate_verify_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_certificate_verify_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_finished_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_finished_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_key_update_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_key_update_send(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_message_hash_recv(struct tls13_ctx *ctx)
{
	return 1;
}

int
tls13_server_message_hash_send(struct tls13_ctx *ctx)
{
	return 1;
}
