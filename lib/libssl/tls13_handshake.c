/*	$OpenBSD: tls13_handshake.c,v 1.10 2019/01/19 04:02:29 jsing Exp $	*/
/*
 * Copyright (c) 2018-2019 Theo Buehler <tb@openbsd.org>
 * Copyright (c) 2019 Joel Sing <jsing@openbsd.org>
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

#include "tls13_handshake.h"
#include "tls13_internal.h"

/* Based on RFC 8446 and inspired by s2n's TLS 1.2 state machine. */

/* Record types */
#define TLS13_HANDSHAKE		1
#define TLS13_APPLICATION_DATA	2

/* Indexing into the state machine */
struct tls13_handshake {
	uint8_t			hs_type;
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
	[APPLICATION_DATA] = {
		.record_type = TLS13_APPLICATION_DATA,
		.handshake_type = 0,
		.sender = TLS13_HS_BOTH,
		.send = NULL,
		.recv = NULL,
	},
};

static enum tls13_message_type handshakes[][TLS13_NUM_MESSAGE_TYPES] = {
	[INITIAL] = {
		CLIENT_HELLO,
		SERVER_HELLO,
	},
	[NEGOTIATED] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE_REQUEST,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_CERTIFICATE,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_CCV] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE_REQUEST,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_CERTIFICATE,
		CLIENT_CERTIFICATE_VERIFY,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITHOUT_CR] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_ENCRYPTED_EXTENSIONS,
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
	[NEGOTIATED | WITH_HRR] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE_REQUEST,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_CERTIFICATE,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_HRR | WITH_CCV] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE_REQUEST,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_CERTIFICATE,
		CLIENT_CERTIFICATE_VERIFY,
		CLIENT_FINISHED,
		APPLICATION_DATA,
	},
	[NEGOTIATED | WITH_HRR | WITHOUT_CR] = {
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
	[NEGOTIATED | WITH_HRR | WITH_PSK] = {
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
	int ret;

	ctx->mode = TLS13_HS_CLIENT;

	for (;;) {
		if ((action = tls13_handshake_active_action(ctx)) == NULL)
			return TLS13_IO_FAILURE;

		if (action->sender == TLS13_HS_BOTH)
			return TLS13_IO_SUCCESS;

		if (action->sender == TLS13_HS_CLIENT) {
			if ((ret = tls13_handshake_send_action(ctx, action)) <= 0)
				return ret;
		} else {
			if ((ret = tls13_handshake_recv_action(ctx, action)) <= 0)
				return ret;
		}

		if (!tls13_handshake_advance_state_machine(ctx))
			return TLS13_IO_FAILURE;
	}
}

int
tls13_accept(struct tls13_ctx *ctx)
{
	struct tls13_handshake_action *action;
	int ret;

	ctx->mode = TLS13_HS_SERVER;

	for (;;) {
		if ((action = tls13_handshake_active_action(ctx)) == NULL)
			return TLS13_IO_FAILURE;

		if (action->sender == TLS13_HS_BOTH)
			return TLS13_IO_SUCCESS;

		if (action->sender == TLS13_HS_SERVER) {
			if ((ret = tls13_handshake_send_action(ctx, action)) <= 0)
				return ret;
		} else {
			if ((ret = tls13_handshake_recv_action(ctx, action)) <= 0)
				return ret;
		}

		if (!tls13_handshake_advance_state_machine(ctx))
			return TLS13_IO_FAILURE;
	}

	return 1;
}

int
tls13_handshake_advance_state_machine(struct tls13_ctx *ctx)
{
	ctx->handshake.message_number++;
	return 0;
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
	uint8_t msg_type;

	msg_type = 0; /* XXX */

	/*
	 * In TLSv1.3 there is no way to know if you're going to receive a
	 * certificate request message or not, hence we have to special case it
	 * here. The receive handler also knows how to deal with this situation.
	 */
	if (msg_type != action->handshake_type &&
	    (msg_type != TLS13_MT_CERTIFICATE ||
	     action->handshake_type != TLS13_MT_CERTIFICATE_REQUEST)) {
		/* XXX send unexpected message alert */
		return TLS13_IO_FAILURE;
	}

	return action->recv(ctx);
}

int
tls13_client_hello_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_hello_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_hello_retry_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_hello_retry_recv(struct tls13_ctx *ctx)
{
	return 0;
}


int
tls13_client_end_of_early_data_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_end_of_early_data_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_verify_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_verify_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_finished_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_finished_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_key_update_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_key_update_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_hello_recv(struct tls13_ctx *ctx)
{
	ctx->handshake.hs_type |= NEGOTIATED;

	return 0;
}

int
tls13_server_hello_send(struct tls13_ctx *ctx)
{
	ctx->handshake.hs_type |= NEGOTIATED;

	return 0;
}

int
tls13_server_encrypted_extensions_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_request_recv(struct tls13_ctx *ctx)
{
	uint8_t msg_type = 0; /* XXX */

	/*
	 * Thanks to poor state design in the RFC, this function can be called
	 * when we actually have a certificate message instead of a certificate
	 * request... in that case we call the certificate handler after
	 * switching state, to avoid advancing state.
	 */
	if (msg_type == TLS13_MT_CERTIFICATE) {
		ctx->handshake.hs_type |= WITHOUT_CR;
		return tls13_server_certificate_recv(ctx);
	}

	return 0;
}

int
tls13_server_certificate_request_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_verify_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_verify_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_finished_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_finished_send(struct tls13_ctx *ctx)
{
	return 0;
}
