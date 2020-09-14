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

/* $Id: compress.h,v 1.5 2020/09/14 08:40:43 florian Exp $ */

#ifndef DNS_COMPRESS_H
#define DNS_COMPRESS_H 1

#include <isc/region.h>

#include <dns/types.h>

#define DNS_COMPRESS_NONE		0x00	/*%< no compression */
#define DNS_COMPRESS_GLOBAL14		0x01	/*%< "normal" compression. */
#define DNS_COMPRESS_ALL		0x01	/*%< all compression. */
#define DNS_COMPRESS_CASESENSITIVE	0x02	/*%< case sensitive compression. */

/*! \file dns/compress.h
 *	Direct manipulation of the structures is strongly discouraged.
 */

#define DNS_COMPRESS_TABLESIZE 64
#define DNS_COMPRESS_INITIALNODES 16

typedef struct dns_compressnode dns_compressnode_t;

struct dns_compressnode {
	isc_region_t		r;
	uint16_t		offset;
	uint16_t		count;
	uint8_t		labels;
	dns_compressnode_t	*next;
};

struct dns_compress {
	unsigned int		allowed;	/*%< Allowed methods. */
	int			edns;		/*%< Edns version or -1. */
	/*% Global compression table. */
	dns_compressnode_t	*table[DNS_COMPRESS_TABLESIZE];
	/*% Preallocated nodes for the table. */
	dns_compressnode_t	initialnodes[DNS_COMPRESS_INITIALNODES];
	uint16_t		count;		/*%< Number of nodes. */
};

typedef enum {
	DNS_DECOMPRESS_ANY,			/*%< Any compression */
	DNS_DECOMPRESS_STRICT,			/*%< Allowed compression */
	DNS_DECOMPRESS_NONE			/*%< No compression */
} dns_decompresstype_t;

struct dns_decompress {
	unsigned int		allowed;	/*%< Allowed methods. */
	int			edns;		/*%< Edns version or -1. */
	dns_decompresstype_t	type;		/*%< Strict checking */
};

isc_result_t
dns_compress_init(dns_compress_t *cctx, int edns);
/*%<
 *	Initialise the compression context structure pointed to by 'cctx'.
 *
 *	Requires:
 *	\li	'cctx' is a valid dns_compress_t structure.
 *	\li	'mctx' is an initialized memory context.
 *	Ensures:
 *	\li	cctx->global is initialized.
 *
 *	Returns:
 *	\li	#ISC_R_SUCCESS
 */

void
dns_compress_invalidate(dns_compress_t *cctx);

/*%<
 *	Invalidate the compression structure pointed to by cctx.
 *
 *	Requires:
 *\li		'cctx' to be initialized.
 */

void
dns_compress_setmethods(dns_compress_t *cctx, unsigned int allowed);

/*%<
 *	Sets allowed compression methods.
 *
 *	Requires:
 *\li		'cctx' to be initialized.
 */

unsigned int
dns_compress_getmethods(dns_compress_t *cctx);

/*%<
 *	Gets allowed compression methods.
 *
 *	Requires:
 *\li		'cctx' to be initialized.
 *
 *	Returns:
 *\li		allowed compression bitmap.
 */

int
dns_compress_findglobal(dns_compress_t *cctx, const dns_name_t *name,
			dns_name_t *prefix, uint16_t *offset);
/*%<
 *	Finds longest possible match of 'name' in the global compression table.
 *
 *	Requires:
 *\li		'cctx' to be initialized.
 *\li		'name' to be a absolute name.
 *\li		'prefix' to be initialized.
 *\li		'offset' to point to an uint16_t.
 *
 *	Ensures:
 *\li		'prefix' and 'offset' are valid if 1 is 	returned.
 *
 *	Returns:
 *\li		#1 / #0
 */

void
dns_compress_add(dns_compress_t *cctx, const dns_name_t *name,
		 const dns_name_t *prefix, uint16_t offset);
/*%<
 *	Add compression pointers for 'name' to the compression table,
 *	not replacing existing pointers.
 *
 *	Requires:
 *\li		'cctx' initialized
 *
 *\li		'name' must be initialized and absolute, and must remain
 *		valid until the message compression is complete.
 *
 *\li		'prefix' must be a prefix returned by
 *		dns_compress_findglobal(), or the same as 'name'.
 */

void
dns_compress_rollback(dns_compress_t *cctx, uint16_t offset);

/*%<
 *	Remove any compression pointers from global table >= offset.
 *
 *	Requires:
 *\li		'cctx' is initialized.
 */

void
dns_decompress_init(dns_decompress_t *dctx, int edns,
		    dns_decompresstype_t type);

/*%<
 *	Initializes 'dctx'.
 *	Records 'edns' and 'type' into the structure.
 *
 *	Requires:
 *\li		'dctx' to be a valid pointer.
 */

void
dns_decompress_invalidate(dns_decompress_t *dctx);

/*%<
 *	Invalidates 'dctx'.
 *
 *	Requires:
 *\li		'dctx' to be initialized
 */

void
dns_decompress_setmethods(dns_decompress_t *dctx, unsigned int allowed);

/*%<
 *	Sets 'dctx->allowed' to 'allowed'.
 *
 *	Requires:
 *\li		'dctx' to be initialized
 */

dns_decompresstype_t
dns_decompress_type(dns_decompress_t *dctx);

/*%<
 *	Returns 'dctx->type'
 *
 *	Requires:
 *\li		'dctx' to be initialized
 */

#endif /* DNS_COMPRESS_H */
