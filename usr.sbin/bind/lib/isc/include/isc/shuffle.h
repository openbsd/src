/*
 * Portions Copyright (C) 2002  Internet Software Consortium.
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

/* $OpenBSD: shuffle.h,v 1.1 2008/02/29 12:21:12 deraadt Exp $ */

#ifndef ISC_SHUFFLE_H
#define ISC_SHUFFLE_H 1

#include <isc/lang.h>
#include <isc/types.h>

typedef struct isc_shuffle isc_shuffle_t;

struct isc_shuffle {
	isc_uint16_t id_shuffle[65536];
	int isindex;
};

ISC_LANG_BEGINDECLS

void
isc_shuffle_init(isc_shuffle_t *shuffle);
/*
 * Initialize a Shuffle generator
 *
 * Requires:
 *
 *	shuffle != NULL
 */

isc_uint16_t
isc_shuffle_generate16(isc_shuffle_t *shuffle);
/*
 * Get a random number from a Shuffle generator
 *
 * Requires:
 *
 *	shuffle be valid.
 *
 *	data != NULL.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_SHUFFLE_H */
