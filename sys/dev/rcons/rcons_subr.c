/*	$NetBSD: rcons_subr.c,v 1.2 1995/10/04 23:57:26 pk Exp $ */

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

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/device.h>
#else
#include <sys/types.h>
#include "myfbdevice.h"
#endif

#include <dev/rcons/rcons.h>
#include <dev/rcons/raster.h>

#include "rcons_subr.h"

extern void rcons_bell(struct rconsole *);

#define RCONS_ISPRINT(c) ((((c) >= ' ') && ((c) <= '~')) || ((c) > 160))
#define RCONS_ISDIGIT(c) ((c) >= '0' && (c) <= '9')

/* Output (or at least handle) a string sent to the console */
void
rcons_puts(rc, str, n)
	register struct rconsole *rc;
	register unsigned char *str;
	register int n;
{
	register int c, i, j;
	register unsigned char *cp;

	/* Jump scroll */
	/* XXX maybe this should be an option? */
	if ((rc->rc_bits & FB_INESC) == 0) {
		/* Count newlines up to an escape sequence */
		i = 0;
		j = 0;
		for (cp = str; j++ < n && *cp != '\033'; ++cp) {
			if (*cp == '\n')
				++i;
			else if (*cp == '\013')
				--i;
		}

		/* Only jump scroll two or more rows */
		if (*rc->rc_row + i >= rc->rc_maxrow + 1) {
			/* Erase the cursor (if necessary) */
			if (rc->rc_bits & FB_CURSOR)
				rcons_cursor(rc);

			rcons_scroll(rc, i);
		}
	}

	/* Process characters */
	while (--n >= 0) {
		c = *str;
		if (c == '\033') {
			/* Start an escape (perhaps aborting one in progress) */
			rc->rc_bits |= FB_INESC | FB_P0_DEFAULT | FB_P1_DEFAULT;
			rc->rc_bits &= ~(FB_P0 | FB_P1);

			/* Most parameters default to 1 */
			rc->rc_p0 = rc->rc_p1 = 1;
		} else if (rc->rc_bits & FB_INESC) {
			rcons_esc(rc, c);
		} else {
			/* Erase the cursor (if necessary) */
			if (rc->rc_bits & FB_CURSOR)
				rcons_cursor(rc);

			/* Display the character */
			if (RCONS_ISPRINT(c)) {
				/* Try to output as much as possible */
				j = rc->rc_maxcol - (*rc->rc_col + 1);
				if (j > n)
					j = n;
				for (i = 1; i < j && RCONS_ISPRINT(str[i]); ++i)
					continue;
				rcons_text(rc, str, i);
				--i;
				str += i;
				n -= i;
			} else
				rcons_pctrl(rc, c);
		}
		++str;
	}
	/* Redraw the cursor (if necessary) */
	if ((rc->rc_bits & FB_CURSOR) == 0)
		rcons_cursor(rc);
}

/* Actually write a string to the frame buffer */
void
rcons_text(rc, str, n)
	register struct rconsole *rc;
	register unsigned char *str;
	register int n;
{
	register int x, y, op;

	x = *rc->rc_col * rc->rc_font->width + rc->rc_xorigin;
	y = *rc->rc_row * rc->rc_font->height +
	    rc->rc_font->ascent + rc->rc_yorigin;
	op = RAS_SRC;
	if (((rc->rc_bits & FB_STANDOUT) != 0) ^
	    ((rc->rc_bits & FB_INVERT) != 0))
		op = RAS_NOT(op);
	raster_textn(rc->rc_sp, x, y, op, rc->rc_font, str, n);
	*rc->rc_col += n;
	if (*rc->rc_col >= rc->rc_maxcol) {
		*rc->rc_col = 0;
		(*rc->rc_row)++;
	}
	if (*rc->rc_row >= rc->rc_maxrow)
		rcons_scroll(rc, 1);
}

/* Handle a control character sent to the console */
void
rcons_pctrl(rc, c)
	register struct rconsole *rc;
	register int c;
{

	switch (c) {

	case '\r':	/* Carriage return */
		*rc->rc_col = 0;
		break;

	case '\b':	/* Backspace */
		if (*rc->rc_col > 0)
			(*rc->rc_col)--;
		break;

	case '\013':	/* Vertical tab */
		if (*rc->rc_row > 0)
			(*rc->rc_row)--;
		break;

	case '\f':	/* Formfeed */
		*rc->rc_row = *rc->rc_col = 0;
		rcons_clear2eop(rc);
		break;

	case '\n':	/* Linefeed */
		(*rc->rc_row)++;
		if (*rc->rc_row >= rc->rc_maxrow)
			rcons_scroll(rc, 1);
		break;

	case '\007':	/* Bell */
		rcons_bell(rc);
		break;

	case '\t':	/* Horizontal tab */
		*rc->rc_col = (*rc->rc_col + 8) & ~7;
		if (*rc->rc_col >= rc->rc_maxcol)
			*rc->rc_col = rc->rc_maxcol - 1;
		break;
	}
}

