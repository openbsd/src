/*	$NetBSD: in_cksum.c,v 1.3 1995/04/26 13:30:03 pk Exp $ */

/*
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

/*
 * Checksum routine for Internet Protocol family headers.
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 *
 * SPARC version.
 */

/*
 * This idea here is that we do as many 32 bit operations as possible
 * for maximum efficiency.  We also unroll all loops in to assembly.
 * This gains about 20% extra efficiency over the non-pipelined method.
 *
 * XXX - this code really needs further performance analysis.  At the
 * moment it has only been run on a SPARC ELC.
 */

#define Asm		__asm __volatile
#define ADD32		Asm("	ld [%2+28],%%i0; ld [%2+24],%%i1; 	\
				ld [%2+20],%%i2; ld [%2+16],%%i3; 	\
				ld [%2+12],%%i4; ld [%2+8],%%i5;	\
				ld [%2+4],%%g3; ld [%2],%%g4;		\
				addcc %0,%%i0,%0; addxcc %0,%%i1,%0;	\
				addxcc %0,%%i2,%0; addxcc %0,%%i3,%0;	\
				addxcc %0,%%i4,%0; addxcc %0,%%i5,%0;	\
				addxcc %0,%%g3,%0; addxcc %0,%%g4,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum) : "0" (sum), "r" (w)	\
				: "%i0", "%i1", "%i2", "%i3",		\
				  "%i4", "%i5", "%g3", "%g4")
#define ADD16		Asm("	ld [%2+12],%%i0; ld [%2+8],%%i1;	\
				ld [%2+4],%%i2; ld [%2],%%i3;		\
				addcc %0,%%i0,%0; addxcc %0,%%i1,%0;	\
				addxcc %0,%%i2,%0; addxcc %0,%%i3,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum) : "0" (sum), "r" (w)	\
				: "%i0", "%i1", "%i2", "%i3")
#define ADD8		Asm("	ld [%2+4],%%i0; ld [%2],%%i1;		\
				addcc %0,%%i0,%0; addxcc %0,%%i1,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum) : "0" (sum), "r" (w)	\
				: "%i0", "%i1")
#define ADD4		Asm("	ld [%2],%%i0; addcc  %0,%%i0,%0;	\
				addxcc %0,0,%0"				\
				: "=r" (sum) : "0" (sum), "r" (w)	\
				: "%i0")

#define REDUCE		{sum = (sum & 0xffff) + (sum >> 16);}
#define ADDCARRY	{if (sum > 0xffff) sum -= 0xffff;}
#define ROL		{sum = sum << 8;}	/* depends on recent REDUCE */
#define ADDB		{ROL; sum += *w; byte_swapped ^= 1;}
#define ADDS		{sum += *(u_short *)w;}
#define SHIFT(n)	{w += n; mlen -= n;}

int
in_cksum(m, len)
	register struct mbuf *m;
	register int len;
{
	register u_char *w;
	register u_int sum = 0;
	register int mlen = 0;
	int byte_swapped = 0;

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
				ADDB;
				SHIFT(1);
			}
			if ((2 & (long)w) != 0 && mlen >= 2) {
				ADDS;
				SHIFT(2);
			}
		}
		/*
		 * Do as many 32 bit operattions as possible using the
		 * 32/16/8/4 macro's above, using as many as possible of
		 * these.
		 */
		while (mlen >= 32) {
			ADD32;
			SHIFT(32);
		}
		if (mlen >= 16) {
			ADD16;
			SHIFT(16);
		}
		if (mlen >= 8) {
			ADD8;
			SHIFT(8);
		}
		if (mlen >= 4) {
			ADD4;
			SHIFT(4)
		}
		if (mlen == 0)
			continue;

		REDUCE;
		if (mlen >= 2) {
			ADDS;
			SHIFT(2);
		}
		if (mlen == 1) {
			ADDB;
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
