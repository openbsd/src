/*      $OpenBSD: psl.h,v 1.7 2003/06/02 23:27:57 millert Exp $      */
/*      $NetBSD: psl.h,v 1.6 1997/06/07 12:15:28 ragge Exp $      */

/*
 * Rewritten for the VAX port. Based on Berkeley code. /IC
 *
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)psl.h	7.2 (Berkeley) 5/4/91
 */

#ifndef PSL_C

/*
 * VAX program status longword
 */

#define	PSL_C		0x00000001	/* carry bit */
#define	PSL_V		0x00000002	/* overflow bit */
#define	PSL_Z		0x00000004     	/* zero bit */
#define	PSL_N		0x00000008     	/* negative bit */
#define	PSL_T		0x00000010      /* trace enable bit */
#define	PSL_IPL00	0x00000000	/* interrupt priority level 0 */
#define	PSL_IPL01	0x00010000	/* interrupt priority level 1 */
#define	PSL_IPL02	0x00020000	/* interrupt priority level 2 */
#define	PSL_IPL03	0x00030000	/* interrupt priority level 3 */
#define	PSL_IPL04	0x00040000	/* interrupt priority level 4 */
#define	PSL_IPL05	0x00050000	/* interrupt priority level 5 */
#define	PSL_IPL06	0x00060000	/* interrupt priority level 6 */
#define	PSL_IPL07	0x00070000	/* interrupt priority level 7 */
#define	PSL_IPL08	0x00080000	/* interrupt priority level 8 */
#define	PSL_IPL09	0x00090000	/* interrupt priority level 9 */
#define	PSL_IPL0A	0x000a0000	/* interrupt priority level 10 */
#define	PSL_IPL0B	0x000b0000	/* interrupt priority level 11 */
#define	PSL_IPL0C	0x000c0000	/* interrupt priority level 12 */
#define	PSL_IPL0D	0x000d0000	/* interrupt priority level 13 */
#define	PSL_IPL0E	0x000e0000	/* interrupt priority level 14 */
#define	PSL_IPL0F	0x000f0000	/* interrupt priority level 15 */
#define	PSL_IPL10	0x00100000	/* interrupt priority level 16 */
#define	PSL_IPL11	0x00110000	/* interrupt priority level 17 */
#define	PSL_IPL12	0x00120000	/* interrupt priority level 18 */
#define	PSL_IPL13	0x00130000	/* interrupt priority level 19 */
#define	PSL_IPL14	0x00140000	/* interrupt priority level 20 */
#define	PSL_IPL15	0x00150000	/* interrupt priority level 21 */
#define	PSL_IPL16	0x00160000	/* interrupt priority level 22 */
#define	PSL_IPL17	0x00170000	/* interrupt priority level 23 */
#define	PSL_IPL18	0x00180000	/* interrupt priority level 24 */
#define	PSL_IPL19	0x00190000	/* interrupt priority level 25 */
#define	PSL_IPL1A	0x001a0000	/* interrupt priority level 26 */
#define	PSL_IPL1B	0x001b0000	/* interrupt priority level 27 */
#define	PSL_IPL1C	0x001c0000	/* interrupt priority level 28 */
#define	PSL_IPL1D	0x001d0000	/* interrupt priority level 29 */
#define	PSL_IPL1E	0x001e0000	/* interrupt priority level 30 */
#define	PSL_IPL1F	0x001f0000	/* interrupt priority level 31 */
#define	PSL_PREVU	0x00c00000	/* Previous user mode */
#define	PSL_K		0x00000000	/* kernel mode */
#define	PSL_E		0x01000000     	/* executive mode */
#define	PSL_S		0x02000000     	/* supervisor mode */
#define	PSL_U		0x03000000	/* user mode */
#define	PSL_IS		0x04000000	/* interrupt stack select */
#define	PSL_FPD	        0x08000000	/* first part done flag */
#define PSL_TP          0x40000000      /* trace pending */
#define	PSL_CM		0x80000000	/* compatibility mode */

#define	PSL_LOWIPL	(PSL_K)
#define	PSL_HIGHIPL	(PSL_K | PSL_IPL1F)
#define PSL_IPL		(PSL_IPL1F)
#define	PSL_USER	(0)

#define	PSL_MBZ		0x3020ff00	/* must be zero bits */

#define	PSL_USERSET	(0)
#define	PSL_USERCLR	(PSL_S | PSL_IPL1F | PSL_MBZ)

/*
 * Macros to decode processor status word.
 */
#define	CLKF_USERMODE(framep)	((((framep)->ps) & (PSL_U)) == PSL_U)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	((((framep)->ps) & (PSL_IS)) == PSL_IS)
#define PSL2IPL(ps)             ((ps) >> 16)

#endif
