/*	$OpenBSD: tls12_handshake.c,v 1.2 2026/09/22 19:00:31 jsing Exp $	*/
/*
 * Copyright (c) 2018-2021 Theo Buehler <tb@openbsd.org>
 * Copyright (c) 2019, 2026 Joel Sing <jsing@openbsd.org>
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

#include "ssl_local.h"
#include "tls12_handshake.h"
#include "tls12_internal.h"

/* Based on RFC 5246 and inspired by s2n's TLS 1.2 state machine. */

struct tls12_handshake_action {
	uint8_t ccs;
	uint8_t	handshake_type;
	uint8_t	sender;
	uint8_t	handshake_complete;

	int (*send)(struct tls12_ctx *ctx, CBB *cbb);
	int (*sent)(struct tls12_ctx *ctx);
	int (*recv)(struct tls12_ctx *ctx, CBS *cbs);
};

static enum tls12_message_type
    tls12_handshake_active_state(struct tls12_ctx *ctx);

static const struct tls12_handshake_action *
    tls12_handshake_active_action(struct tls12_ctx *ctx);
static int tls12_handshake_advance_state_machine(struct tls12_ctx *ctx);

static int tls12_handshake_send_action(struct tls12_ctx *ctx,
    const struct tls12_handshake_action *action);
static int tls12_handshake_recv_action(struct tls12_ctx *ctx,
    const struct tls12_handshake_action *action);

static int tls12_handshake_send_ccs(struct tls12_ctx *ctx);
static int tls12_handshake_recv_ccs(struct tls12_ctx *ctx);

static int tls12_handshake_set_legacy_state(struct tls12_ctx *ctx);
static int tls12_handshake_legacy_info_callback(struct tls12_ctx *ctx);

static const struct tls12_handshake_action state_machine[] = {
	[CLIENT_HELLO] = {
		.handshake_type = TLS12_MT_CLIENT_HELLO,
		.sender = TLS12_HS_CLIENT,
		.send = tls12_client_hello_send,
		.recv = tls12_client_hello_recv,
	},
	[CLIENT_CERTIFICATE] = {
		.handshake_type = TLS12_MT_CERTIFICATE,
		.sender = TLS12_HS_CLIENT,
		.send = tls12_client_certificate_send,
		.recv = tls12_client_certificate_recv,
	},
	[CLIENT_CERTIFICATE_VERIFY] = {
		.handshake_type = TLS12_MT_CERTIFICATE_VERIFY,
		.sender = TLS12_HS_CLIENT,
		.send = tls12_client_certificate_verify_send,
		.recv = tls12_client_certificate_verify_recv,
	},
	[CLIENT_KEY_EXCHANGE] = {
		.handshake_type = TLS12_MT_CLIENT_KEY_EXCHANGE,
		.sender = TLS12_HS_CLIENT,
		.send = tls12_client_key_exchange_send,
		.recv = tls12_client_key_exchange_recv,
	},
	[CLIENT_CHANGE_CIPHER_SPEC] = {
		.ccs = 1,
		.sender = TLS12_HS_CLIENT,
	},
	[CLIENT_FINISHED] = {
		.handshake_type = TLS12_MT_FINISHED,
		.sender = TLS12_HS_CLIENT,
		.send = tls12_client_finished_send,
		.recv = tls12_client_finished_recv,
	},
	[SERVER_HELLO] = {
		.handshake_type = TLS12_MT_SERVER_HELLO,
		.sender = TLS12_HS_SERVER,
		.send = tls12_server_hello_send,
		.recv = tls12_server_hello_recv,
	},
	[SERVER_CERTIFICATE] = {
		.handshake_type = TLS12_MT_CERTIFICATE,
		.sender = TLS12_HS_SERVER,
		.send = tls12_server_certificate_send,
		.recv = tls12_server_certificate_recv,
	},
	[SERVER_KEY_EXCHANGE] = {
		.handshake_type = TLS12_MT_SERVER_KEY_EXCHANGE,
		.sender = TLS12_HS_SERVER,
		.send = tls12_server_key_exchange_send,
		.recv = tls12_server_key_exchange_recv,
	},
	[SERVER_CERTIFICATE_REQUEST] = {
		.handshake_type = TLS12_MT_CERTIFICATE_REQUEST,
		.sender = TLS12_HS_SERVER,
		.send = tls12_server_certificate_request_send,
		.recv = tls12_server_certificate_request_recv,
	},
	[SERVER_HELLO_DONE] = {
		.handshake_type = TLS12_MT_SERVER_HELLO_DONE,
		.sender = TLS12_HS_SERVER,
		.send = tls12_server_hello_done_send,
		.recv = tls12_server_hello_done_recv,
	},
	[SERVER_NEW_SESSION_TICKET] = {
		.handshake_type = TLS12_MT_NEW_SESSION_TICKET,
		.sender = TLS12_HS_SERVER,
		.send = tls12_server_new_session_ticket_send,
		.recv = tls12_server_new_session_ticket_recv,
	},
	[SERVER_CHANGE_CIPHER_SPEC] = {
		.ccs = 1,
		.sender = TLS12_HS_SERVER,
	},
	[SERVER_FINISHED] = {
		.handshake_type = TLS12_MT_FINISHED,
		.sender = TLS12_HS_SERVER,
		.send = tls12_server_finished_send,
		.recv = tls12_server_finished_recv,
	},
	[APPLICATION_DATA] = {
		.handshake_complete = 1,
	},
};

