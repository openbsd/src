/*	$NetBSD: in_cksum.c,v 1.3 1995/04/26 13:30:03 pk Exp $ */

/*
 * Copyright (c) 1995 Zubin Dittia.
 * Copyright (c) 1995 Theo de Raadt.
 * Copyright (c) 1995 Matthew Green.
 * Copyright (c) 1994 Charles Hannum.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, and it's contributors.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
#include <sys/mbuf.h>

#include <netinet/in.h>

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
 * reduces performance considerably on Sun4m machines (because of their
 * superscaler architecture).  So I chose to leave it out.
 *
 * Zubin Dittia (zubin@dworkin.wustl.edu)
 */

#define Asm	__asm __volatile
#define ADD64		Asm("	ld [%2],%3; ld [%2+4],%4;		\
				addcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+8],%3; ld [%2+12],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+16],%3; ld [%2+20],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+24],%3; ld [%2+28],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+32],%3; ld [%2+36],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+40],%3; ld [%2+44],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+48],%3; ld [%2+52],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+56],%3; ld [%2+60],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum)				\
				: "0" (sum), "r" (w), "r" (tmp1), "r" (tmp2))
#define ADD32		Asm("	ld [%2],%3; ld [%2+4],%4;		\
				addcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+8],%3; ld [%2+12],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+16],%3; ld [%2+20],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+24],%3; ld [%2+28],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum)				\
				: "0" (sum), "r" (w), "r" (tmp1), "r" (tmp2))
#define ADD16		Asm("	ld [%2],%3; ld [%2+4],%4;		\
				addcc %0,%3,%0; addxcc %0,%4,%0;	\
				ld [%2+8],%3; ld [%2+12],%4;		\
				addxcc %0,%3,%0; addxcc %0,%4,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum)				\
				: "0" (sum), "r" (w), "r" (tmp1), "r" (tmp2))
#define ADD8		Asm("	ld [%2],%3; ld [%2+4],%4;		\
				addcc %0,%3,%0; addxcc %0,%4,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum)				\
				: "0" (sum), "r" (w), "r" (tmp1), "r" (tmp2))
#define ADD4		Asm("	ld [%2],%3; addcc  %0,%3,%0;		\
				addxcc %0,0,%0"				\
				: "=r" (sum)				\
				: "0" (sum), "r" (w), "r" (tmp1))

#define REDUCE		{sum = (sum & 0xffff) + (sum >> 16);}
#define ADDCARRY	{if (sum > 0xffff) sum -= 0xffff;}
#define ROL		{sum = sum << 8;}	/* depends on recent REDUCE */
#define ADDBYTE		{ROL; sum += *w; byte_swapped ^= 1;}
#define ADDSHORT	{sum += *(u_short *)w;}
#define ADVANCE(n)	{w += n; mlen -= n;}

int
in_cksum(m, len)
	register struct mbuf *m;
	register int len;
{
	register u_char *w;
	register u_int sum = 0;
	register int mlen = 0;
	int byte_swapped = 0;

	/*
	 * Declare two temporary registers for use by the asm code.  We
	 * allow the compiler to pick which specific machine registers to
	 * use, instead of hard-coding this in the asm code above.
	 */
	register u_int tmp1, tmp2;

	for (; m && len; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		w = mtod(m, u_char *);
		mlen = m->m_len;
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
		 * Do as many 32 bit operattions as possible using the
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
	REDUCE;
	ADDCARRY;

	return (0xffff ^ sum);
}