/* Handle the next character in an escape sequence */
void
rcons_esc(rc, c)
	register struct rconsole *rc;
	register int c;
{

	if (c == '[') {
		/* Parameter 0 */
		rc->rc_bits &= ~FB_P1;
		rc->rc_bits |= FB_P0;
	} else if (c == ';') {
		/* Parameter 1 */
		rc->rc_bits &= ~FB_P0;
		rc->rc_bits |= FB_P1;
	} else if (RCONS_ISDIGIT(c)) {
		/* Add a digit to a parameter */
		if (rc->rc_bits & FB_P0) {
			/* Parameter 0 */
			if (rc->rc_bits & FB_P0_DEFAULT) {
				rc->rc_bits &= ~FB_P0_DEFAULT;
				rc->rc_p0 = 0;
			}
			rc->rc_p0 *= 10;
			rc->rc_p0 += c - '0';
		} else if (rc->rc_bits & FB_P1) {
			/* Parameter 1 */
			if (rc->rc_bits & FB_P1_DEFAULT) {
				rc->rc_bits &= ~FB_P1_DEFAULT;
				rc->rc_p1 = 0;
			}
			rc->rc_p1 *= 10;
			rc->rc_p1 += c - '0';
		}
	} else {
		/* Erase the cursor (if necessary) */
		if (rc->rc_bits & FB_CURSOR)
			rcons_cursor(rc);

		/* Process the completed escape sequence */
		rcons_doesc(rc, c);
		rc->rc_bits &= ~FB_INESC;
	}
}

/* Process a complete escape sequence */
void
rcons_doesc(rc, c)
	register struct rconsole *rc;
	register int c;
{

#ifdef notdef
	/* XXX add escape sequence to enable visual (and audible) bell */
	rc->rc_bits = FB_VISBELL;
#endif

	switch (c) {

	case '@':
		/* Insert Character (ICH) */
		rcons_insertchar(rc, rc->rc_p0);
		break;

	case 'A':
		/* Cursor Up (CUU) */
		*rc->rc_row -= rc->rc_p0;
		if (*rc->rc_row < 0)
			*rc->rc_row = 0;
		break;

	case 'B':
		/* Cursor Down (CUD) */
		*rc->rc_row += rc->rc_p0;
		if (*rc->rc_row >= rc->rc_maxrow)
			*rc->rc_row = rc->rc_maxrow - 1;
		break;

	case 'C':
		/* Cursor Forward (CUF) */
		*rc->rc_col += rc->rc_p0;
		if (*rc->rc_col >= rc->rc_maxcol)
			*rc->rc_col = rc->rc_maxcol - 1;
		break;

	case 'D':
		/* Cursor Backward (CUB) */
		*rc->rc_col -= rc->rc_p0;
		if (*rc->rc_col < 0)
			*rc->rc_col = 0;
		break;

	case 'E':
		/* Cursor Next Line (CNL) */
		*rc->rc_col = 0;
		*rc->rc_row += rc->rc_p0;
		if (*rc->rc_row >= rc->rc_maxrow)
			*rc->rc_row = rc->rc_maxrow - 1;
		break;

	case 'f':
		/* Horizontal And Vertical Position (HVP) */
	case 'H':
		/* Cursor Position (CUP) */
		*rc->rc_col = rc->rc_p1 - 1;
		if (*rc->rc_col < 0)
			*rc->rc_col = 0;
		else if (*rc->rc_col >= rc->rc_maxcol)
			*rc->rc_col = rc->rc_maxcol - 1;

		*rc->rc_row = rc->rc_p0 - 1;
		if (*rc->rc_row < 0)
			*rc->rc_row = 0;
		else if (*rc->rc_row >= rc->rc_maxrow)
			*rc->rc_row = rc->rc_maxrow - 1;
		break;

	case 'J':
		/* Erase in Display (ED) */
		rcons_clear2eop(rc);
		break;

	case 'K':
		/* Erase in Line (EL) */
		rcons_clear2eol(rc);
		break;

	case 'L':
		/* Insert Line (IL) */
		rcons_insertline(rc, rc->rc_p0);
		break;

	case 'M':
		/* Delete Line (DL) */
		rcons_delline(rc, rc->rc_p0);
		break;

	case 'P':
		/* Delete Character (DCH) */
		rcons_delchar(rc, rc->rc_p0);
		break;

	case 'm':
		/* Select Graphic Rendition (SGR); */
		/* (defaults to zero) */
		if (rc->rc_bits & FB_P0_DEFAULT)
			rc->rc_p0 = 0;
		if (rc->rc_p0)
			rc->rc_bits |= FB_STANDOUT;
		else
			rc->rc_bits &= ~FB_STANDOUT;
		break;

	case 'p':
		/* Black On White (SUNBOW) */
		rcons_invert(rc, 0);
		break;

	case 'q':
		/* White On Black (SUNWOB) */
		rcons_invert(rc, 1);
		break;

	case 'r':
		/* Set scrolling (SUNSCRL) */
		/* (defaults to zero) */
		if (rc->rc_bits & FB_P0_DEFAULT)
			rc->rc_p0 = 0;
		/* XXX not implemented yet */
		rc->rc_scroll = rc->rc_p0;
		break;

	case 's':
		/* Reset terminal emulator (SUNRESET) */
		rc->rc_bits &= ~FB_STANDOUT;
		rc->rc_scroll = 0;
		if (rc->rc_bits & FB_INVERT)
			rcons_invert(rc, 0);
		break;
	}
}

