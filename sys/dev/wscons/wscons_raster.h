/*	$OpenBSD: wscons_raster.h,v 1.2 1996/07/29 23:03:00 niklas Exp $ */
/*	$NetBSD: wscons_raster.h,v 1.1 1996/04/12 02:00:51 cgd Exp $ */

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

#ifndef _DEV_PSEUDO_RCONS_H_
#define _DEV_PSEUDO_RCONS_H_

struct rcons {
	/* Console raster.  Filled in before rcons_init(). */
	struct	raster *rc_sp;		/* frame buffer raster */
	int	*rc_crowp;		/* ptr to cursror row */
	int	*rc_ccolp;		/* ptr to cursror column */

	/* Number of rows/columns on raster.  Filled in by rcons_init(). */
	int	rc_maxrow;		/* emulator height of screen */
	int	rc_maxcol;		/* emulator width of screen */

	/* Font information.  Filled in by rcons_init(). */
	struct	raster_font *rc_font;	/* font and related info */
	int	rc_font_ascent;		/* distance from font to char origin */

	/* Raster console state.  Filled in by rcons_init(). */
	u_int	rc_bits;		/* see defines below */
	int	rc_xorigin;		/* x origin for first column */
	int	rc_yorigin;		/* y origin for first row */
	int	rc_raswidth;		/* raster width for row copies */
	int	rc_ras_blank;		/* current screen blank raster op */

	/* Internal cursor row and column.  XXX  Weird Sun cursor pointers. */
	int	rc_crow;		/* internal cursror row */
	int	rc_ccol;		/* ptr to cursror column */
};

#define RC_STANDOUT	0x001		/* standout mode */
/* #define RC_BOLD	0x?		/* boldface mode */
#define RC_INVERT	0x002		/* inverted screen colors */
#define	RC_CURSOR	0x004		/* cursor currently displayed */

/* Initialization functions. */
void	rcons_init __P((struct rcons *rc, int mrow, int mcol));

/* Console emulation interface functions.  See ansicons.h for more info. */
void	rcons_cursor __P((void *, int, int, int));
void	rcons_invert __P((void *, int));
void	rcons_putstr __P((void *, int, int, char *, int));
void	rcons_copycols __P((void *, int, int, int, int));
void	rcons_erasecols __P((void *, int, int, int));
void	rcons_copyrows __P((void *, int, int, int));
void	rcons_eraserows __P((void *, int, int));

#endif /* _DEV_PSEUDO_RCONS_H_ */
