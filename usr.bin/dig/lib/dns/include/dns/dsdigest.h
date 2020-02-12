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

/* $Id: dsdigest.h,v 1.2 2020/02/12 13:05:03 jsg Exp $ */

#ifndef DNS_DSDIGEST_H
#define DNS_DSDIGEST_H 1

/*! \file dns/dsdigest.h */

#include <isc/lang.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

isc_result_t
dns_dsdigest_fromtext(dns_dsdigest_t *dsdigestp, isc_textregion_t *source);
/*%<
 * Convert the text 'source' refers to into a DS/DLV digest type value.
 * The text may contain either a mnemonic digest name or a decimal
 * digest number.
 *
 * Requires:
 *\li	'dsdigestp' is a valid pointer.
 *
 *\li	'source' is a valid text region.
 *
 * Returns:
 *\li	ISC_R_SUCCESS			on success
 *\li	ISC_R_RANGE			numeric type is out of range
 *\li	DNS_R_UNKNOWN			mnemonic type is unknown
 */

#define DNS_DSDIGEST_FORMATSIZE 20

ISC_LANG_ENDDECLS

#endif /* DNS_DSDIGEST_H */
