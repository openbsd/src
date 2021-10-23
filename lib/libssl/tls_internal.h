/* $OpenBSD: tls_internal.h,v 1.1 2021/10/23 13:12:14 jsing Exp $ */
/*
 * Copyright (c) 2018, 2019, 2021 Joel Sing <jsing@openbsd.org>
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

#ifndef HEADER_TLS_INTERNAL_H
#define HEADER_TLS_INTERNAL_H

#include "bytestring.h"

__BEGIN_HIDDEN_DECLS

#define TLS_IO_SUCCESS			 1
#define TLS_IO_EOF			 0
#define TLS_IO_FAILURE			-1
#define TLS_IO_ALERT			-2
#define TLS_IO_WANT_POLLIN		-3
#define TLS_IO_WANT_POLLOUT		-4
#define TLS_IO_WANT_RETRY		-5 /* Retry the previous call immediately. */

/*
 * Callbacks.
 */
typedef ssize_t (*tls_read_cb)(void *_buf, size_t _buflen, void *_cb_arg);
typedef ssize_t (*tls_write_cb)(const void *_buf, size_t _buflen,
    void *_cb_arg);
typedef ssize_t (*tls_flush_cb)(void *_cb_arg);

/*
 * Buffers.
 */
struct tls_buffer;

struct tls_buffer *tls_buffer_new(size_t init_size);
int tls_buffer_set_data(struct tls_buffer *buf, CBS *data);
void tls_buffer_free(struct tls_buffer *buf);
ssize_t tls_buffer_extend(struct tls_buffer *buf, size_t len,
    tls_read_cb read_cb, void *cb_arg);
void tls_buffer_cbs(struct tls_buffer *buf, CBS *cbs);
int tls_buffer_finish(struct tls_buffer *buf, uint8_t **out, size_t *out_len);

__END_HIDDEN_DECLS

#endif
