/*	$OpenBSD: dc503reg.h,v 1.1 2006/07/23 19:17:23 miod Exp $	*/
/*	$NetBSD: pmreg.h,v 1.7 2005/12/11 12:18:36 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)pmreg.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Layout of the programmable cursor chip (DC503) registers.
 * All registers are 16 bit wide.
 */

#define	PCC_CMD		0x00	/* cursor command register */
#define	PCCCMD_TEST	0x8000
#define	PCCCMD_HSHI	0x4000
#define	PCCCMD_VBHI	0x2000
#define	PCCCMD_LODSA	0x1000
#define	PCCCMD_FORG2	0x0800
#define	PCCCMD_ENRG2	0x0400
#define	PCCCMD_FORG1	0x0200
#define	PCCCMD_ENRG1	0x0100
#define	PCCCMD_XHWID	0x0080
#define	PCCCMD_XHCL1	0x0040
#define	PCCCMD_XHCLP	0x0020
#define	PCCCMD_XHAIR	0x0010
#define	PCCCMD_FOPB	0x0008
#define	PCCCMD_ENPB	0x0004
#define	PCCCMD_FOPA	0x0002
#define	PCCCMD_ENPA	0x0001

#define	PCC_XPOS	0x04	/* cursor X position */
#define	PCC_YPOS	0x08	/* cursor Y position */
#define	PCC_XMIN_1	0x0c	/* region 1 left edge */
#define	PCC_XMAX_1	0x10	/* region 1 right edge */
#define	PCC_YMIN_1	0x14	/* region 1 top edge */
#define	PCC_YMAX_1	0x18	/* region 1 bottom edge */
#define	PCC_XMIN_2	0x1c	/* region 2 left edge */
#define	PCC_XMAX_2	0x20	/* region 2 right edge */
#define	PCC_YMIN_2	0x24	/* region 2 top edge */
#define	PCC_YMAX_2	0x28	/* region 2 bottom edge */
#define	PCC_LOAD	0x2c	/* cursor pattern load */

struct dc503reg {
	volatile u_int16_t	cmdr;
	int16_t	pad1;
	volatile u_int16_t	xpos;
	int16_t	pad2;
	volatile u_int16_t	ypos;
	int16_t	pad3;
	volatile u_int16_t	xmin1;
	int16_t	pad4;
	volatile u_int16_t	xmax1;
	int16_t	pad5;
	volatile u_int16_t	ymin1;
	int16_t	pad6;
	volatile u_int16_t	ymax1;
	int16_t	pad7[9];
	volatile u_int16_t	xmin2;
	int16_t	pad8;
	volatile u_int16_t	xmax2;
	int16_t	pad9;
	volatile u_int16_t	ymin2;
	int16_t	pad10;
	volatile u_int16_t	ymax2;
	int16_t	pad11;
	volatile u_int16_t	load;
};

#define	PCC_CURSOR_SIZE	16
