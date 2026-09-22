/*	$OpenBSD: tls12_legacy.c,v 1.2 2026/09/22 00:38:51 jsing Exp $ */
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

#include <limits.h>

#include "ssl_local.h"
#include "tls12_internal.h"

static ssize_t
tls12_legacy_wire_read(SSL *ssl, uint8_t *buf, size_t len)
{
	int n;

	if (ssl->rbio == NULL) {
		SSLerror(ssl, SSL_R_BIO_NOT_SET);
		return TLS12_IO_FAILURE;
	}

	ssl->rwstate = SSL_READING;
	errno = 0;

	if ((n = BIO_read(ssl->rbio, buf, len)) <= 0) {
		if (BIO_should_read(ssl->rbio))
			return TLS12_IO_WANT_POLLIN;
		if (n == 0)
			return TLS12_IO_EOF;

		if (ERR_peek_error() == 0 && errno != 0)
			SYSerror(errno);

		return TLS12_IO_FAILURE;
	}

	if (n == len)
		ssl->rwstate = SSL_NOTHING;

	return n;
}

ssize_t
tls12_legacy_wire_read_cb(void *buf, size_t n, void *arg)
{
	SSL *ssl = arg;

	return tls12_legacy_wire_read(ssl, buf, n);
}

static ssize_t
tls12_legacy_wire_write(SSL *ssl, const uint8_t *buf, size_t len)
{
	int n;

	if (ssl->wbio == NULL) {
		SSLerror(ssl, SSL_R_BIO_NOT_SET);
		return TLS12_IO_FAILURE;
	}

	ssl->rwstate = SSL_WRITING;
	errno = 0;

	if ((n = BIO_write(ssl->wbio, buf, len)) <= 0) {
		if (BIO_should_write(ssl->wbio))
			return TLS12_IO_WANT_POLLOUT;

		if (ERR_peek_error() == 0 && errno != 0)
			SYSerror(errno);

		return TLS12_IO_FAILURE;
	}

	if (n == len)
		ssl->rwstate = SSL_NOTHING;

	return n;
}

ssize_t
tls12_legacy_wire_write_cb(const void *buf, size_t n, void *arg)
{
	SSL *ssl = arg;

	return tls12_legacy_wire_write(ssl, buf, n);
}
