/* $OpenBSD: tls13_buffer.c,v 1.1 2019/01/17 06:32:12 jsing Exp $ */
/*
 * Copyright (c) 2018, 2019 Joel Sing <jsing@openbsd.org>
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

#include "ssl_locl.h"

#include "bytestring.h"
#include "tls13_internal.h"

struct tls13_buffer {
	size_t capacity;
	uint8_t *data;
	size_t len;
	size_t offset;
};

static int tls13_buffer_resize(struct tls13_buffer *buf, size_t capacity);

struct tls13_buffer *
tls13_buffer_new(size_t init_size)
{
	struct tls13_buffer *buf = NULL;

	if ((buf = calloc(1, sizeof(struct tls13_buffer))) == NULL)
		goto err;

	if (!tls13_buffer_resize(buf, init_size))
		goto err;

	return buf;

 err:
	tls13_buffer_free(buf);

	return NULL;
}

void
tls13_buffer_free(struct tls13_buffer *buf)
{
	if (buf == NULL)
		return;

	freezero(buf->data, buf->capacity);
	freezero(buf, sizeof(struct tls13_buffer));
}

static int
tls13_buffer_resize(struct tls13_buffer *buf, size_t capacity)
{
	uint8_t *data;

	if (buf->capacity == capacity)
		return 1;

	if ((data = recallocarray(buf->data, buf->capacity, capacity, 1)) == NULL)
		return 0;

	buf->data = data;
	buf->capacity = capacity;

	return 1;
}

ssize_t
tls13_buffer_extend(struct tls13_buffer *buf, size_t len,
    tls13_read_cb read_cb, void *cb_arg)
{
	ssize_t ret;

	if (len == buf->len)
		return buf->len;

	if (len < buf->len)
		return TLS13_IO_FAILURE;

	if (!tls13_buffer_resize(buf, len))
		return TLS13_IO_FAILURE;

	for (;;) {
		if ((ret = read_cb(&buf->data[buf->len],
		    buf->capacity - buf->len, cb_arg)) <= 0)
			return ret;

		buf->len += ret;

		if (buf->len == buf->capacity)
			return buf->len;
	}
}

void
tls13_buffer_cbs(struct tls13_buffer *buf, CBS *cbs)
{
	CBS_init(cbs, buf->data, buf->len);
}

int
tls13_buffer_finish(struct tls13_buffer *buf, uint8_t **out, size_t *out_len)
{
	if (out == NULL || out_len == NULL)
		return 0;

	*out = buf->data;
	*out_len = buf->len;

	buf->capacity = 0;
	buf->data = NULL;
	buf->len = 0;

	return 1;
}
