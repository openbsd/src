/*	$OpenBSD: convert.c,v 1.22 2016/01/22 18:57:42 krw Exp $	*/

/*
 * convert.c - Little-endian conversion
 *
 * Written by Eryk Vershen
 *
 * See comments in convert.h
 */

/*
 * Copyright 1996,1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <endian.h>

#include "convert.h"

void	reverse2 (uint8_t *);
void	reverse4 (uint8_t *);

int
convert_dpme(struct dpme *dpme, int to_cpu_form)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	/*
         * Since we will toss the block if the signature doesn't match
         * we don't need to check the signature down here.
         */
	reverse2((uint8_t *)&dpme->dpme_signature);
	reverse2((uint8_t *)&dpme->dpme_reserved_1);
	reverse4((uint8_t *)&dpme->dpme_map_entries);
	reverse4((uint8_t *)&dpme->dpme_pblock_start);
	reverse4((uint8_t *)&dpme->dpme_pblocks);
	reverse4((uint8_t *)&dpme->dpme_lblock_start);
	reverse4((uint8_t *)&dpme->dpme_lblocks);
	reverse4((uint8_t *)&dpme->dpme_flags);
	reverse4((uint8_t *)&dpme->dpme_boot_block);
	reverse4((uint8_t *)&dpme->dpme_boot_bytes);
	reverse4((uint8_t *)&dpme->dpme_load_addr);
	reverse4((uint8_t *)&dpme->dpme_load_addr_2);
	reverse4((uint8_t *)&dpme->dpme_goto_addr);
	reverse4((uint8_t *)&dpme->dpme_goto_addr_2);
	reverse4((uint8_t *)&dpme->dpme_checksum);
#endif
	return 0;
}


int
convert_block0(struct block0 *block0, int to_cpu_form)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	struct ddmap *m;
	uint16_t count;
	int i;

	/*
         * Since this data is optional we do not want to convert willy-nilly.
         * We use the flag to determine whether to check for the signature
         * before or after we flip the bytes and to determine which form of
         * the count to use.
         */
	if (to_cpu_form) {
		reverse2((uint8_t *)&block0->sbSig);
		if (block0->sbSig != BLOCK0_SIGNATURE) {
			reverse2((uint8_t *)&block0->sbSig);
			if (block0->sbSig != BLOCK0_SIGNATURE) {
				return 0;
			}
		}
	} else {
		if (block0->sbSig != BLOCK0_SIGNATURE) {
			return 0;
		}
		reverse2((uint8_t *)&block0->sbSig);
	}
	reverse2((uint8_t *)&block0->sbBlkSize);
	reverse4((uint8_t *)&block0->sbBlkCount);
	reverse2((uint8_t *)&block0->sbDevType);
	reverse2((uint8_t *)&block0->sbDevId);
	reverse4((uint8_t *)&block0->sbData);
	if (to_cpu_form) {
		reverse2((uint8_t *)&block0->sbDrvrCount);
		count = block0->sbDrvrCount;
	} else {
		count = block0->sbDrvrCount;
		reverse2((uint8_t *)&block0->sbDrvrCount);
	}

	if (count > 0) {
		m = (struct ddmap *)block0->sbMap;
		for (i = 0; i < count; i++) {
			reverse4((uint8_t *)&m[i].ddBlock);
			reverse2((uint8_t *)&m[i].ddSize);
			reverse2((uint8_t *)&m[i].ddType);
		}
	}
#endif
	return 0;
}


void
reverse2(uint8_t *bytes)
{
	uint8_t t;

	t = *bytes;
	*bytes = bytes[1];
	bytes[1] = t;
}


void
reverse4(uint8_t *bytes)
{
	uint8_t t;

	t = *bytes;
	*bytes = bytes[3];
	bytes[3] = t;
	t = bytes[1];
	bytes[1] = bytes[2];
	bytes[2] = t;
}
