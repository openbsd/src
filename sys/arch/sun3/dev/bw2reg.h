/*	$NetBSD: bw2reg.h,v 1.1 1995/03/10 01:50:59 gwr Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)bwtworeg.h	8.1 (Berkeley) 6/11/93
 */

/* Default physical address. */
#define BW2_50_PADDR	  0x100000
#define	BW2_FB_PADDR	0xff000000
#define BW2_CR_PADDR	0xff1c0000

/* Sizes that can be mapped. */
#define	BW2_FBSIZE  		0x20000	/* size of frame buffer */
#define	BW2_FBSIZE_HIRES	0x40000	/* size of hi-res FB */

/*
 * bwtwo control register (16-bits)
 */

#define	VC_VIDEO_EN	0x8000	/* Video enable */
#define	VC_COPY_EN	0x4000	/* Copy enable */
#define	VC_INT_ENA	0x2000	/* Interrupt enable */
#define	VC_INT_PEND	0x1000	/* Int active - r/o */
#define	VC_B_JUMPER	0x0800	/* Config jumper, 0=default */
#define	VC_A_JUMPER	0x0400	/* Config jumper, 0=default */
#define	VC_COLOR_JP 0x0200	/* Config jumper, 0: def. 1: use P2-color */
#define	VC_1024_JP	0x0100	/* Config jumper, 0: def. 1: use 1K by 1K */
/* Note copy-mode only on Sun3/160 */
#define VC_COPYBASE	0x00FF	/* copy-mode high address bits */
