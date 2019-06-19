/*	$OpenBSD: tls13_handshake.c,v 1.35 2019/04/05 20:23:38 tb Exp $	*/
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

#include "ssl_locl.h"
#include "tls13_handshake.h"
#include "tls13_internal.h"

/* Based on RFC 8446 and inspired by s2n's TLS 1.2 state machine. */

struct tls13_handshake_action {
	uint8_t			handshake_type;
	uint8_t			sender;
	uint8_t			handshake_complete;
	uint8_t			preserve_transcript_hash;

	int (*send)(struct tls13_ctx *ctx);
	int (*sent)(struct tls13_ctx *ctx);
	int (*recv)(struct tls13_ctx *ctx);
};

enum tls13_message_type tls13_handshake_active_state(struct tls13_ctx *ctx);

int tls13_accept(struct tls13_ctx *ctx);

struct tls13_handshake_action *
    tls13_handshake_active_action(struct tls13_ctx *ctx);
int tls13_handshake_advance_state_machine(struct tls13_ctx *ctx);

int tls13_handshake_send_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action);
int tls13_handshake_recv_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action);

struct tls13_handshake_action state_machine[] = {
	[CLIENT_HELLO] = {
		.handshake_type = TLS13_MT_CLIENT_HELLO,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_hello_send,
		.recv = tls13_client_hello_recv,
	},
	[CLIENT_HELLO_RETRY] = {
		.handshake_type = TLS13_MT_CLIENT_HELLO,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_hello_retry_send,
		.recv = tls13_client_hello_retry_recv,
	},
	[CLIENT_END_OF_EARLY_DATA] = {
		.handshake_type = TLS13_MT_END_OF_EARLY_DATA,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_end_of_early_data_send,
		.recv = tls13_client_end_of_early_data_recv,
	},
	[CLIENT_CERTIFICATE] = {
		.handshake_type = TLS13_MT_CERTIFICATE,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_certificate_send,
		.recv = tls13_client_certificate_recv,
	},
	[CLIENT_CERTIFICATE_VERIFY] = {
		.handshake_type = TLS13_MT_CERTIFICATE_VERIFY,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_certificate_verify_send,
		.recv = tls13_client_certificate_verify_recv,
	},
	[CLIENT_FINISHED] = {
		.handshake_type = TLS13_MT_FINISHED,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_finished_send,
		.sent = tls13_client_finished_sent,
		.recv = tls13_client_finished_recv,
	},
	[CLIENT_KEY_UPDATE] = {
		.handshake_type = TLS13_MT_KEY_UPDATE,
		.sender = TLS13_HS_CLIENT,
		.send = tls13_client_key_update_send,
		.recv = tls13_client_key_update_recv,
	},
	[SERVER_HELLO] = {
		.handshake_type = TLS13_MT_SERVER_HELLO,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_hello_send,
		.recv = tls13_server_hello_recv,
	},
	[SERVER_HELLO_RETRY] = {
		.handshake_type = TLS13_MT_SERVER_HELLO,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_hello_retry_send,
		.recv = tls13_server_hello_retry_recv,
	},
	[SERVER_ENCRYPTED_EXTENSIONS] = {
		.handshake_type = TLS13_MT_ENCRYPTED_EXTENSIONS,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_encrypted_extensions_send,
		.recv = tls13_server_encrypted_extensions_recv,
	},
	[SERVER_CERTIFICATE] = {
		.handshake_type = TLS13_MT_CERTIFICATE,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_certificate_send,
		.recv = tls13_server_certificate_recv,
	},
	[SERVER_CERTIFICATE_REQUEST] = {
		.handshake_type = TLS13_MT_CERTIFICATE,
		.sender = TLS13_HS_SERVER,
		.send = tls13_server_certificate_request_send,
		.recv = tls13_server_certificate_request_recv,
	},
	[SERVER_CERTIFICATE_VERIFY] = {
		.handshake_type = TLS13_MT_CERTIFICATE_VERIFY,
		.sender = TLS13_HS_SERVER,
		.preserve_transcript_hash = 1,
		.send = tls13_server_certificate_verify_send,
		.recv = tls13_server_certificate_verify_recv,
	},
	[SERVER_FINISHED] = {
		.handshake_type = TLS13_MT_FINISHED,
		.sender = TLS13_HS_SERVER,
		.preserve_transcript_hash = 1,
		.send = tls13_server_finished_send,
		.recv = tls13_server_finished_recv,
	},
	[APPLICATION_DATA] = {
		.handshake_complete = 1,
	},
};

enum tls13_message_type handshakes[][TLS13_NUM_MESSAGE_TYPES] = {
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
	[NEGOTIATED | WITH_HRR] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_HELLO_RETRY,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_CERTIFICATE_REQUEST,
		SERVER_CERTIFICATE,
		SERVER_CERTIFICATE_VERIFY,
		SERVER_FINISHED,
		CLIENT_CERTIFICATE,
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
	[NEGOTIATED | WITH_HRR | WITHOUT_CR] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_HELLO_RETRY,
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
	[NEGOTIATED | WITH_HRR | WITH_PSK] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_HELLO_RETRY,
		SERVER_ENCRYPTED_EXTENSIONS,
		SERVER_FINISHED,
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
	[NEGOTIATED | WITH_HRR | WITH_CCV] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		CLIENT_HELLO_RETRY,
		SERVER_HELLO_RETRY,
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
};

