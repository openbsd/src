/*	$OpenBSD: ite_subr.c,v 1.5 2006/08/17 06:31:10 miod Exp $	*/
/*	$NetBSD: ite_subr.c,v 1.8 1996/03/03 04:23:40 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: ite_subr.c 1.2 92/01/20$
 *
 *	@(#)ite_subr.c	8.1 (Berkeley) 6/10/93
 */

#ifdef ITECONSOLE

#include <sys/param.h>

#include "samachdep.h"
#include "itevar.h"
#include "itereg.h"

#define	getbyte(ip, offset) \
	*(((u_char *)(ip)->regbase) + (offset))

#define getword(ip, offset) \
	((getbyte(ip, offset) << 8) | getbyte(ip, (offset) + 2))

u_char	ite_readbyte(struct ite_data *, int);
void	ite_writeglyph(struct ite_data *, u_char *, u_char *);

void
ite_fontinfo(struct ite_data *ip)
{
	u_long fontaddr = getword(ip, getword(ip, FONTROM) + FONTADDR);

	ip->ftheight = getbyte(ip, fontaddr + FONTHEIGHT);
	ip->ftwidth  = getbyte(ip, fontaddr + FONTWIDTH);
	ip->rows     = ip->dheight / ip->ftheight;
	ip->cols     = ip->dwidth / ip->ftwidth;

	if (ip->fbwidth > ip->dwidth) {
		/*
		 * Stuff goes to right of display.
		 */
		ip->fontx    = ip->dwidth;
		ip->fonty    = 0;
		ip->cpl      = (ip->fbwidth - ip->dwidth) / ip->ftwidth;
		ip->cblankx  = ip->dwidth;
		ip->cblanky  = ip->fonty + ((128 / ip->cpl) +1) * ip->ftheight;
	} else {
		/*
		 * Stuff goes below the display.
		 */
		ip->fontx   = 0;
		ip->fonty   = ip->dheight;
		ip->cpl     = ip->fbwidth / ip->ftwidth;
		ip->cblankx = 0;
		ip->cblanky = ip->fonty + ((128 / ip->cpl) + 1) * ip->ftheight;
	}
}

void
ite_fontinit1bpp(struct ite_data *ip)
{
	u_char *fbmem, *dp;
	int c, l, b;
	int stride, width;

	dp = (u_char *)(getword(ip, getword(ip, FONTROM) + FONTADDR) +
	    ip->regbase) + FONTDATA;
	stride = ip->fbwidth >> 3;
	width = (ip->ftwidth + 7) / 8;

	for (c = 0; c < 128; c++) {
		fbmem = (u_char *)FBBASE +
		    (ip->fonty + (c / ip->cpl) * ip->ftheight) * stride;
		fbmem += (ip->fontx >> 3) + (c % ip->cpl) * width;
		for (l = 0; l < ip->ftheight; l++) {
			for (b = 0; b < width; b++) {
				*fbmem++ = *dp;
				dp += 2;
			}
			fbmem -= width;
			fbmem += stride;
		}
	}
}

void
ite_fontinit8bpp(struct ite_data *ip)
{
	int bytewidth = (((ip->ftwidth - 1) / 8) + 1);
	int glyphsize = bytewidth * ip->ftheight;
	u_char fontbuf[500];
	u_char *dp, *fbmem;
	int c, i, romp;

	romp = getword(ip, getword(ip, FONTROM) + FONTADDR) + FONTDATA;
	for (c = 0; c < 128; c++) {
		fbmem = (u_char *)(FBBASE +
		     (ip->fonty + (c / ip->cpl) * ip->ftheight) * ip->fbwidth +
		     (ip->fontx + (c % ip->cpl) * ip->ftwidth));
		dp = fontbuf;
		for (i = 0; i < glyphsize; i++) {
			*dp++ = getbyte(ip, romp);
			romp += 2;
		}
		ite_writeglyph(ip, fbmem, fontbuf);
	}
}