const enum tls12_message_type tls12_handshakes[][TLS12_NUM_MESSAGE_TYPES] = {
	[INITIAL] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_HELLO_DONE,
		CLIENT_KEY_EXCHANGE,
		CLIENT_CHANGE_CIPHER_SPEC,
		CLIENT_FINISHED,
		SERVER_CHANGE_CIPHER_SPEC,
		SERVER_FINISHED,
		APPLICATION_DATA,
	},
	[INITIAL | WITH_SERVER_CERT] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_CERTIFICATE,
		SERVER_HELLO_DONE,
		CLIENT_KEY_EXCHANGE,
		CLIENT_CHANGE_CIPHER_SPEC,
		CLIENT_FINISHED,
		SERVER_CHANGE_CIPHER_SPEC,
		SERVER_FINISHED,
		APPLICATION_DATA,
	},
	[INITIAL | WITH_SERVER_CERT | WITH_SERVER_KEX] = {
		CLIENT_HELLO,
		SERVER_HELLO,
		SERVER_CERTIFICATE,
		SERVER_KEY_EXCHANGE,
		SERVER_HELLO_DONE,
		CLIENT_KEY_EXCHANGE,
		CLIENT_CHANGE_CIPHER_SPEC,
		CLIENT_FINISHED,
		SERVER_CHANGE_CIPHER_SPEC,
		SERVER_FINISHED,
		APPLICATION_DATA,
	},
};

const size_t tls12_handshake_count =
    sizeof(tls12_handshakes) / sizeof(tls12_handshakes[0]);

#ifndef TLS12_DEBUG
#define DEBUGF(...)
#else
#define DEBUGF(...) fprintf(stderr, __VA_ARGS__)

static const char *
tls12_handshake_mode_name(uint8_t mode)
{
	switch (mode) {
	case TLS12_HS_CLIENT:
		return "Client";
	case TLS12_HS_SERVER:
		return "Server";
	}
	return "Unknown";
}

