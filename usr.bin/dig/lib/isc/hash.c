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

/* $Id: hash.c,v 1.6 2020/02/25 05:00:43 jsg Exp $ */

/*! \file
 * Some portion of this code was derived from universal hash function
 * libraries of Rice University.
\section license UH Universal Hashing Library

Copyright ((c)) 2002, Rice University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.

    * Neither the name of Rice University (RICE) nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

This software is provided by RICE and the contributors on an "as is"
basis, without any representations or warranties of any kind, express
or implied including, but not limited to, representations or
warranties of non-infringement, merchantability or fitness for a
particular purpose. In no event shall RICE or contributors be liable
for any direct, indirect, incidental, special, exemplary, or
consequential damages (including, but not limited to, procurement of
substitute goods or services; loss of use, data, or profits; or
business interruption) however caused and on any theory of liability,
whether in contract, strict liability, or tort (including negligence
or otherwise) arising in any way out of the use of this software, even
if advised of the possibility of such damage.
*/

#include <stdlib.h>

#include <isc/hash.h>
#include <isc/util.h>

static unsigned char maptolower[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static uint32_t fnv_offset_basis;
static isc_boolean_t fnv_once = ISC_FALSE;

static void
fnv_initialize(void) {
	/*
	 * This function should not leave fnv_offset_basis set to
	 * 0. Also, after this function has been called, if it is called
	 * again, it should not change fnv_offset_basis.
	 */
	while (fnv_offset_basis == 0) {
		fnv_offset_basis = arc4random();
	}
}

uint32_t
isc_hash_function_reverse(const void *data, size_t length,
			  isc_boolean_t case_sensitive,
			  const uint32_t *previous_hashp)
{
	uint32_t hval;
	const unsigned char *bp;
	const unsigned char *be;

	REQUIRE(length == 0 || data != NULL);
	if (!fnv_once) {
		fnv_once = ISC_TRUE;
		fnv_initialize();
	}

	hval = previous_hashp != NULL ?
		*previous_hashp : fnv_offset_basis;

	if (length == 0)
		return (hval);

	bp = (const unsigned char *) data;
	be = bp + length;

	/*
	 * Fowler-Noll-Vo FNV-1a hash function.
	 *
	 * NOTE: A random fnv_offset_basis is used by default to avoid
	 * collision attacks as the hash function is reversible. This
	 * makes the mapping non-deterministic, but the distribution in
	 * the domain is still uniform.
	 */

	if (case_sensitive) {
		while (be >= bp + 4) {
			be -= 4;
			hval ^= (uint32_t) be[3];
			hval *= 16777619;
			hval ^= (uint32_t) be[2];
			hval *= 16777619;
			hval ^= (uint32_t) be[1];
			hval *= 16777619;
			hval ^= (uint32_t) be[0];
			hval *= 16777619;
		}
		while (--be >= bp) {
			hval ^= (uint32_t) *be;
			hval *= 16777619;
		}
	} else {
		while (be >= bp + 4) {
			be -= 4;
			hval ^= (uint32_t) maptolower[be[3]];
			hval *= 16777619;
			hval ^= (uint32_t) maptolower[be[2]];
			hval *= 16777619;
			hval ^= (uint32_t) maptolower[be[1]];
			hval *= 16777619;
			hval ^= (uint32_t) maptolower[be[0]];
			hval *= 16777619;
		}
		while (--be >= bp) {
			hval ^= (uint32_t) maptolower[*be];
			hval *= 16777619;
		}
	}

	return (hval);
}
