/*
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id: sshfp_44.h,v 1.1 2003/10/19 18:19:11 jakob Exp $ */

/* draft-ietf-secsh-dns-05.txt */
#ifndef GENERIC_SSHFP_44_H
#define GENERIC_SSHFP_44_H 1

typedef struct dns_rdata_sshfp {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	isc_uint8_t		algorithm;
	isc_uint8_t		digest_type;
	isc_uint16_t		length;
	unsigned char		*digest;
} dns_rdata_sshfp_t;

#endif /* GENERIC_SSHFP_44_H */
