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

#ifndef DNS_ZONE_P_H
#define DNS_ZONE_P_H

/*! \file */

/*%
 *     Types and functions below not be used outside this module and its
 *     associated unit tests.
 */

ISC_LANG_BEGINDECLS

typedef struct {
	dns_diff_t	*diff;
	isc_boolean_t	offline;
} dns__zonediff_t;

isc_result_t
dns__zone_findkeys(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver,
		   isc_mem_t *mctx, unsigned int maxkeys,
		   dst_key_t **keys, unsigned int *nkeys);

isc_result_t
dns__zone_updatesigs(dns_diff_t *diff, dns_db_t *db, dns_dbversion_t *version,
		     dst_key_t *zone_keys[], unsigned int nkeys,
		     dns_zone_t *zone, isc_stdtime_t inception,
		     isc_stdtime_t expire, isc_stdtime_t now,
		     isc_boolean_t check_ksk, isc_boolean_t keyset_kskonly,
		     dns__zonediff_t *zonediff);

ISC_LANG_ENDDECLS

#endif /* DNS_ZONE_P_H */
