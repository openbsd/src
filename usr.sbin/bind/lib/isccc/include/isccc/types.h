/*
 * Portions Copyright (C) 2001  Internet Software Consortium.
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM AND
 * NOMINUM DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
 * SHALL INTERNET SOFTWARE CONSORTIUM OR NOMINUM BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: types.h,v 1.2 2001/03/27 20:08:14 bwelling Exp $ */

#ifndef ISCCC_TYPES_H
#define ISCCC_TYPES_H 1

#include <isc/boolean.h>
#include <isc/int.h>
#include <isc/result.h>

typedef isc_uint32_t isccc_time_t;
typedef struct isccc_sexpr isccc_sexpr_t;
typedef struct isccc_dottedpair isccc_dottedpair_t;
typedef struct isccc_symtab isccc_symtab_t;

typedef struct isccc_region {
	unsigned char *		rstart;
	unsigned char *		rend;
} isccc_region_t;

#endif /* ISCCC_TYPES_H */
