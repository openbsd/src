/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: cert_37.h,v 1.15 2001/01/09 21:53:53 bwelling Exp $ */

/* RFC 2538 */
#ifndef GENERIC_CERT_37_H
#define GENERIC_CERT_37_H 1

typedef struct dns_rdata_cert {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	isc_uint16_t		type;
	isc_uint16_t		key_tag;
	isc_uint8_t		algorithm;
	isc_uint16_t		length;
	unsigned char		*certificate;
} dns_rdata_cert_t;

#endif /* GENERIC_CERT_37_H */
