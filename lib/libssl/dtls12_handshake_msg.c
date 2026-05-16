/* $OpenBSD: dtls12_handshake_msg.c,v 1.1 2026/05/16 08:20:41 jsing Exp $ */
/*
 * Copyright (c) 2026 Joel Sing <jsing@openbsd.org>
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bytestring.h"
#include "dtls12_internal.h"

#define DTLS12_HANDSHAKE_MSG_HEADER_LEN			12
#define DTLS12_HANDSHAKE_MSG_INITIAL_LEN		256

#define DTLS12_HANDSHAKE_MSG_FRAGMENT_LENGTH_OFFSET	9

struct dtls12_handshake_msg {
	uint8_t msg_type;
	uint32_t msg_len;
	uint16_t msg_seq;

	uint32_t fragment_offset;
	uint32_t fragment_len;
	uint8_t *fragment_data;
	size_t fragment_data_len;
	int fragment_pending;

	uint8_t *data;
	size_t data_len;

	CBS cbs;
	CBB cbb;
};

struct dtls12_handshake_msg *
dtls12_handshake_msg_new(void)
{
	struct dtls12_handshake_msg *msg = NULL;

	if ((msg = calloc(1, sizeof(struct dtls12_handshake_msg))) == NULL)
		return NULL;

	return msg;
}

void
dtls12_handshake_msg_free(struct dtls12_handshake_msg *msg)
{
	if (msg == NULL)
		return;

	CBB_cleanup(&msg->cbb);

	freezero(msg->data, msg->data_len);
	freezero(msg->fragment_data, msg->fragment_data_len);

	freezero(msg, sizeof(struct dtls12_handshake_msg));
}

void
dtls12_handshake_msg_data(struct dtls12_handshake_msg *msg, CBS *cbs)
{
	CBS_init(cbs, msg->data, msg->data_len);
}

uint8_t
dtls12_handshake_msg_type(struct dtls12_handshake_msg *msg)
{
	return msg->msg_type;
}

int
dtls12_handshake_msg_content(struct dtls12_handshake_msg *msg, CBS *cbs)
{
	dtls12_handshake_msg_data(msg, cbs);

	return CBS_skip(cbs, DTLS12_HANDSHAKE_MSG_HEADER_LEN);
}

int
dtls12_handshake_msg_start(struct dtls12_handshake_msg *msg, CBB *body,
    uint8_t msg_type, size_t msg_seq)
{
	msg->msg_type = msg_type;
	msg->msg_seq = msg_seq;

	msg->msg_len = 0;
	msg->fragment_offset = 0;

	if (!CBB_init(&msg->cbb, DTLS12_HANDSHAKE_MSG_INITIAL_LEN))
		return 0;
	if (!CBB_add_u8(&msg->cbb, msg->msg_type))
		return 0;
	if (!CBB_add_u24(&msg->cbb, msg->msg_len))
		return 0;
	if (!CBB_add_u16(&msg->cbb, msg->msg_seq))
		return 0;
	if (!CBB_add_u24(&msg->cbb, msg->fragment_offset))
		return 0;
	if (!CBB_add_u24_length_prefixed(&msg->cbb, body))
		return 0;

	return 1;
}

int
dtls12_handshake_msg_finish(struct dtls12_handshake_msg *msg)
{
	CBS cbs;

	if (!CBB_finish(&msg->cbb, &msg->data, &msg->data_len))
		return 0;

	/* Update message length to match fragment length. */
	CBS_init(&cbs, msg->data, msg->data_len);
	if (!CBS_skip(&cbs, DTLS12_HANDSHAKE_MSG_FRAGMENT_LENGTH_OFFSET))
		return 0;
	if (!CBS_get_u24(&cbs, &msg->msg_len))
		return 0;

	if (!CBB_init_fixed(&msg->cbb, msg->data, msg->data_len))
		return 0;
	if (!CBB_add_u8(&msg->cbb, msg->msg_type))
		return 0;
	if (!CBB_add_u24(&msg->cbb, msg->msg_len))
		return 0;
	if (!CBB_finish(&msg->cbb, NULL, NULL))
		return 0;

	dtls12_handshake_msg_fragment_reset(msg);

	return 1;
}

int
dtls12_handshake_msg_fragment_reset(struct dtls12_handshake_msg *msg)
{
	freezero(msg->fragment_data, msg->fragment_data_len);
	msg->fragment_data = NULL;
	msg->fragment_data_len = 0;

	msg->fragment_offset = 0;
	msg->fragment_pending = 1;

	return dtls12_handshake_msg_content(msg, &msg->cbs);
}

int
dtls12_handshake_msg_fragment_build(struct dtls12_handshake_msg *msg,
    size_t max_fragment_len, CBS *cbs)
{
	CBB body;

	CBS_init(cbs, NULL, 0);

	if (msg->fragment_offset > msg->msg_len)
		return 0;
	if (msg->msg_len - msg->fragment_offset > CBS_len(&msg->cbs))
		return 0;

	freezero(msg->fragment_data, msg->fragment_data_len);
	msg->fragment_data = NULL;
	msg->fragment_data_len = 0;

	if ((msg->fragment_len = CBS_len(&msg->cbs)) > max_fragment_len)
		msg->fragment_len = max_fragment_len;

	/* Build the fragment. */
	if (!CBB_init(&msg->cbb, DTLS12_HANDSHAKE_MSG_INITIAL_LEN))
		goto err;
	if (!CBB_add_u8(&msg->cbb, msg->msg_type))
		goto err;
	if (!CBB_add_u24(&msg->cbb, msg->msg_len))
		goto err;
	if (!CBB_add_u16(&msg->cbb, msg->msg_seq))
		goto err;
	if (!CBB_add_u24(&msg->cbb, msg->fragment_offset))
		goto err;
	if (!CBB_add_u24_length_prefixed(&msg->cbb, &body))
		goto err;
	if (!CBB_add_bytes(&body, CBS_data(&msg->cbs), msg->fragment_len))
		goto err;
	if (!CBB_finish(&msg->cbb, &msg->fragment_data, &msg->fragment_data_len))
		goto err;

	CBS_init(cbs, msg->fragment_data, msg->fragment_data_len);

	return 1;

 err:
	CBB_cleanup(&msg->cbb);

	return 0;
}

int
dtls12_handshake_msg_fragment_next(struct dtls12_handshake_msg *msg)
{
	if (msg->fragment_offset > msg->msg_len)
		return 0;
	if (msg->msg_len - msg->fragment_offset < msg->fragment_len)
		return 0;

	if (!CBS_skip(&msg->cbs, msg->fragment_len))
		return 0;

	msg->fragment_offset += msg->fragment_len;

	msg->fragment_pending = (CBS_len(&msg->cbs) > 0);

	return 1;
}

int
dtls12_handshake_msg_fragment_pending(struct dtls12_handshake_msg *msg)
{
	return msg->fragment_pending;
}
