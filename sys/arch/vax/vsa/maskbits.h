/*	$OpenBSD: maskbits.h,v 1.1 2006/08/05 10:00:30 miod Exp $	*/

/*-
 * Copyright (c) 1994
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
 *	@(#)maskbits.h	8.2 (Berkeley) 3/21/94
 */

/*
 * Derived from X11R4
 */

/* the following notes use the following conventions:
SCREEN LEFT				SCREEN RIGHT
in this file and maskbits.c, left and right refer to screen coordinates,
NOT bit numbering in registers.

rasops_lmask[n]
	bits[0,n-1] = 0	bits[n,31] = 1
rasops_rmask[n] =
	bits[0,n-1] = 1	bits[n,31] = 0

maskbits(x, w, startmask, endmask, nlw)
	for a span of width w starting at position x, returns
a mask for ragged bits at start, mask for ragged bits at end,
and the number of whole longwords between the ends.

*/

#define maskbits(x, w, startmask, endmask, nlw)				\
do {									\
	startmask = rasops_lmask[(x) & 0x1f];				\
	endmask = rasops_rmask[((x) + (w)) & 0x1f];			\
	if (startmask)							\
		nlw = (((w) - (32 - ((x) & 0x1f))) >> 5);		\
	else								\
		nlw = (w) >> 5;						\
} while (0)

#define	FASTGETBITS(psrc,x,w,dst)					\
	__asm__ ("extzv %1,%2,%3,%0"					\
		: "=g" (dst)						\
		: "g" (x), "g" (w), "m" (*(char *)(psrc)))

#define	FASTPUTBITS(src, x, w, pdst)					\
	__asm__ ("insv %3,%1,%2,%0"					\
		: "=m" (*(char *)(pdst))				\
		: "g" (x), "g" (w), "g" (src))

#define	RR_CLEAR	0x00
#define	RR_SET		0x01
#define	RR_COPY		0x02

#define getandputrop(psrc, srcbit, dstbit, width, pdst, rop)		\
do {									\
	unsigned int _tmpdst;						\
	switch (rop) {							\
	case RR_CLEAR:							\
		_tmpdst = 0;						\
		break;							\
	case RR_SET:							\
		_tmpdst = ~0;						\
		break;							\
	default:							\
		FASTGETBITS(psrc, srcbit, width, _tmpdst);		\
		break;							\
	}								\
	FASTPUTBITS(_tmpdst, dstbit, width, pdst);			\
} while (0)

#define getunalignedword(psrc, x, dst)					\
do {									\
        int _tmp;							\
        FASTGETBITS(psrc, x, 32, _tmp);					\
        dst = _tmp;							\
} while (0)