void
ite_writeglyph(struct ite_data *ip, u_char *fbmem, u_char *glyphp)
{
	int bn;
	int l, b;

	for (l = 0; l < ip->ftheight; l++) {
		bn = 7;
		for (b = 0; b < ip->ftwidth; b++) {
			if ((1 << bn) & *glyphp)
				*fbmem++ = 1;
			else
				*fbmem++ = 0;
			if (--bn < 0) {
				bn = 7;
				glyphp++;
			}
		}
		if (bn < 7)
			glyphp++;
		fbmem -= ip->ftwidth;
		fbmem += ip->fbwidth;
	}
}

/*
 * The cursor is just an inverted space.
 */
#define flip_cursor(ip) \
  	(*ip->bmv)(ip, ip->cblanky, ip->cblankx, ip->cursory * ip->ftheight, \
	    ip->cursorx * ip->ftwidth, ip->ftheight, ip->ftwidth, RR_XOR)

void
ite_dio_cursor(struct ite_data *ip, int flag)
{
	switch (flag) {
	case MOVE_CURSOR:
		flip_cursor(ip);
		/* FALLTHROUGH */
	case DRAW_CURSOR:
		ip->cursorx = ip->curx;
		ip->cursory = ip->cury;
		/* FALLTHROUGH */
	default:
		flip_cursor(ip);
		break;
	}
}

void
ite_dio_putc1bpp(struct ite_data *ip, int c, int dy, int dx)
{
	ite_dio_windowmove1bpp(ip, charY(ip, c), charX1bpp(ip, c),
	    dy * ip->ftheight, dx * ip->ftwidth,
	    ip->ftheight, ip->ftwidth, RR_COPY);
}

void
ite_dio_putc8bpp(struct ite_data *ip, int c, int dy, int dx)
{
	(*ip->bmv)(ip, charY(ip, c), charX(ip, c),
	    dy * ip->ftheight, dx * ip->ftwidth,
	    ip->ftheight, ip->ftwidth, RR_COPY);
}

void
ite_dio_clear(struct ite_data *ip, int sy, int sx, int h, int w)
{
	(*ip->bmv)(ip, sy * ip->ftheight, sx * ip->ftwidth,
	    sy * ip->ftheight, sx * ip->ftwidth,
	    h  * ip->ftheight, w  * ip->ftwidth, RR_CLEAR);
}

void
ite_dio_scroll(struct ite_data *ip)
{
	flip_cursor(ip);

	(*ip->bmv)(ip, ip->ftheight, 0, 0, 0, (ip->rows - 1) * ip->ftheight,
	    ip->cols * ip->ftwidth, RR_COPY);
}

#include "maskbits.h"

/* NOTE:
 * the first element in starttab could be 0xffffffff.  making it 0
 * lets us deal with a full first word in the middle loop, rather
 * than having to do the multiple reads and masks that we'd
 * have to do if we thought it was partial.
 */
int starttab[32] = {
	0x00000000,
	0x7FFFFFFF,
	0x3FFFFFFF,
	0x1FFFFFFF,
	0x0FFFFFFF,
	0x07FFFFFF,
	0x03FFFFFF,
	0x01FFFFFF,
	0x00FFFFFF,
	0x007FFFFF,
	0x003FFFFF,
	0x001FFFFF,
	0x000FFFFF,
	0x0007FFFF,
	0x0003FFFF,
	0x0001FFFF,
	0x0000FFFF,
	0x00007FFF,
	0x00003FFF,
	0x00001FFF,
	0x00000FFF,
	0x000007FF,
	0x000003FF,
	0x000001FF,
	0x000000FF,
	0x0000007F,
	0x0000003F,
	0x0000001F,
	0x0000000F,
	0x00000007,
	0x00000003,
	0x00000001
};