static const char *
tls12_handshake_message_name(uint8_t msg_type)
{
	switch (msg_type) {
	case TLS12_MT_HELLO_REQUEST:
		return "HelloRequest";
	case TLS12_MT_CLIENT_HELLO:
		return "ClientHello";
	case TLS12_MT_SERVER_HELLO:
		return "ServerHello";
	case TLS12_MT_CERTIFICATE:
		return "Certificate";
	case TLS12_MT_SERVER_KEY_EXCHANGE:
		return "ServerKeyExchange";
	case TLS12_MT_CERTIFICATE_REQUEST:
		return "CertificateRequest";
	case TLS12_MT_SERVER_HELLO_DONE:
		return "ServerHelloDone";
	case TLS12_MT_CERTIFICATE_VERIFY:
		return "CertificateVerify";
	case TLS12_MT_CLIENT_KEY_EXCHANGE:
		return "ClientKeyExchange";
	case TLS12_MT_NEW_SESSION_TICKET:
		return "NewSessionTicket";
	case TLS12_MT_FINISHED:
		return "Finished";
	}
	return "Unknown";
}
#endif

static enum tls12_message_type
tls12_handshake_active_state(struct tls12_ctx *ctx)
{
	struct tls12_handshake_stage hs = ctx->handshake_stage;

	if (hs.hs_type >= tls12_handshake_count)
		return INVALID;
	if (hs.message_number >= TLS12_NUM_MESSAGE_TYPES)
		return INVALID;

	return tls12_handshakes[hs.hs_type][hs.message_number];
}

static const struct tls12_handshake_action *
tls12_handshake_active_action(struct tls12_ctx *ctx)
{
	enum tls12_message_type mt = tls12_handshake_active_state(ctx);

	if (mt == INVALID)
		return NULL;

	return &state_machine[mt];
}

static int
tls12_handshake_advance_state_machine(struct tls12_ctx *ctx)
{
	if (++ctx->handshake_stage.message_number >= TLS12_NUM_MESSAGE_TYPES)
		return 0;

	return 1;
}

static int
tls12_handshake_end_of_flight(struct tls12_ctx *ctx,
    const struct tls12_handshake_action *previous)
{
	const struct tls12_handshake_action *current;

	if ((current = tls12_handshake_active_action(ctx)) == NULL)
		return 1;

	return current->sender != previous->sender;
}

int
tls12_handshake_msg_record(struct tls12_ctx *ctx)
{
	CBS cbs;

	tls12_handshake_msg_data(ctx->hs_msg, &cbs);
	return tls1_transcript_record(ctx->ssl, CBS_data(&cbs), CBS_len(&cbs));
}

int
tls12_handshake_perform(struct tls12_ctx *ctx)
{
	const struct tls12_handshake_action *action;
	int sending;
	int ret;

	if (!ctx->handshake_started) {
		/*
		 * Set legacy state to connect/accept and call info callback
		 * to signal that the handshake started.
		 */
		if (!tls12_handshake_set_legacy_state(ctx))
			return TLS12_IO_FAILURE;
		if (!tls12_handshake_legacy_info_callback(ctx))
			return TLS12_IO_FAILURE;

		ctx->handshake_started = 1;

		/* Set legacy state for initial ClientHello read or write. */
		if (!tls12_handshake_set_legacy_state(ctx))
			return TLS12_IO_FAILURE;
	}

	for (;;) {
		if ((action = tls12_handshake_active_action(ctx)) == NULL)
			return TLS12_IO_FAILURE;

		if (ctx->need_flush) {
			/* XXX - record layer flush */
			ctx->need_flush = 0;
		}

		if (action->handshake_complete) {
			ctx->handshake_completed = 1;
			/* XXX - record layer handshake completed */

			if (!tls12_handshake_set_legacy_state(ctx))
				return TLS12_IO_FAILURE;
			if (!tls12_handshake_legacy_info_callback(ctx))
				return TLS12_IO_FAILURE;

			return TLS12_IO_SUCCESS;
		}

		sending = action->sender == ctx->mode;

		DEBUGF("%s %s %s\n", tls12_handshake_mode_name(ctx->mode),
		    sending ? "sending" : "receiving",
		    tls12_handshake_message_name(action->handshake_type));

		if (ctx->alert != 0)
			return tls12_send_alert(ctx->rl, ctx->alert);

		if (action->ccs) {
			if (sending)
				ret = tls12_handshake_send_ccs(ctx);
			else
				ret = tls12_handshake_recv_ccs(ctx);
		} else {
			if (sending)
				ret = tls12_handshake_send_action(ctx, action);
			else
				ret = tls12_handshake_recv_action(ctx, action);
		}

		if (ctx->alert != 0)
			return tls12_send_alert(ctx->rl, ctx->alert);

		if (ret <= 0) {
			DEBUGF("%s %s returned %d\n",
			    tls12_handshake_mode_name(ctx->mode),
			    (action->sender == ctx->mode) ? "send" : "recv",
			    ret);
			return ret;
		}

		if (!tls12_handshake_legacy_info_callback(ctx))
			return TLS12_IO_FAILURE;

		if (!tls12_handshake_advance_state_machine(ctx))
			return TLS12_IO_FAILURE;

		if (sending)
			ctx->need_flush = tls12_handshake_end_of_flight(ctx,
			    action);

		if (!tls12_handshake_set_legacy_state(ctx))
			return TLS12_IO_FAILURE;
	}
}

