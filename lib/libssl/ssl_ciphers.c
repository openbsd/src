/*	$OpenBSD: ssl_ciphers.c,v 1.1 2019/01/21 10:28:52 tb Exp $ */
/*
 * Copyright (c) 2019 Theo Buehler <tb@openbsd.org>
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

int
ssl_cipher_is_permitted(const SSL_CIPHER *cipher, uint16_t min_ver,
    uint16_t max_ver)
{
	/* XXX: We only support DTLSv1 which is effectively TLSv1.1 */
	if (min_ver == DTLS1_VERSION || max_ver == DTLS1_VERSION)
		min_ver = max_ver = TLS1_1_VERSION;

	switch(cipher->algorithm_ssl) {
	case SSL_SSLV3:
		if (min_ver <= TLS1_2_VERSION)
			return 1;
		break;
	case SSL_TLSV1_2:
		if (min_ver <= TLS1_2_VERSION && TLS1_2_VERSION <= max_ver)
			return 1;
		break;
	case SSL_TLSV1_3:
		if (min_ver <= TLS1_3_VERSION && TLS1_3_VERSION <= max_ver)
			return 1;
		break;
	}

	return 0;
}
