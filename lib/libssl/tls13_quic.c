/*	$OpenBSD: tls13_quic.c,v 1.2 2022/07/24 14:31:37 jsing Exp $ */
/*
 * Copyright (c) 2022 Joel Sing <jsing@openbsd.org>
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

#include <ssl_locl.h>

#include "tls13_internal.h"

static ssize_t
tls13_quic_wire_read_cb(void *buf, size_t n, void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *ssl = ctx->ssl;

	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return TLS13_IO_FAILURE;
}

static ssize_t
tls13_quic_wire_write_cb(const void *buf, size_t n, void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *ssl = ctx->ssl;

	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return TLS13_IO_FAILURE;
}

static ssize_t
tls13_quic_wire_flush_cb(void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *ssl = ctx->ssl;

	/* XXX - call flush_flight. */
	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return TLS13_IO_FAILURE;
}

static ssize_t
tls13_quic_handshake_read_cb(void *buf, size_t n, void *arg)
{
	/* XXX - read handshake data. */
	return TLS13_IO_FAILURE;
}

static ssize_t
tls13_quic_handshake_write_cb(const void *buf, size_t n, void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *ssl = ctx->ssl;

	/* XXX - call add_handshake_data. */
	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return TLS13_IO_FAILURE;
}

static int
tls13_quic_set_read_traffic_key(struct tls13_secret *read_key,
    enum ssl_encryption_level_t read_level, void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *ssl = ctx->ssl;

	ctx->hs->tls13.quic_read_level = read_level;

	/* XXX - call set_read_secret. */
	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return 0;
}

static int
tls13_quic_set_write_traffic_key(struct tls13_secret *write_key,
    enum ssl_encryption_level_t write_level, void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *ssl = ctx->ssl;

	ctx->hs->tls13.quic_write_level = write_level;

	/* XXX - call set_write_secret. */
	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return 0;
}

static int
tls13_quic_alert_send_cb(int alert_desc, void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *ssl = ctx->ssl;

	/* XXX - call send_alert. */
	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return TLS13_IO_FAILURE;
}

static const struct tls13_record_layer_callbacks quic_rl_callbacks = {
	.wire_read = tls13_quic_wire_read_cb,
	.wire_write = tls13_quic_wire_write_cb,
	.wire_flush = tls13_quic_wire_flush_cb,

	.handshake_read = tls13_quic_handshake_read_cb,
	.handshake_write = tls13_quic_handshake_write_cb,
	.set_read_traffic_key = tls13_quic_set_read_traffic_key,
	.set_write_traffic_key = tls13_quic_set_write_traffic_key,
	.alert_send = tls13_quic_alert_send_cb,

	.alert_recv = tls13_alert_received_cb,
	.alert_sent = tls13_alert_sent_cb,
	.phh_recv = tls13_phh_received_cb,
	.phh_sent = tls13_phh_done_cb,
};

int
tls13_quic_init(struct tls13_ctx *ctx)
{
	BIO *bio;

	tls13_record_layer_set_callbacks(ctx->rl, &quic_rl_callbacks, ctx);

	ctx->middlebox_compat = 0;

	/*
	 * QUIC does not use BIOs, however we currently expect a BIO to exist
	 * for status handling.
	 */
	if ((bio = BIO_new(BIO_s_null())) == NULL)
		return 0;

	BIO_up_ref(bio);
	SSL_set_bio(ctx->ssl, bio, bio);
	bio = NULL;

	return 1;
}
