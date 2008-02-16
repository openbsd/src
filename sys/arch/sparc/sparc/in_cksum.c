/*	$OpenBSD: in_cksum.c,v 1.12 2008/02/16 23:02:41 miod Exp $	*/
/*	$NetBSD: in_cksum.c,v 1.7 1996/10/05 23:44:34 mrg Exp $ */

/*
 * Copyright (c) 1995 Zubin Dittia.
 * Copyright (c) 1995 Matthew R. Green.
 * Copyright (c) 1994 Charles Hannum.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in_cksum.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

/*
 * Checksum routine for Internet Protocol family headers.
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 *
 * SPARC version.
 */

/*
 * The checksum computation code here is significantly faster than its
 * vanilla C counterpart (by significantly, I mean 2-3 times faster if
 * the data is in cache, and 1.5-2 times faster if the data is not in
 * cache).
 * We optimize on three fronts:
 *	1. By using the add-with-carry (addxcc) instruction, we can use
 *	   32-bit operations instead of 16-bit operations.
 *	2. By unrolling the main loop to reduce branch overheads.
 *	3. By doing a sequence of load,load,add,add,load,load,add,add,
 *	   we can avoid the extra stall cycle which is incurred if the
 *	   instruction immediately following a load tries to use the
 *	   target register of the load.
 * Another possible optimization is to replace a pair of 32-bit loads
 * with a single 64-bit load (ldd) instruction, but I found that although
 * this improves performance somewhat on Sun4c machines, it actually
 * reduces performance considerably on Sun4m machines (I don't know why).
 * So I chose to leave it out.
 *
 * Zubin Dittia (zubin@dworkin.wustl.edu)
 */

#define Asm	__asm __volatile
#define ADD64		Asm("	ld [%4+ 0],%1;   ld [%4+ 4],%2;		\
				addcc  %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+ 8],%1;   ld [%4+12],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+16],%1;   ld [%4+20],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+24],%1;   ld [%4+28],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+32],%1;   ld [%4+36],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+40],%1;   ld [%4+44],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+48],%1;   ld [%4+52],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+56],%1;   ld [%4+60],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum), "=&r" (tmp1), "=&r" (tmp2)\
				: "0" (sum), "r" (w))
#define ADD32		Asm("	ld [%4+ 0],%1;   ld [%4+ 4],%2;		\
				addcc  %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+ 8],%1;   ld [%4+12],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+16],%1;   ld [%4+20],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+24],%1;   ld [%4+28],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum), "=&r" (tmp1), "=&r" (tmp2)\
				: "0" (sum), "r" (w))
#define ADD16		Asm("	ld [%4+ 0],%1;   ld [%4+ 4],%2;		\
				addcc  %0,%1,%0; addxcc %0,%2,%0;	\
				ld [%4+ 8],%1;   ld [%4+12],%2;		\
				addxcc %0,%1,%0; addxcc %0,%2,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum), "=&r" (tmp1), "=&r" (tmp2)\
				: "0" (sum), "r" (w))
#define ADD8		Asm("	ld [%4+ 0],%1;   ld [%4+ 4],%2;		\
				addcc  %0,%1,%0; addxcc %0,%2,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum), "=&r" (tmp1), "=&r" (tmp2)\
				: "0" (sum), "r" (w))
#define ADD4		Asm("	ld [%3+ 0],%1; 				\
				addcc  %0,%1,%0;			\
				addxcc %0,0,%0"				\
				: "=r" (sum), "=&r" (tmp1)		\
				: "0" (sum), "r" (w))

#define REDUCE		{sum = (sum & 0xffff) + (sum >> 16);}
#define ROL		{sum = sum << 8;}	/* depends on recent REDUCE */
#define ADDBYTE		{ROL; sum += *w; byte_swapped ^= 1;}
#define ADDSHORT	{sum += *(u_short *)w;}
#define ADVANCE(n)	{w += n; mlen -= n;}

static __inline__ int
in_cksum_internal(struct mbuf *m, int off, int len, u_int sum)
{
	u_char *w;
	int mlen = 0;
	int byte_swapped = 0;

	/*
	 * Declare two temporary registers for use by the asm code.  We
	 * allow the compiler to pick which specific machine registers to
	 * use, instead of hard-coding this in the asm code above.
	 */
	u_int tmp1, tmp2;

	for (; m && len; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		w = mtod(m, u_char *) + off;
		mlen = m->m_len - off;
		off = 0;
		if (len < mlen)
			mlen = len;
		len -= mlen;

		/*
		 * Ensure that we're aligned on a word boundary here so
		 * that we can do 32 bit operations below.
		 */
		if ((3 & (long)w) != 0) {
			REDUCE;
			if ((1 & (long)w) != 0 && mlen >= 1) {
				ADDBYTE;
				ADVANCE(1);
			}
			if ((2 & (long)w) != 0 && mlen >= 2) {
				ADDSHORT;
				ADVANCE(2);
			}
		}

		/*
		 * Do as many 32 bit operations as possible using the
		 * 64/32/16/8/4 macro's above, using as many as possible of
		 * these.
		 */
		while (mlen >= 64) {
			ADD64;
			ADVANCE(64);
		}
		if (mlen >= 32) {
			ADD32;
			ADVANCE(32);
		}
		if (mlen >= 16) {
			ADD16;
			ADVANCE(16);
		}
		if (mlen >= 8) {
			ADD8;
			ADVANCE(8);
		}
		if (mlen >= 4) {
			ADD4;
			ADVANCE(4)
		}
		if (mlen == 0)
			continue;

		REDUCE;
		if (mlen >= 2) {
			ADDSHORT;
			ADVANCE(2);
		}
		if (mlen == 1) {
			ADDBYTE;
		}
	}
	if (byte_swapped) {
		REDUCE;
		ROL;
	}
	/* Two REDUCEs is faster than REDUCE1; if (sum > 65535) sum -= 65535; */
	REDUCE;
	REDUCE;

	return (0xffff ^ sum);
}

int
in_cksum(struct mbuf *m, int len)
{

	return (in_cksum_internal(m, 0, len, 0));
}

int
in4_cksum(struct mbuf *m, u_int8_t nxt, int off, int len)
{
	u_char *w;
	u_int sum = 0;
	struct ipovly ipov;

	/*
	 * Declare two temporary registers for use by the asm code.  We
	 * allow the compiler to pick which specific machine registers to
	 * use, instead of hard-coding this in the asm code above.
	 */
	u_int tmp1, tmp2;

	if (nxt != 0) {
		/* pseudo header */
		memset(&ipov, 0, sizeof(ipov));
		ipov.ih_len = htons(len);
		ipov.ih_pr = nxt;
		ipov.ih_src = mtod(m, struct ip *)->ip_src;
		ipov.ih_dst = mtod(m, struct ip *)->ip_dst;
		w = (u_char *)&ipov;
		/* assumes sizeof(ipov) == 20 */
		ADD16;
		w += 16;
		ADD4;
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
