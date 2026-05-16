/* $OpenBSD: dtls12_internal.h,v 1.1 2026/05/16 08:20:41 jsing Exp $ */
/*
 * Copyright (c) 2026 Joel Sing <jsing@openbsd.org>
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

#ifndef HEADER_DTLS12_INTERNAL_H
#define HEADER_DTLS12_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "bytestring.h"

__BEGIN_HIDDEN_DECLS

struct dtls12_handshake_msg;

struct dtls12_handshake_msg *dtls12_handshake_msg_new(void);
void dtls12_handshake_msg_free(struct dtls12_handshake_msg *msg);
void dtls12_handshake_msg_data(struct dtls12_handshake_msg *msg, CBS *cbs);
uint8_t dtls12_handshake_msg_type(struct dtls12_handshake_msg *msg);
int dtls12_handshake_msg_content(struct dtls12_handshake_msg *msg, CBS *cbs);
int dtls12_handshake_msg_start(struct dtls12_handshake_msg *msg, CBB *body,
    uint8_t msg_type, size_t msg_seq);
int dtls12_handshake_msg_finish(struct dtls12_handshake_msg *msg);
int dtls12_handshake_msg_fragment_build(struct dtls12_handshake_msg *msg,
    size_t max_fragment_len, CBS *cbs);
int dtls12_handshake_msg_fragment_next(struct dtls12_handshake_msg *msg);
int dtls12_handshake_msg_fragment_pending(struct dtls12_handshake_msg *msg);
int dtls12_handshake_msg_fragment_reset(struct dtls12_handshake_msg *msg);

__END_HIDDEN_DECLS

#endif
