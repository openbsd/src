/*	$NetBSD: rcons.h,v 1.3 1995/10/05 13:17:51 pk Exp $ */

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
 *	@(#)fbvar.h	8.1 (Berkeley) 6/11/93
 */

#include <dev/rcons/raster.h>

struct rconsole {
	/* Raster console emulator state */

	/* This section must be filled in by the framebugger device */
	int	rc_width;
	int	rc_height;
	int	rc_depth;
	caddr_t	rc_pixels;		/* display RAM */
	int	rc_linebytes;		/* bytes per display line */
	int	rc_maxrow;		/* emulator height of screen */
	int	rc_maxcol;		/* emulator width of screen */
	void	(*rc_bell)__P((int));	/* ring the bell */
	/* The following two items may optionally be left zero */
	int	*rc_row;		/* emulator row */
	int	*rc_col;		/* emulator column */

	/* Bits maintained by the raster routines */
	u_int	rc_bits;		/* see defines below */
	int	rc_ringing;		/* bell currently ringing */
	int	rc_belldepth;		/* audible bell depth */
	int	rc_scroll;		/* stupid sun scroll mode */

	int	rc_p0;			/* escape sequence parameter 0 */
	int	rc_p1;			/* escape sequence parameter 1 */

	int	rc_emuwidth;		/* emulator screen width  */
	int	rc_emuheight;		/* emulator screen height */

	int	rc_xorigin;		/* x origin for first column */
	int	rc_yorigin;		/* y origin for first row */

	struct	raster *rc_sp;		/* frame buffer raster */
	struct	raster *rc_cursor;	/* optional cursor */
	int	rc_ras_blank;		/* current screen blank raster op */

	struct	raster_font *rc_font;	/* font and related info */
};

#define FB_INESC	0x001		/* processing an escape sequence */
#define FB_STANDOUT	0x002		/* standout mode */
/* #define FB_BOLD	0x?		/* boldface mode */
#define FB_INVERT	0x008		/* white on black mode */
#define FB_VISBELL	0x010		/* visual bell */
#define FB_CURSOR	0x020		/* cursor is visible */
#define FB_P0_DEFAULT	0x100		/* param 0 is defaulted */
#define FB_P1_DEFAULT	0x200		/* param 1 is defaulted */
#define FB_P0		0x400		/* working on param 0 */
#define FB_P1		0x800		/* working on param 1 */

extern void	rcons_cnputc __P((int));
