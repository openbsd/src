/*	$OpenBSD: in_cksum.c,v 1.3 2008/02/16 23:02:39 miod Exp $	*/

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
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#define	REDUCE		(sum = (sum & 0xffff) + (sum >> 16))
#define	ROL		(sum <<= 8)
#define	ADDB		(ROL, sum += *w, byte_swapped ^= 1)
#define	ADDS		(sum += *(u_short *)w)
#define	SHIFT(n)	(w += (n), mlen -= (n))
#define	ADDCARRY	do { REDUCE; REDUCE; } while (0)

static __inline__ int
in_cksum_internal(struct mbuf *m, int off, int len, u_int sum)
{
	u_char *w;
	int mlen;
	int byte_swapped = 0;

	for (; m && len; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		w = mtod(m, u_char *) + off;
		mlen = m->m_len - off;
		off = 0;
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
	return (0xffff ^ sum);
}

int
in_cksum(struct mbuf *m, int len)
{
	return (in_cksum_internal(m, 0, len, 0));
}

int
in4_cksum(struct mbuf *m, uint8_t nxt, int off, int len)
{
	u_int16_t *w;
	u_int sum = 0;
	struct ipovly ipov;

	if (nxt != 0) {
		/* pseudo header */
		bzero(&ipov, sizeof(ipov));
		ipov.ih_len = htons(len);
		ipov.ih_pr = nxt; 
		ipov.ih_src = mtod(m, struct ip *)->ip_src; 
		ipov.ih_dst = mtod(m, struct ip *)->ip_dst;
		w = (u_int16_t *)&ipov;
		/* assumes sizeof(ipov) == 20 */
		sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3]; sum += w[4];
		sum += w[5]; sum += w[6]; sum += w[7]; sum += w[8]; sum += w[9];
	}

	/* skip unnecessary part */
	while (m && off > 0) {
		if (m->m_len > off)
			break;
		off -= m->m_len;
		m = m->m_next;
	}

	return (in_cksum_internal(m, off, len, sum));
}
