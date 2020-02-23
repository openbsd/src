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

/* $Id: secalg.h,v 1.3 2020/02/23 19:54:25 jung Exp $ */

#ifndef DNS_SECALG_H
#define DNS_SECALG_H 1

/*! \file dns/secalg.h */

#include <dns/types.h>

isc_result_t
dns_secalg_totext(dns_secalg_t secalg, isc_buffer_t *target);
/*%<
 * Put a textual representation of the DNSSEC security algorithm 'secalg'
 * into 'target'.
 *
 * Requires:
 *\li	'secalg' is a valid secalg.
 *
 *\li	'target' is a valid text buffer.
 *
 * Ensures,
 *	if the result is success:
 *\li		The used space in 'target' is updated.
 *
 * Returns:
 *\li	ISC_R_SUCCESS			on success
 *\li	ISC_R_NOSPACE			target buffer is too small
 */

#define DNS_SECALG_FORMATSIZE 20
void
dns_secalg_format(dns_secalg_t alg, char *cp, unsigned int size);
/*%<
 * Wrapper for dns_secalg_totext(), writing text into 'cp'
 */

#endif /* DNS_SECALG_H */