static int
tls12_handshake_send_action(struct tls12_ctx *ctx,
    const struct tls12_handshake_action *action)
{
	ssize_t ret;
	CBB cbb;

	/* If we have no handshake message, we need to build one. */
	if (ctx->hs_msg == NULL) {
		if ((ctx->hs_msg = tls12_handshake_msg_new()) == NULL)
			return TLS12_IO_FAILURE;
		if (!tls12_handshake_msg_start(ctx->hs_msg, &cbb,
		    action->handshake_type))
			return TLS12_IO_FAILURE;
		if (!action->send(ctx, &cbb))
			return TLS12_IO_FAILURE;
		if (!tls12_handshake_msg_finish(ctx->hs_msg))
			return TLS12_IO_FAILURE;
	}

	if ((ret = tls12_handshake_msg_send(ctx->hs_msg, ctx->rl)) <= 0)
		return ret;

	if (!tls12_handshake_msg_record(ctx))
		return TLS12_IO_FAILURE;

	if (ctx->handshake_message_sent_cb != NULL)
		ctx->handshake_message_sent_cb(ctx);

	tls12_handshake_msg_free(ctx->hs_msg);
	ctx->hs_msg = NULL;

	if (action->sent != NULL && !action->sent(ctx))
		return TLS12_IO_FAILURE;

	return TLS12_IO_SUCCESS;
}

static int
tls12_handshake_recv_action(struct tls12_ctx *ctx,
    const struct tls12_handshake_action *action)
{
	ssize_t ret;
	CBS cbs;

	if (ctx->hs_msg == NULL) {
		if ((ctx->hs_msg = tls12_handshake_msg_new()) == NULL)
			return TLS12_IO_FAILURE;
	}

	if ((ret = tls12_handshake_msg_recv(ctx->hs_msg, ctx->rl)) <= 0)
		return ret;

	if (!tls12_handshake_msg_record(ctx))
		return TLS12_IO_FAILURE;

	if (ctx->handshake_message_recv_cb != NULL)
		ctx->handshake_message_recv_cb(ctx);

	if (!tls12_handshake_msg_content(ctx->hs_msg, &cbs))
		return TLS12_IO_FAILURE;

	ret = TLS12_IO_FAILURE;
	if (!action->recv(ctx, &cbs))
		goto err;

	if (CBS_len(&cbs) != 0) {
		/* TLS12_ERR_TRAILING_DATA */
		ctx->alert = TLS12_ALERT_DECODE_ERROR;
		goto err;
	}

	ret = TLS12_IO_SUCCESS;

 err:
	tls12_handshake_msg_free(ctx->hs_msg);
	ctx->hs_msg = NULL;

	return ret;
}

static int
tls12_handshake_send_ccs(struct tls12_ctx *ctx)
{
	return TLS12_IO_FAILURE;
}

static int
tls12_handshake_recv_ccs(struct tls12_ctx *ctx)
{
	return TLS12_IO_FAILURE;
}

