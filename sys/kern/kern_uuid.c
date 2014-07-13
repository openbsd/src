/*	$OpenBSD: kern_uuid.c,v 1.1 2014/07/13 15:32:28 miod Exp $	*/
/*	$NetBSD: kern_uuid.c,v 1.18 2011/11/19 22:51:25 tls Exp $	*/

/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: /repoman/r/ncvs/src/sys/kern/kern_uuid.c,v 1.7 2004/01/12 
13:34:11 rse Exp $
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/uuid.h>

/*
 * For a description of UUIDs see RFC 4122.
 */

struct uuid_private {
	struct {
		uint32_t low;
		uint16_t mid;
		uint16_t hi;
	} time;
	uint16_t seq;
	uint16_t node[UUID_NODE_LEN>>1];
};

#ifdef DEBUG
int
uuid_snprintf(char *buf, size_t sz, const struct uuid *uuid)
{
	const struct uuid_private *id;
	int cnt;

	id = (const struct uuid_private *)uuid;
	cnt = snprintf(buf, sz, "%08x-%04x-%04x-%04x-%04x%04x%04x",
	    id->time.low, id->time.mid, id->time.hi, betoh16(id->seq),
	    betoh16(id->node[0]), betoh16(id->node[1]), betoh16(id->node[2]));
	return (cnt);
}

int
uuid_printf(const struct uuid *uuid)
{
	char buf[_UUID_BUF_LEN];

	(void) uuid_snprintf(buf, sizeof(buf), uuid);
	printf("%s", buf);
	return (0);
}
#endif

/*
 * Encode/Decode UUID into octet-stream.
 *
 * 0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                          time_low                             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |       time_mid                |         time_hi_and_version   |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         node (2-5)                            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

void
uuid_enc_le(void *buf, const struct uuid *uuid)
{
	uint8_t *p = buf;
	int i;

	p[0] = htole32(uuid->time_low);
	p[4] = htole16(uuid->time_mid);
	p[6] = htole16(uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
uuid_dec_le(void const *buf, struct uuid *uuid)
{
	const uint8_t *p = buf;
	int i;

	uuid->time_low = letoh32(*(uint32_t*)p);
	uuid->time_mid = letoh16(*(uint16_t*)(p + 4));
	uuid->time_hi_and_version = letoh16(*(uint16_t*)(p + 6));
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}

void
uuid_enc_be(void *buf, const struct uuid *uuid)
{
	uint8_t *p = buf;
	int i;

	p[0] = htobe32(uuid->time_low);
	p[4] = htobe16(uuid->time_mid);
	p[6] = htobe16(uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
uuid_dec_be(void const *buf, struct uuid *uuid)
{
	const uint8_t *p = buf;
	int i;

	uuid->time_low = betoh32(*(uint32_t*)p);
	uuid->time_mid = betoh16(*(uint16_t*)(p + 4));
	uuid->time_hi_and_version = betoh16(*(uint16_t*)(p + 6));
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}
