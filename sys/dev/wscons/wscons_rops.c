/* $OpenBSD: wscons_rops.c,v 1.8 2001/08/14 19:26:48 maja Exp $ */
/* $NetBSD: wscons_rops.c,v 1.6 2000/03/30 12:45:44 augustss Exp $ */

/*
 * Copyright (c) 1991, 1993
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
 *	@(#)rcons_subr.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/device.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

/*
 * Paint (or unpaint) the cursor.
 * Pays no lip service to hardware cursors.
 */
void
rcons_cursor(id, on, row, col)
	void *id;
	int on, row, col;
{
	struct rcons *rc = id;
	int x, y;

	/* turn the cursor off */
	if (!on) {
		/* make sure it's on */
		if ((rc->rc_bits & RC_CURSOR) == 0)
			return;

		row = *rc->rc_crowp;
		col = *rc->rc_ccolp;
	} else {
		/* unpaint the old copy. */
		*rc->rc_crowp = row;
		*rc->rc_ccolp = col;
	}

	x = col * rc->rc_font->width + rc->rc_xorigin;
	y = row * rc->rc_font->height + rc->rc_yorigin;

	raster_op(rc->rc_sp, x, y,
#ifdef notdef
	    /* XXX This is the right way but too slow */
	    rc->rc_font->chars[(int)' '].r->width,
	    rc->rc_font->chars[(int)' '].r->height,
#else
	    rc->rc_font->width, rc->rc_font->height,
#endif
	    RAS_INVERT,
	    (struct raster *) 0, 0, 0);

	rc->rc_bits ^= RC_CURSOR;
}

int
rcons_mapchar(id, uni, index)
	void *id;
	int uni;
	unsigned int *index;
{

	if (uni < 256) {
		*index = uni;
		return (5);
	}
	*index = ' ';
	return (0);
}

/*
 * Actually write a string to the frame buffer.
 */
void
rcons_putchar(id, row, col, uc, attr)
	void *id;
	int row, col;
	u_int uc;
	long attr;
{
	struct rcons *rc = id;
	int x, y, op;
	u_char help;

	x = col * rc->rc_font->width + rc->rc_xorigin;
	y = row * rc->rc_font->height + rc->rc_font_ascent + rc->rc_yorigin;

	op = RAS_SRC;
	if ((attr != 0) ^ ((rc->rc_bits & RC_INVERT) != 0))
		op = RAS_NOT(op);
	help = uc & 0xff;
	raster_textn(rc->rc_sp, x, y, op, rc->rc_font, &help, 1);
}

/*
 * Possibly change to white-on-black or black-on-white modes.
 */
void
rcons_invert(id, inverted)
	void *id;
	int inverted;
{
	struct rcons *rc = id;

	if (((rc->rc_bits & RC_INVERT) != 0) ^ inverted) {
		/* Invert the display */
		raster_op(rc->rc_sp, 0, 0, rc->rc_sp->width, rc->rc_sp->height,
		    RAS_INVERT, (struct raster *) 0, 0, 0);

		/* Swap things around */
		rc->rc_bits ^= RC_INVERT;
	}
}

/*
 * Copy columns (characters) in a row (line).
 */
void
rcons_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
{
	struct rcons *rc = id;
	int y, srcx, dstx, nx;

	y = rc->rc_yorigin + rc->rc_font->height * row;
	srcx = rc->rc_xorigin + rc->rc_font->width * srccol;
	dstx = rc->rc_xorigin + rc->rc_font->width * dstcol;
	nx = rc->rc_font->width * ncols;

	raster_op(rc->rc_sp, dstx, y,
	    nx, rc->rc_font->height, RAS_SRC,
	    rc->rc_sp, srcx, y);
}

/*
 * Clear columns (characters) in a row (line).
 */
void
rcons_erasecols(id, row, startcol, ncols, fillattr)
	void *id;
	int row, startcol, ncols;
	long fillattr;
{
	struct rcons *rc = id;
	int y, startx, nx, op;

	y = rc->rc_yorigin + rc->rc_font->height * row;
	startx = rc->rc_xorigin + rc->rc_font->width * startcol;
	nx = rc->rc_font->width * ncols;

	op = RAS_CLEAR;
	if ((fillattr != 0) ^ ((rc->rc_bits & RC_INVERT) != 0))
		op = RAS_NOT(op);
	raster_op(rc->rc_sp, startx, y,
	    nx, rc->rc_font->height, op,
	    (struct raster *) 0, 0, 0);
}

/*
 * Copy rows (lines).
 */
void
rcons_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct rcons *rc = id;
	int srcy, dsty, ny;

	srcy = rc->rc_yorigin + rc->rc_font->height * srcrow;
	dsty = rc->rc_yorigin + rc->rc_font->height * dstrow;
	ny = rc->rc_font->height * nrows;

	raster_op(rc->rc_sp, rc->rc_xorigin, dsty,
	    rc->rc_raswidth, ny, RAS_SRC,
	    rc->rc_sp, rc->rc_xorigin, srcy);
}

/*
 * Erase rows (lines).
 */
void
rcons_eraserows(id, startrow, nrows, fillattr)
	void *id;
	int startrow, nrows;
	long fillattr;
{
	struct rcons *rc = id;
	int starty, ny, op;

	starty = rc->rc_yorigin + rc->rc_font->height * startrow;
	ny = rc->rc_font->height * nrows;

	op = RAS_CLEAR;
	if ((fillattr != 0) ^ ((rc->rc_bits & RC_INVERT) != 0))
		op = RAS_NOT(op);
	raster_op(rc->rc_sp, rc->rc_xorigin, starty,
	    rc->rc_raswidth, ny, op,
	    (struct raster *) 0, 0, 0);
}

int
rcons_alloc_attr(id, fg, bg, flags, attrp)
	void *id;
	int fg, bg, flags;
	long *attrp;
{
	if (flags & (WSATTR_HILIT | WSATTR_BLINK |
		     WSATTR_UNDERLINE | WSATTR_WSCOLORS))
		return (EINVAL);
	if (flags & WSATTR_REVERSE)
		*attrp = 1;
	else
		*attrp = 0;
	return (0);
}