struct tls12_handshake_legacy_state {
	int recv;
	int send;
};

static const struct tls12_handshake_legacy_state legacy_states[] = {
	[CLIENT_HELLO] = {
		.recv = SSL3_ST_SR_CLNT_HELLO_A,
		.send = SSL3_ST_CW_CLNT_HELLO_A,
	},
	[SERVER_HELLO] = {
		.recv = SSL3_ST_CR_SRVR_HELLO_A,
		.send = SSL3_ST_SW_SRVR_HELLO_A,
	},
	[SERVER_CERTIFICATE_REQUEST] = {
		.recv = SSL3_ST_CR_CERT_REQ_A,
		.send = SSL3_ST_SW_CERT_REQ_A,
	},
	[SERVER_CERTIFICATE] = {
		.recv = SSL3_ST_CR_CERT_A,
		.send = SSL3_ST_SW_CERT_A,
	},
	[SERVER_FINISHED] = {
		.recv = SSL3_ST_CR_FINISHED_A,
		.send = SSL3_ST_SW_FINISHED_A,
	},
	[CLIENT_CERTIFICATE] = {
		.recv = SSL3_ST_SR_CERT_VRFY_A,
		.send = SSL3_ST_CW_CERT_VRFY_B,
	},
	[CLIENT_CERTIFICATE_VERIFY] = {
		.send = 0,
		.recv = 0,
	},
	[CLIENT_FINISHED] = {
		.recv = SSL3_ST_SR_FINISHED_A,
		.send = SSL3_ST_CW_FINISHED_A,
	},
	[APPLICATION_DATA] = {
		.recv = 0,
		.send = 0,
	},
};

CTASSERT(sizeof(state_machine) / sizeof(state_machine[0]) ==
    sizeof(legacy_states) / sizeof(legacy_states[0]));

static int
tls12_handshake_legacy_state(struct tls12_ctx *ctx, int *out_state)
{
	const struct tls12_handshake_action *action;
	enum tls12_message_type mt;

	*out_state = 0;

	if (!ctx->handshake_started) {
		if (ctx->mode == TLS12_HS_CLIENT)
			*out_state = SSL_ST_CONNECT;
		else
			*out_state = SSL_ST_ACCEPT;

		return 1;
	}

	if (ctx->handshake_completed) {
		*out_state = SSL_ST_OK;
		return 1;
	}

	if ((mt = tls12_handshake_active_state(ctx)) == INVALID)
		return 0;

	if ((action = tls12_handshake_active_action(ctx)) == NULL)
		return 0;

	if (action->sender == ctx->mode)
		*out_state = legacy_states[mt].send;
	else
		*out_state = legacy_states[mt].recv;

	return 1;
}

static int
tls12_handshake_info_position(struct tls12_ctx *ctx)
{
	if (!ctx->handshake_started)
		return TLS12_INFO_HANDSHAKE_STARTED;

	if (ctx->handshake_completed)
		return TLS12_INFO_HANDSHAKE_COMPLETED;

	if (ctx->mode == TLS12_HS_CLIENT)
		return TLS12_INFO_CONNECT_LOOP;
	else
		return TLS12_INFO_ACCEPT_LOOP;
}

static int
tls12_handshake_legacy_info_callback(struct tls12_ctx *ctx)
{
	int state, where;

	if (!tls12_handshake_legacy_state(ctx, &state))
		return 0;

	/* Do nothing if there's no corresponding legacy state. */
	if (state == 0)
		return 1;

	if (ctx->info_cb != NULL) {
		where = tls12_handshake_info_position(ctx);
		ctx->info_cb(ctx, where, 1);
	}

	return 1;
}

static int
tls12_handshake_set_legacy_state(struct tls12_ctx *ctx)
{
	int state;

	if (!tls12_handshake_legacy_state(ctx, &state))
		return 0;

	/* Do nothing if there's no corresponding legacy state. */
	if (state == 0)
		return 1;

	ctx->hs->state = state;

	return 1;
}
