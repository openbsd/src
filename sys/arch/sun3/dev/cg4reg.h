/*	$NetBSD: cg4reg.h,v 1.3 1996/10/29 19:54:21 gwr Exp $	*/

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
 *	@(#)cgthreereg.h	8.2 (Berkeley) 10/30/93
 */

/*
 * Size that can be mapped (user-level mmap).
 */
#define	CG4_OVERLAY_SIZE	 0x20000	/* size of overlay plane */
#define	CG4_ENABLE_SIZE 	 0x20000	/* size of enable plane */
#define	CG4_PIXMAP_SIZE		0x100000	/* size of frame buffer */

/* number of colormap entries */
#define CG4_CMAP_ENTRIES	256

/*
 * There are two kinds of cg4 hardware:
 * "Type A" has a AMD DACs (Digital-to-Analog Converters)
 * "Type B" has a Brooktree DACs.  H/W addresses differ too.
 */
#define CG4_TYPE_A 0
#define CG4_TYPE_B 1

/*
 * Memory layout of the Type A hardware (OBMEM)
 */
#define CG4A_DEF_BASE     0xFE400000	/* Sun3/110 */
#define CG4A_OFF_ENABLE 	0
#define CG4A_OFF_PIXMAP		0x400000
#define CG4A_OFF_OVERLAY	0xC00000
#define CG4A_OBIO_CMAP		0x0E0000	/* OBIO space! */

/* colormap/status register structure */
struct amd_regs {
	u_char r[CG4_CMAP_ENTRIES];
	u_char g[CG4_CMAP_ENTRIES];
	u_char b[CG4_CMAP_ENTRIES];
	u_char status;
#define CG4A_STATUS_FIRSTHALF	0x80
#define CG4A_STATUS_TOOLATE	0x40
};


/*
 * Memory layout of the Type B hardware (OBMEM)
 * Appears on the Sun3/60 at base 0xFF200000
 */
#define CG4B_DEF_BASE     0xFF200000	/* Sun3/60 */
#define CG4B_OFF_CMAP		0
#define CG4B_OFF_OVERLAY	0x200000
#define CG4B_OFF_ENABLE 	0x400000
#define CG4B_OFF_PIXMAP		0x600000

