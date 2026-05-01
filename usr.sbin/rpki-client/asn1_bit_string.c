/*	$OpenBSD: asn1_bit_string.c,v 1.1 2026/05/01 11:22:24 tb Exp $ */

/*
 * Copyright (c) 2025 Theo Buehler <tb@openbsd.org>
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
#include <stddef.h>
#include <stdint.h>

#include <openssl/asn1.h>

#include "extern.h"

/*
 * My implementation of the OpenSSL 4 API that beck upstreamed in:
 * https://github.com/openssl/openssl/pull/29926
 * https://github.com/openssl/openssl/pull/29387
 */

#ifndef HAVE_ASN1_BIT_STRING_GET_LENGTH
int
ASN1_BIT_STRING_get_length(const ASN1_BIT_STRING *abs, size_t *out_length,
    int *out_unused_bits)
{
	size_t length;
	int unused_bits;

	if (abs == NULL || abs->type != V_ASN1_BIT_STRING)
		return 0;

	if (out_length == NULL || out_unused_bits == NULL)
		return 0;

	length = abs->length;
	unused_bits = 0;

	if ((abs->flags & ASN1_STRING_FLAG_BITS_LEFT) != 0)
		unused_bits = abs->flags & 0x07;

	if (length == 0 && unused_bits != 0)
		return 0;

	if (unused_bits != 0) {
		unsigned char mask = (1 << unused_bits) - 1;
		if ((abs->data[length - 1] & mask) != 0)
			return 0;
	}

	*out_length = length;
	*out_unused_bits = unused_bits;

	return 1;
}
#endif /* !HAVE_ASN1_BIT_STRING_GET_LENGTH */

#ifndef HAVE_ASN1_BIT_STRING_SET1
int
ASN1_BIT_STRING_set1(ASN1_BIT_STRING *abs, const uint8_t *data, size_t length,
    int unused_bits)
{
	/* OpenSSL likes such checks. */
	if (abs == NULL)
		return 0;

	if (length > INT_MAX || unused_bits < 0 || unused_bits > 7)
		return 0;

	if (length == 0 && unused_bits != 0)
		return 0;

	if (length > 0 && (data[length - 1] & ((1 << unused_bits) - 1)) != 0)
		return 0;

	if (!ASN1_STRING_set(abs, data, length))
		return 0;

	abs->type = V_ASN1_BIT_STRING;
	abs->flags &= ~(ASN1_STRING_FLAG_BITS_LEFT | 0x07);
	abs->flags |= ASN1_STRING_FLAG_BITS_LEFT | unused_bits;

	return 1;
}
#endif /* !HAVE_ASN1_BIT_STRING_SET1 */
