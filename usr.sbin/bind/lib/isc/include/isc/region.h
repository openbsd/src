/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
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

/* $ISC: region.h,v 1.16 2001/01/09 21:57:25 bwelling Exp $ */

#ifndef ISC_REGION_H
#define ISC_REGION_H 1

#include <isc/types.h>

struct isc_region {
	unsigned char *	base;
	unsigned int	length;
};

struct isc_textregion {
	char *		base;
	unsigned int	length;
};

/* XXXDCL questionable ... bears discussion.  we have been putting off
 * discussing the region api.
 */
struct isc_constregion {
	const void *	base;
	unsigned int	length;
};

struct isc_consttextregion {
	const char *	base;
	unsigned int	length;
};

/*
 * The region structure is not opaque, and is usually directly manipulated.
 * Some macros are defined below for convenience.
 */

#define isc_region_consume(r,l) \
	do { \
		isc_region_t *_r = (r); \
		unsigned int _l = (l); \
		INSIST(_r->length >= _l); \
		_r->base += _l; \
		_r->length -= _l; \
	} while (0)

#define isc_textregion_consume(r,l) \
	do { \
		isc_textregion_t *_r = (r); \
		unsigned int _l = (l); \
		INSIST(_r->length >= _l); \
		_r->base += _l; \
		_r->length -= _l; \
	} while (0)

#define isc_constregion_consume(r,l) \
	do { \
		isc_constregion_t *_r = (r); \
		unsigned int _l = (l); \
		INSIST(_r->length >= _l); \
		_r->base += _l; \
		_r->length -= _l; \
	} while (0)

#endif /* ISC_REGION_H */
