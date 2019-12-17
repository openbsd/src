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

/* $Id: internal.h,v 1.2 2019/12/17 01:46:35 sthen Exp $ */

#ifndef PK11_INTERNAL_H
#define PK11_INTERNAL_H 1

/*! \file pk11/internal.h */

ISC_LANG_BEGINDECLS

const char *pk11_get_lib_name(void);

void *pk11_mem_get(size_t size);

void pk11_mem_put(void *ptr, size_t size);

CK_SLOT_ID pk11_get_best_token(pk11_optype_t optype);

unsigned int pk11_numbits(CK_BYTE_PTR data, unsigned int bytecnt);

CK_ATTRIBUTE *pk11_attribute_first(const pk11_object_t *obj);

CK_ATTRIBUTE *pk11_attribute_next(const pk11_object_t *obj,
				  CK_ATTRIBUTE *attr);

CK_ATTRIBUTE *pk11_attribute_bytype(const pk11_object_t *obj,
				    CK_ATTRIBUTE_TYPE type);

ISC_LANG_ENDDECLS

#endif /* PK11_INTERNAL_H */
