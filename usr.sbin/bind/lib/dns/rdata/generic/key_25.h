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

#ifndef GENERIC_KEY_25_H
#define GENERIC_KEY_25_H 1

/* $ISC: key_25.h,v 1.14 2001/01/09 21:54:07 bwelling Exp $ */

/* RFC 2535 */

typedef struct dns_rdata_key_t {
        dns_rdatacommon_t	common;
        isc_mem_t *		mctx;
        isc_uint16_t		flags;
        isc_uint8_t		protocol;
        isc_uint8_t		algorithm;
        isc_uint16_t		datalen;
        unsigned char *		data;
} dns_rdata_key_t;


#endif /* GENERIC_KEY_25_H */
