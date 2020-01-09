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

#ifndef GENERIC_GPOS_27_H
#define GENERIC_GPOS_27_H 1

/* $Id: gpos_27.h,v 1.4 2020/01/09 18:17:17 florian Exp $ */

/*!
 *  \brief per RFC1712 */

typedef struct dns_rdata_gpos {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	char			*longitude;
	char			*latitude;
	char			*altitude;
	uint8_t		long_len;
	uint8_t		lat_len;
	uint8_t		alt_len;
} dns_rdata_gpos_t;

#endif /* GENERIC_GPOS_27_H */