const size_t handshake_count = sizeof(handshakes) / sizeof(handshakes[0]);

enum tls13_message_type
tls13_handshake_active_state(struct tls13_ctx *ctx)
{
	struct tls13_handshake_stage hs = ctx->handshake_stage;

	if (hs.hs_type >= handshake_count)
		return INVALID;
	if (hs.message_number >= TLS13_NUM_MESSAGE_TYPES)
		return INVALID;

	return handshakes[hs.hs_type][hs.message_number];
}

struct tls13_handshake_action *
tls13_handshake_active_action(struct tls13_ctx *ctx)
{
	enum tls13_message_type mt = tls13_handshake_active_state(ctx);

	if (mt == INVALID)
		return NULL;

	return &state_machine[mt];
}

int
tls13_handshake_advance_state_machine(struct tls13_ctx *ctx)
{
	if (++ctx->handshake_stage.message_number >= TLS13_NUM_MESSAGE_TYPES)
		return 0;

	return 1;
}

int
tls13_handshake_perform(struct tls13_ctx *ctx)
{
	struct tls13_handshake_action *action;
	int ret;

	for (;;) {
		if ((action = tls13_handshake_active_action(ctx)) == NULL)
			return TLS13_IO_FAILURE;

		if (action->handshake_complete) {
			ctx->handshake_completed = 1;
			tls13_record_layer_handshake_completed(ctx->rl);
			return TLS13_IO_SUCCESS;
		}

		if (action->sender == ctx->mode) {
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
	ctx->mode = TLS13_HS_SERVER;

	return tls13_handshake_perform(ctx);
}

int
tls13_handshake_send_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action)
{
	ssize_t ret;
	CBS cbs;

	/* If we have no handshake message, we need to build one. */
	if (ctx->hs_msg == NULL) {
		if ((ctx->hs_msg = tls13_handshake_msg_new()) == NULL)
			return TLS13_IO_FAILURE;

		/* XXX - provide CBB. */
		if (!action->send(ctx))
			return TLS13_IO_FAILURE;
	}

	if ((ret = tls13_handshake_msg_send(ctx->hs_msg, ctx->rl)) <= 0)
		return ret;

	tls13_handshake_msg_data(ctx->hs_msg, &cbs);
	if (!tls1_transcript_record(ctx->ssl, CBS_data(&cbs), CBS_len(&cbs)))
		return TLS13_IO_FAILURE;

	tls13_handshake_msg_free(ctx->hs_msg);
	ctx->hs_msg = NULL;

	if (action->sent != NULL && !action->sent(ctx))
		return TLS13_IO_FAILURE;

	return TLS13_IO_SUCCESS;
}

int
tls13_handshake_recv_action(struct tls13_ctx *ctx,
    struct tls13_handshake_action *action)
{
	uint8_t msg_type;
	ssize_t ret;
	CBS cbs;

	if (ctx->hs_msg == NULL) {
		if ((ctx->hs_msg = tls13_handshake_msg_new()) == NULL)
			return TLS13_IO_FAILURE;
	}

	if ((ret = tls13_handshake_msg_recv(ctx->hs_msg, ctx->rl)) <= 0)
		return ret;

	if (action->preserve_transcript_hash) {
		if (!tls1_transcript_hash_value(ctx->ssl,
		    ctx->hs->transcript_hash, sizeof(ctx->hs->transcript_hash),
		    &ctx->hs->transcript_hash_len))
			return TLS13_IO_FAILURE;
	}

	tls13_handshake_msg_data(ctx->hs_msg, &cbs);
	if (!tls1_transcript_record(ctx->ssl, CBS_data(&cbs), CBS_len(&cbs)))
		return TLS13_IO_FAILURE;

	/*
	 * In TLSv1.3 there is no way to know if you're going to receive a
	 * certificate request message or not, hence we have to special case it
	 * here. The receive handler also knows how to deal with this situation.
	 */
	msg_type = tls13_handshake_msg_type(ctx->hs_msg);
	if (msg_type != action->handshake_type &&
	    (msg_type != TLS13_MT_CERTIFICATE ||
	     action->handshake_type != TLS13_MT_CERTIFICATE_REQUEST)) {
		/* XXX send unexpected message alert */
		return TLS13_IO_FAILURE;
	}

	/* XXX provide CBS and check all consumed. */
	ret = TLS13_IO_FAILURE;
	if (action->recv(ctx))
		ret = TLS13_IO_SUCCESS;

	tls13_handshake_msg_free(ctx->hs_msg);
	ctx->hs_msg = NULL;

	if (ctx->ssl->method->internal->version < TLS1_3_VERSION)
		return TLS13_IO_USE_LEGACY;

	return ret;
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
tls13_server_hello_retry_recv(struct tls13_ctx *ctx)
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
tls13_server_hello_send(struct tls13_ctx *ctx)
{
	ctx->handshake_stage.hs_type |= NEGOTIATED;

	return 0;
}

int
tls13_server_hello_retry_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_send(struct tls13_ctx *ctx)
{
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
tls13_server_finished_send(struct tls13_ctx *ctx)
{
	return 0;
}
