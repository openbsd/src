/*	$OpenBSD: sfbreg.h,v 1.5 2000/03/03 00:54:55 todd Exp $	*/
/*-
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	from: @(#)sfb.c	8.1 (Berkeley) 6/10/93
 */

#define	SFB_OFFSET_VRAM		0x201000	/* from module's base */
#define SFB_OFFSET_BT459	0x1C0000	/* Bt459 registers */
#define SFB_ASIC_OFFSET		0x100000	/* SFB ASIC registers... */
#define  SFB_COPY_REG0		0x100000	/* Copy Buffer Register0 */
#define  SFB_COPY_REG1		0x100004	/* Copy Buffer Register1 */
#define  SFB_COPY_REG2		0x100008	/* Copy Buffer Register2 */
#define  SFB_COPY_REG3		0x10000C	/* Copy Buffer Register3 */
#define  SFB_COPY_REG4		0x100010	/* Copy Buffer Register4 */
#define  SFB_COPY_REG5		0x100014	/* Copy Buffer Register5 */
#define  SFB_COPY_REG6		0x100018	/* Copy Buffer Register6 */
#define  SFB_COPY_REG7		0x10001C	/* Copy Buffer Register7 */
#define  SFB_FOREGROUND		0x100020	/* Foreground */
#define  SFB_BACKGROUND		0x100024	/* Background */
#define  SFB_PLANEMASK		0x100028	/* PlaneMask */
#define  SFB_PIXELMASK		0x10002C	/* PixelMask Register */
#define  SFB_MODE		0x100030	/* Mode Register */
#define  SFB_BOOL_OP		0x100034	/* Boolean Op Register */
#define  SFB_PIXELSHIFT		0x100038	/* PixelShift Register */
#define  SFB_ADDRESS		0x10003C	/* Address Register */
#define  SFB_BRESENHAM1		0x100040	/* Bresenham Register 1 */
#define  SFB_BRESENHAM2		0x100044	/* Bresenham Register 2 */
#define  SFB_BRESENHAM3		0x100048	/* Bresenham Register 3 */
#define  SFB_BCONT		0x10004C	/* BCont */
#define  SFB_DEEP		0x100050	/* Deep Register */
#define  SFB_START		0x100054	/* Start Register */
#define  SFB_CLEAR		0x100058	/* Clear Interrupt */
#define  SFB_VREFRESH_COUNT	0x100060	/* Video Refresh Count */
#define  SFB_VHORIZONTAL	0x100064	/* Video Horizontal Setup */
#define  SFB_VVERTICAL		0x100068	/* Video Vertical Setup */
#define  SFB_VBASE		0x10006C	/* Video Base Address */
#define  SFB_VVALID		0x100070	/* Video Valid */
#define  SFB_INTERRUPT_ENABLE	0x100074	/* Enable/disable interrupts */
#define  SFB_TCCLK		0x100078	/* TCCLK count */
#define  SFB_VIDCLK		0x10007C	/* VIDCLK count */
#define SFB_FB_SIZE		0x1FF000	/* frame buffer size */

