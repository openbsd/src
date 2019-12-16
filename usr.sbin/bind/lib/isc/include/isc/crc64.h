/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef ISC_CRC64_H
#define ISC_CRC64_H 1

/*! \file isc/crc64.h
 * \brief CRC64 in C
 */

#include <isc/lang.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

void
isc_crc64_init(isc_uint64_t *crc);
/*%
 * Initialize a new CRC.
 *
 * Requires:
 * * 'crc' is not NULL.
 */

void
isc_crc64_update(isc_uint64_t *crc, const void *data, size_t len);
/*%
 * Add data to the CRC.
 *
 * Requires:
 * * 'crc' is not NULL.
 * * 'data' is not NULL.
 */

void
isc_crc64_final(isc_uint64_t *crc);
/*%
 * Finalize the CRC.
 *
 * Requires:
 * * 'crc' is not NULL.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_CRC64_H */
