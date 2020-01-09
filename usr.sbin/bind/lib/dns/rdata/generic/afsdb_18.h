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

#ifndef GENERIC_AFSDB_18_H
#define GENERIC_AFSDB_18_H 1

/* $Id: afsdb_18.h,v 1.4 2020/01/09 18:17:17 florian Exp $ */

/*!
 *  \brief Per RFC1183 */

typedef struct dns_rdata_afsdb {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	uint16_t		subtype;
	dns_name_t		server;
} dns_rdata_afsdb_t;

#endif /* GENERIC_AFSDB_18_H */