/* Paint (or unpaint) the cursor */
void
rcons_cursor(rc)
	register struct rconsole *rc;
{
	register int x, y;

	x = *rc->rc_col * rc->rc_font->width + rc->rc_xorigin;
	y = *rc->rc_row * rc->rc_font->height + rc->rc_yorigin;
	raster_op(rc->rc_sp, x, y,
#ifdef notdef
	    /* XXX This is the right way but too slow */
	    rc->rc_font->chars[(int)' '].r->width,
	    rc->rc_font->chars[(int)' '].r->height,
#else
	    rc->rc_font->width, rc->rc_font->height,
#endif
	    RAS_INVERT, (struct raster *) 0, 0, 0);
	rc->rc_bits ^= FB_CURSOR;
}

/* Possibly change to SUNWOB or SUNBOW mode */
void
rcons_invert(rc, wob)
	struct rconsole *rc;
	int wob;
{
	if (((rc->rc_bits & FB_INVERT) != 0) ^ wob) {
		/* Invert the display */
		raster_op(rc->rc_sp, 0, 0, rc->rc_sp->width, rc->rc_sp->height,
		    RAS_INVERT, (struct raster *) 0, 0, 0);

		/* Swap things around */
		rc->rc_ras_blank = RAS_NOT(rc->rc_ras_blank);
		rc->rc_bits ^= FB_INVERT;
	}
}

/* Clear to the end of the page */
void
rcons_clear2eop(rc)
	register struct rconsole *rc;
{
	register int y;

	if (*rc->rc_col == 0 && *rc->rc_row == 0) {
		/* Clear the entire frame buffer */
		raster_op(rc->rc_sp, 0, 0,
		    rc->rc_sp->width, rc->rc_sp->height,
		    rc->rc_ras_blank, (struct raster *) 0, 0, 0);
	} else {
		/* Only clear what needs to be cleared */
		rcons_clear2eol(rc);
		y = (*rc->rc_row + 1) * rc->rc_font->height;

		raster_op(rc->rc_sp, rc->rc_xorigin, rc->rc_yorigin + y,
		    rc->rc_emuwidth, rc->rc_emuheight - y,
		    rc->rc_ras_blank, (struct raster *) 0, 0, 0);
	}
}

/* Clear to the end of the line */
void
rcons_clear2eol(rc)
	register struct rconsole *rc;
{
	register int x;

	x = *rc->rc_col * rc->rc_font->width;

	raster_op(rc->rc_sp,
	    rc->rc_xorigin + x,
	    *rc->rc_row * rc->rc_font->height + rc->rc_yorigin,
	    rc->rc_emuwidth - x, rc->rc_font->height,
	    rc->rc_ras_blank, (struct raster *) 0, 0, 0);
}

/* Scroll up one line */
void
rcons_scroll(rc, n)
	register struct rconsole *rc;
	register int n;
{
	register int ydiv;

	/* Can't scroll more than the whole screen */
	if (n > rc->rc_maxrow)
		n = rc->rc_maxrow;

	/* Calculate new row */
	*rc->rc_row -= n;
	if (*rc->rc_row < 0)
		*rc->rc_row  = 0;

	/* Calculate number of pixels to scroll */
	ydiv = rc->rc_font->height * n;

	raster_op(rc->rc_sp, rc->rc_xorigin, rc->rc_yorigin,
	    rc->rc_emuwidth, rc->rc_emuheight - ydiv,
	    RAS_SRC, rc->rc_sp, rc->rc_xorigin, ydiv + rc->rc_yorigin);

	raster_op(rc->rc_sp,
	    rc->rc_xorigin, rc->rc_yorigin + rc->rc_emuheight - ydiv,
	    rc->rc_emuwidth, ydiv, rc->rc_ras_blank, (struct raster *) 0, 0, 0);
}

