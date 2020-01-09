/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef GENERIC_CAA_257_H
#define GENERIC_CAA_257_H 1

/* $Id: caa_257.h,v 1.3 2020/01/09 18:17:17 florian Exp $ */

typedef struct dns_rdata_caa {
	dns_rdatacommon_t	common;
	isc_mem_t *		mctx;
	uint8_t		flags;
	unsigned char *		tag;
	uint8_t		tag_len;
	unsigned char		*value;
	uint16_t		value_len;
} dns_rdata_caa_t;

#endif /* GENERIC_CAA_257_H */
