/*	$OpenBSD: get.c,v 1.6 2004/04/14 20:37:28 henning Exp $ */

/*
 * Copyright (c) 1993-95 Mats O Jansson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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
 */

#ifndef LINT
static const char rcsid[] =
    "$OpenBSD: get.c,v 1.6 2004/04/14 20:37:28 henning Exp $";
#endif

#include <sys/types.h>
#include "common/mopdef.h"

u_char
mopGetChar(u_char *pkt, int *index)
{
	u_char ret;

	ret = pkt[*index];
	*index = *index + 1;
	return (ret);
}

u_short
mopGetShort(u_char *pkt, int *index)
{
	u_short ret;

	ret = pkt[*index] + pkt[*index+1]*256;
	*index = *index + 2;
	return (ret);
}

u_long
mopGetLong(u_char *pkt, int *index)
{
	u_long ret;

	ret = pkt[*index] + pkt[*index+1]*0x100 + pkt[*index+2]*0x10000 +
	    pkt[*index+3]*0x1000000;
	*index = *index + 4;
	return (ret);
}

void
mopGetMulti(u_char *pkt, int *index, u_char *dest, int size)
{
	int i;

	for (i = 0; i < size; i++)
		dest[i] = pkt[*index+i];
	*index = *index + size;
}

int
mopGetTrans(u_char *pkt, int trans)
{
	u_short	*ptype;

	if (trans == 0) {
		ptype = (u_short *)(pkt+12);
		if (ntohs(*ptype) < 1600)
			trans = TRANS_8023;
		else
			trans = TRANS_ETHER;
	}
	return (trans);
}

void
mopGetHeader(u_char *pkt, int *index, u_char **dst, u_char **src,
    u_short *proto, int *len, int trans)
{
	*dst = pkt;
	*src = pkt + 6;
	*index = *index + 12;

	switch (trans) {
	case TRANS_ETHER:
		*proto = (u_short)(pkt[*index] * 256 + pkt[*index + 1]);
		*index = *index + 2;
		*len   = (int)(pkt[*index + 1] * 256 + pkt[*index]);
		*index = *index + 2;
		break;
	case TRANS_8023:
		*len   = (int)(pkt[*index] * 256 + pkt[*index + 1]);
		*index = *index + 8;
		*proto = (u_short)(pkt[*index] * 256 + pkt[*index + 1]);
		*index = *index + 2;
		break;
	}
}

u_short
mopGetLength(u_char *pkt, int trans)
{
	switch (trans) {
	case TRANS_ETHER:
		return (pkt[15] * 256 + pkt[14]);
		break;
	case TRANS_8023:
		return (pkt[12] * 256 + pkt[13]);
		break;
	}
	return (0);
}
