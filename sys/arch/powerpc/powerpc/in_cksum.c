/*	$OpenBSD: in_cksum.c,v 1.5 2002/09/15 09:01:59 deraadt Exp $	*/
/*	$NetBSD: in_cksum.c,v 1.1 1996/09/30 16:34:47 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <netinet/in.h>

/*
 * First cut for in_cksum.
 * This code is in C and should be optimized for PPC later.
 */
#define	REDUCE		(sum = (sum & 0xffff) + (sum >> 16))
#define	ROL		(sum = sum << 8)
#define	ADDB		(ROL, sum += *w, byte_swapped ^= 1)
#define	ADDS		(sum += *(u_short *)w)
#define	SHIFT(n)	(w += (n), mlen -= (n))
#define	ADDCARRY	do { while (sum > 0xffff) REDUCE; } while (0)

int
in_cksum(m, len)
	struct mbuf *m;
	int len;
{
	u_char *w;
	u_int sum = 0;
	int mlen;
	int byte_swapped = 0;
	
	for (; m && len; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		w = mtod(m, u_char *);
		mlen = m->m_len;
		if (len < mlen)
			mlen = len;
		len -= mlen;
		if ((long)w & 1) {
			REDUCE;
			ADDB;
			SHIFT(1);
		}
		while (mlen >= 2) {
			ADDS;
			SHIFT(2);
		}
		REDUCE;
		if (mlen == 1)
			ADDB;
	}
	if (byte_swapped) {
		REDUCE;
		ROL;
	}
	ADDCARRY;
	return sum ^ 0xffff;
}