int endtab[32] = {
	0x00000000,
	0x80000000,
	0xC0000000,
	0xE0000000,
	0xF0000000,
	0xF8000000,
	0xFC000000,
	0xFE000000,
	0xFF000000,
	0xFF800000,
	0xFFC00000,
	0xFFE00000,
	0xFFF00000,
	0xFFF80000,
	0xFFFC0000,
	0xFFFE0000,
	0xFFFF0000,
	0xFFFF8000,
	0xFFFFC000,
	0xFFFFE000,
	0xFFFFF000,
	0xFFFFF800,
	0xFFFFFC00,
	0xFFFFFE00,
	0xFFFFFF00,
	0xFFFFFF80,
	0xFFFFFFC0,
	0xFFFFFFE0,
	0xFFFFFFF0,
	0xFFFFFFF8,
	0xFFFFFFFC,
	0xFFFFFFFE
};

void
ite_dio_windowmove1bpp(struct ite_data *ip, int sy, int sx, int dy, int dx,
    int h, int w, int func)
{
	int width;		/* add to get to same position in next line */

	unsigned int *psrcLine, *pdstLine;
                                /* pointers to line with current src and dst */
	unsigned int *psrc;	/* pointer to current src longword */
	unsigned int *pdst;	/* pointer to current dst longword */

                                /* following used for looping through a line */
	unsigned int startmask, endmask;  /* masks for writing ends of dst */
	int nlMiddle;		/* whole longwords in dst */
	int nl;			/* temp copy of nlMiddle */
	unsigned int tmpSrc;	/* place to store full source word */
	int xoffSrc;		/* offset (>= 0, < 32) from which to
                                   fetch whole longwords fetched
                                   in src */
	int nstart;		/* number of ragged bits at start of dst */
	int nend;		/* number of ragged bits at end of dst */
	int srcStartOver;	/* pulling nstart bits from src
                                   overflows into the next word? */

	if (h == 0 || w == 0)
		return;

	width = ip->fbwidth >> 5;
	psrcLine = ((unsigned int *) ip->fbbase) + (sy * width);
	pdstLine = ((unsigned int *) ip->fbbase) + (dy * width);

	/* x direction doesn't matter for < 1 longword */
	if (w <= 32) {
		int srcBit, dstBit;     /* bit offset of src and dst */

		pdstLine += (dx >> 5);
		psrcLine += (sx >> 5);
		psrc = psrcLine;
		pdst = pdstLine;

		srcBit = sx & 0x1f;
		dstBit = dx & 0x1f;

		while (h--) {
			getandputrop(psrc, srcBit, dstBit, w, pdst, func);
			pdst += width;
			psrc += width;
		}
	} else {
		maskbits(dx, w, startmask, endmask, nlMiddle);
		if (startmask)
			nstart = 32 - (dx & 0x1f);
		else
			nstart = 0;
		if (endmask)
			nend = (dx + w) & 0x1f;
		else
			nend = 0;

		xoffSrc = ((sx & 0x1f) + nstart) & 0x1f;
		srcStartOver = ((sx & 0x1f) + nstart) > 31;

		pdstLine += (dx >> 5);
		psrcLine += (sx >> 5);

		while (h--) {
			psrc = psrcLine;
			pdst = pdstLine;

			if (startmask) {
				getandputrop(psrc, (sx & 0x1f), (dx & 0x1f),
				    nstart, pdst, func);
				pdst++;
				if (srcStartOver)
					psrc++;
			}

			/* special case for aligned operations */
			if (xoffSrc == 0) {
				nl = nlMiddle;
				while (nl--) {
					DoRop (*pdst, func, *psrc++, *pdst);
					pdst++;
				}
			} else {
				nl = nlMiddle + 1;
				while (--nl) {
					getunalignedword (psrc, xoffSrc,
					    tmpSrc);
					DoRop (*pdst, func, tmpSrc, *pdst);
					pdst++;
					psrc++;
				}
			}

			if (endmask) {
				getandputrop0(psrc, xoffSrc, nend, pdst, func);
			}

			pdstLine += width;
			psrcLine += width;
		}
	}
}
#endif
