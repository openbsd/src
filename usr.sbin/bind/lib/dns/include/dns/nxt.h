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

/* $ISC: nxt.h,v 1.12 2001/01/09 21:53:08 bwelling Exp $ */

#ifndef DNS_NXT_H
#define DNS_NXT_H 1

#include <isc/lang.h>

#include <dns/types.h>

#define DNS_NXT_BUFFERSIZE (256 + 16)

ISC_LANG_BEGINDECLS

isc_result_t
dns_nxt_buildrdata(dns_db_t *db, dns_dbversion_t *version,
		   dns_dbnode_t *node, dns_name_t *target,
		   unsigned char *buffer, dns_rdata_t *rdata);
/*
 * Build the rdata of a NXT record.
 *
 * Requires:
 *	buffer	Points to a temporary buffer of at least
 * 		DNS_NXT_BUFFERSIZE bytes.
 *	rdata	Points to an initialized dns_rdata_t.
 *
 * Ensures:
 *      *rdata	Contains a valid NXT rdata.  The 'data' member refers
 *		to 'buffer'.
 */

isc_result_t
dns_nxt_build(dns_db_t *db, dns_dbversion_t *version, dns_dbnode_t *node,
	      dns_name_t *target, dns_ttl_t ttl);
/*
 * Build a NXT record and add it to a database.
 */

isc_boolean_t
dns_nxt_typepresent(dns_rdata_t *nxt, dns_rdatatype_t type);
/*
 * Determine if a type is marked as present in an NXT record.
 *
 * Requires:
 *	'nxt' points to a valid rdataset of type NXT
 *	'type' < 128
 *
 */

ISC_LANG_ENDDECLS

#endif /* DNS_NXT_H */