/* Delete characters */
void
rcons_delchar(rc, n)
	register struct rconsole *rc;
	register int n;
{
	register int tox, fromx, y, width;

	/* Can't delete more chars than there are */
	if (n > rc->rc_maxcol - *rc->rc_col)
		n = rc->rc_maxcol - *rc->rc_col;

	fromx = (*rc->rc_col + n) * rc->rc_font->width;
	tox = *rc->rc_col * rc->rc_font->width;
	y = *rc->rc_row * rc->rc_font->height;
	width = n * rc->rc_font->width;

	raster_op(rc->rc_sp, tox + rc->rc_xorigin, y + rc->rc_yorigin,
	    rc->rc_emuwidth - fromx, rc->rc_font->height,
	    RAS_SRC, rc->rc_sp, fromx + rc->rc_xorigin, y + rc->rc_yorigin);

	raster_op(rc->rc_sp,
	    rc->rc_emuwidth - width + rc->rc_xorigin, y + rc->rc_yorigin,
	    width, rc->rc_font->height,
	    rc->rc_ras_blank, (struct raster *) 0, 0, 0);
}

/* Delete a number of lines */
void
rcons_delline(rc, n)
	register struct rconsole *rc;
	register int n;
{
	register int fromy, toy, height;

	/* Can't delete more lines than there are */
	if (n > rc->rc_maxrow - *rc->rc_row)
		n = rc->rc_maxrow - *rc->rc_row;

	fromy = (*rc->rc_row + n) * rc->rc_font->height;
	toy = *rc->rc_row * rc->rc_font->height;
	height = rc->rc_font->height * n;

	raster_op(rc->rc_sp, rc->rc_xorigin, toy + rc->rc_yorigin,
	    rc->rc_emuwidth, rc->rc_emuheight - fromy, RAS_SRC,
	    rc->rc_sp, rc->rc_xorigin, fromy + rc->rc_yorigin);

	raster_op(rc->rc_sp,
	    rc->rc_xorigin, rc->rc_emuheight - height + rc->rc_yorigin,
	    rc->rc_emuwidth, height,
	    rc->rc_ras_blank, (struct raster *) 0, 0, 0);
}

/* Insert some characters */
void
rcons_insertchar(rc, n)
	register struct rconsole *rc;
	register int n;
{
	register int tox, fromx, y;

	/* Can't insert more chars than can fit */
	if (n > rc->rc_maxcol - *rc->rc_col)
		n = rc->rc_maxcol - *rc->rc_col;

	tox = (*rc->rc_col + n) * rc->rc_font->width;
	fromx = *rc->rc_col * rc->rc_font->width;
	y = *rc->rc_row * rc->rc_font->height;

	raster_op(rc->rc_sp, tox + rc->rc_xorigin, y + rc->rc_yorigin,
	    rc->rc_emuwidth - tox, rc->rc_font->height,
	    RAS_SRC, rc->rc_sp, fromx + rc->rc_xorigin, y + rc->rc_yorigin);

	raster_op(rc->rc_sp, fromx + rc->rc_xorigin, y + rc->rc_yorigin,
	    rc->rc_font->width * n, rc->rc_font->height,
	    rc->rc_ras_blank, (struct raster *) 0, 0, 0);
}

/* Insert some lines */
void
rcons_insertline(rc, n)
	register struct rconsole *rc;
	register int n;
{
	register int fromy, toy;

	/* Can't insert more lines than can fit */
	if (n > rc->rc_maxrow - *rc->rc_row)
		n = rc->rc_maxrow - *rc->rc_row;

	toy = (*rc->rc_row + n) * rc->rc_font->height;
	fromy = *rc->rc_row * rc->rc_font->height;

	raster_op(rc->rc_sp, rc->rc_xorigin, toy + rc->rc_yorigin,
	    rc->rc_emuwidth, rc->rc_emuheight - toy,
	    RAS_SRC, rc->rc_sp, rc->rc_xorigin, fromy + rc->rc_yorigin);

	raster_op(rc->rc_sp, rc->rc_xorigin, fromy + rc->rc_yorigin,
	    rc->rc_emuwidth, rc->rc_font->height * n,
	    rc->rc_ras_blank, (struct raster *) 0, 0, 0);
}
