/*	$OpenBSD: put.c,v 1.6 2004/04/14 20:37:28 henning Exp $ */

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
    "$OpenBSD: put.c,v 1.6 2004/04/14 20:37:28 henning Exp $";
#endif

#include <stddef.h>
#include <sys/types.h>
#include <time.h>
#include "common/mopdef.h"

void
mopPutChar(u_char *pkt, int *index, u_char value)
{
	pkt[*index] = value;
	(*index)++;
}

void
mopPutShort(u_char *pkt, int *index, u_short value)
{
	int i;

	for (i = 0; i < 2; i++) {
		pkt[*index + i] = value % 256;
		value /= 256;
	}
	*index += 2;
}

void
mopPutLong(u_char *pkt, int *index, u_long value)
{
	int i;

	for (i = 0; i < 4; i++) {
		pkt[*index + i] = value % 256;
		value /= 256;
	}
	*index += 4;
}

void
mopPutMulti(u_char *pkt, int *index, u_char *value, int size)
{
	int i;

	for (i = 0; i < size; i++)
		pkt[*index + i] = value[i];
	*index += size;
}

void
mopPutTime(u_char *pkt, int *index, time_t value)
{
	time_t		 tnow;
	struct tm	*timenow;

	if ((value == 0))
		tnow = time(NULL);
	else
		tnow = value;

	timenow = localtime(&tnow);

	mopPutChar(pkt, index, 10);
	mopPutChar(pkt, index, (timenow->tm_year / 100) + 19);
	mopPutChar(pkt, index, (timenow->tm_year % 100));
	mopPutChar(pkt, index, (timenow->tm_mon + 1));
	mopPutChar(pkt, index, (timenow->tm_mday));
	mopPutChar(pkt, index, (timenow->tm_hour));
	mopPutChar(pkt, index, (timenow->tm_min));
	mopPutChar(pkt, index, (timenow->tm_sec));
	mopPutChar(pkt, index, 0x00);
	mopPutChar(pkt, index, 0x00);
	mopPutChar(pkt, index, 0x00);
}

void
mopPutHeader(u_char *pkt, int *index, char dst[], char src[], u_short proto,
    int trans)
{
	mopPutMulti(pkt, index, dst, 6);
	mopPutMulti(pkt, index, src, 6);
	if (trans == TRANS_8023) {
		mopPutShort(pkt, index, 0);
		mopPutChar(pkt, index, MOP_K_PROTO_802_DSAP);
		mopPutChar(pkt, index, MOP_K_PROTO_802_SSAP);
		mopPutChar(pkt, index, MOP_K_PROTO_802_CNTL);
		mopPutChar(pkt, index, 0x08);
		mopPutChar(pkt, index, 0x00);
		mopPutChar(pkt, index, 0x2b);
	}
#if !defined(__FreeBSD__)
	mopPutChar(pkt, index, (proto / 256));
	mopPutChar(pkt, index, (proto % 256));
#else
	if (trans == TRANS_8023) {
		mopPutChar(pkt, index, (proto / 256));
		mopPutChar(pkt, index, (proto % 256));
	} else {
		mopPutChar(pkt, index, (proto % 256));
		mopPutChar(pkt, index, (proto / 256));
	}
#endif
	if (trans == TRANS_ETHER)
		mopPutShort(pkt, index, 0);

}

void
mopPutLength(u_char *pkt, int trans, u_short len)
{
	int	 index = 0;

	switch (trans) {
	case TRANS_ETHER:
		index = 14;
		mopPutChar(pkt, &index, ((len - 16) % 256));
		mopPutChar(pkt, &index, ((len - 16) / 256));
		break;
	case TRANS_8023:
		index = 12;
#if !defined(__FreeBSD__)
		mopPutChar(pkt, &index, ((len - 14) / 256));
		mopPutChar(pkt, &index, ((len - 14) % 256));
#else
		mopPutChar(pkt, &index, ((len - 14) % 256));
		mopPutChar(pkt, &index, ((len - 14) / 256));
#endif
		break;
	}

}



