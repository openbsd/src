/*	$OpenBSD: ipx_cksum.c,v 1.3 2003/06/02 23:27:47 millert Exp $	*/

/*-
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
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
 * from NetBSD: in_cksum.c	1.8	95/05/01
 * from tahoe:	in_cksum.c	1.2	86/01/05
 *	@(#)in_cksum.c	1.3 (Berkeley) 1/19/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <netipx/ipx.h>

/*
 * Checksum routine for Internet Protocol family headers.
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 * 
 * This implementation is 386 version.
 */

#define REDUCE          {sum = (sum & 0xffff) + (sum >> 16);}
#define	ADDCARRY	{if (sum > 0xffff) sum -= 0xffff;}
#define	SWAP		{sum <<= 8;}
#define	ADVANCE(x)	{w += x; mlen -= x;}

/*
 * Thanks to gcc we don't have to guess
 * which registers contain sum & w.
 */
#define	Asm	__asm __volatile
#define ADD(n)  Asm("addl " #n "(%2),%0" : "=r" (sum) : "0" (sum), "r" (w))
#define ADC(n)  Asm("adcl " #n "(%2),%0" : "=r" (sum) : "0" (sum), "r" (w))
#define MOP     Asm("adcl $0,%0" :         "=r" (sum) : "0" (sum))
#define	UNSWAP	Asm("roll $8,%0" :	   "=r" (sum) : "0" (sum))
#define	ADDBYTE	{sum += *w; SWAP; byte_swapped ^= 1;}
#define	ADDWORD	{sum += *(u_short *)w;}

u_short
ipx_cksum(m, len)
	register struct mbuf *m;
	register int len;
{
	register u_char *w;
	register unsigned sum = 0;
	register int mlen = 0;
	int byte_swapped = 0;

	for (; m && len; m = m->m_next) {
		mlen = m->m_len;
		if (mlen == 0)
			continue;
		w = mtod(m, u_char *);
		if (len < mlen)
			mlen = len;
		len -= mlen;
		if (mlen < 16)
			goto short_mbuf;
		/*
		 * Force to long boundary so we do longword aligned
		 * memory operations
		 */
		if ((3 & (long)w) != 0) {
			REDUCE;
			if ((1 & (long)w) != 0) {
				ADDBYTE;
				ADVANCE(1);
			}
			if ((2 & (long)w) != 0) {
				ADDWORD;
				ADVANCE(2);
			}
		}
		/*
		 * Align 4 bytes past a 16-byte cache line boundary.
		 */
		if ((4 & (long)w) == 0) {
			ADD(0);
			MOP;
			ADVANCE(4);
		}
		if ((8 & (long)w) != 0) {
			ADD(0);  ADC(4);
			MOP;
			ADVANCE(8);
		}
		/*
		 * Do as much of the checksum as possible 32 bits at at time.
		 * In fact, this loop is unrolled to make overhead from
		 * branches &c small.
		 */
		while ((mlen -= 32) >= 0) {
			/*
			 * Add with carry 16 words and fold in the last carry
			 * by adding a 0 with carry.
			 *
			 * We aligned the pointer above so that the out-of-
			 * order operations will cause the next cache line to
			 * be preloaded while we finish with the current one.
			 */
			ADD(12); ADC(0);  ADC(4);  ADC(8);
			ADC(28); ADC(16); ADC(20); ADC(24);
			MOP;
			w += 32;
		}
		mlen += 32;
		if (mlen >= 16) {
			ADD(12); ADC(0);  ADC(4);  ADC(8);
			MOP;
			ADVANCE(16);
		}
	short_mbuf:
		if (mlen >= 8) {
			ADD(0);  ADC(4);
			MOP;
			ADVANCE(8);
		}
		if (mlen >= 4) {
			ADD(0);
			MOP;
			ADVANCE(4);
		}
		if (mlen > 0) {
			REDUCE;
			if (mlen >= 2) {
				ADDWORD;
				ADVANCE(2);
			}
			if (mlen >= 1) {
				ADDBYTE;
			}
		}
	}

	if (len)
		printf("cksum: out of data\n");
	if (byte_swapped) {
		UNSWAP;
	}
	REDUCE;
	ADDCARRY;
	return (sum ^ 0xffff);
}

