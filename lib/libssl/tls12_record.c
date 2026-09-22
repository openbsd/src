/* $OpenBSD: tls12_record.c,v 1.2 2026/09/22 00:38:51 jsing Exp $ */
/*
 * Copyright (c) 2018, 2019, 2026 Joel Sing <jsing@openbsd.org>
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

#include <stdlib.h>

#include <openssl/ssl3.h>

#include "tls_internal.h"
#include "tls12_internal.h"
#include "tls12_record.h"

struct tls12_record {
	uint16_t version;
	uint8_t content_type;
	size_t rec_len;
	uint8_t *data;
	size_t data_len;
	CBS cbs;

	struct tls_buffer *buf;
};

struct tls12_record *
tls12_record_new(void)
{
	return calloc(1, sizeof(struct tls12_record));
}

void
tls12_record_free(struct tls12_record *rec)
{
	if (rec == NULL)
		return;

	tls_buffer_free(rec->buf);

	freezero(rec->data, rec->data_len);
	freezero(rec, sizeof(struct tls12_record));
}

uint16_t
tls12_record_version(struct tls12_record *rec)
{
	return rec->version;
}

void
tls12_record_data(struct tls12_record *rec, CBS *cbs)
{
	CBS_init(cbs, rec->data, rec->data_len);
}

int
tls12_record_set_data(struct tls12_record *rec, uint8_t *data, size_t data_len)
{
	if (data_len > TLS12_RECORD_MAX_LEN)
		return 0;

	freezero(rec->data, rec->data_len);
	rec->data = data;
	rec->data_len = data_len;
	CBS_init(&rec->cbs, rec->data, rec->data_len);

	return 1;
}

ssize_t
tls12_record_recv(struct tls12_record *rec, tls_read_cb wire_read,
    void *wire_arg)
{
	uint16_t rec_len, rec_version;
	uint8_t content_type;
	ssize_t ret;
	CBS cbs;

	if (rec->data != NULL)
		return TLS12_IO_FAILURE;

	if (rec->buf == NULL)
		rec->buf = tls_buffer_new(TLS12_RECORD_HEADER_LEN);
	if (rec->buf == NULL)
		return TLS12_IO_FAILURE;

	if (rec->content_type == 0) {
		if ((ret = tls_buffer_extend(rec->buf,
		    TLS12_RECORD_HEADER_LEN, wire_read, wire_arg)) <= 0)
			return ret;

		if (!tls_buffer_data(rec->buf, &cbs))
			return TLS12_IO_FAILURE;

		if (!CBS_get_u8(&cbs, &content_type))
			return TLS12_IO_FAILURE;
		if (!CBS_get_u16(&cbs, &rec_version))
			return TLS12_IO_FAILURE;
		if (!CBS_get_u16(&cbs, &rec_len))
			return TLS12_IO_FAILURE;

		if ((rec_version >> 8) != SSL3_VERSION_MAJOR)
			return TLS12_IO_RECORD_VERSION;
		if (rec_len > TLS12_RECORD_MAX_CIPHERTEXT_LEN)
			return TLS12_IO_RECORD_OVERFLOW;

		rec->content_type = content_type;
		rec->version = rec_version;
		rec->rec_len = rec_len;
	}

	if ((ret = tls_buffer_extend(rec->buf,
	    TLS12_RECORD_HEADER_LEN + rec->rec_len, wire_read, wire_arg)) <= 0)
		return ret;

	if (!tls_buffer_finish(rec->buf, &rec->data, &rec->data_len))
		return TLS12_IO_FAILURE;

	return rec->data_len;
}

ssize_t
tls12_record_send(struct tls12_record *rec, tls_write_cb wire_write,
    void *wire_arg)
{
	ssize_t ret;

	if (rec->data == NULL)
		return TLS12_IO_FAILURE;

	while (CBS_len(&rec->cbs) > 0) {
		if ((ret = wire_write(CBS_data(&rec->cbs),
		    CBS_len(&rec->cbs), wire_arg)) <= 0)
			return ret;

		if (!CBS_skip(&rec->cbs, ret))
			return TLS12_IO_FAILURE;
	}

	return rec->data_len;
}
