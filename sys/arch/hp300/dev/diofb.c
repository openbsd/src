/*	$OpenBSD: diofb.c,v 1.4 2005/01/18 19:17:03 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>

#include <dev/wscons/wsdisplayvar.h>

#include <hp300/dev/diofbreg.h>
#include <hp300/dev/diofbvar.h>

/*
 * X and Y location of character 'c' in the framebuffer, in pixels.
 */
#define	charX(fb,c)	\
	(((c) % (fb)->cpl) * (fb)->ftscale + (fb)->fontx)
#define	charY(fb,c)	\
	(((c) / (fb)->cpl) * (fb)->ftheight + (fb)->fonty)

void diofb_fontcopy(struct diofb *, char *, char *);

int	diofb_mapchar(void *, int, unsigned int *);
void	diofb_cursor(void *, int, int, int);
void	diofb_putchar(void *, int, int, u_int, long);
void	diofb_copycols(void *, int, int, int, int);
void	diofb_erasecols(void *, int, int, int, long);
void	diofb_copyrows(void *, int, int, int);
void	diofb_eraserows(void *, int, int, long);

const struct wsdisplay_emulops	diofb_emulops = {
	diofb_cursor,
	diofb_mapchar,
	diofb_putchar,
	diofb_copycols,
	diofb_erasecols,
	diofb_copyrows,
	diofb_eraserows,
	diofb_alloc_attr
};

/*
 * Frame buffer geometry initialization
 */

int
diofb_fbinquire(struct diofb *fb, int scode, struct diofbreg *fbr)
{
	int fboff, regsize;

	if (ISIIOVA(fbr))
		fb->regaddr = (caddr_t)IIOP(fbr);
	else
		fb->regaddr = dio_scodetopa(scode);

	if (fb->fbwidth == 0 || fb->fbheight == 0) {
		fb->fbwidth = (fbr->fbwmsb << 8) | fbr->fbwlsb;
		fb->fbheight = (fbr->fbhmsb << 8) | fbr->fbhlsb;
	}
	fb->fbsize = fb->fbwidth * fb->fbheight;

	fboff = (fbr->fbomsb << 8) | fbr->fbolsb;
	fb->fbaddr = (caddr_t) (*((u_char *)fbr + fboff) << 16);

	if (fb->regaddr >= (caddr_t)DIOII_BASE) {
		/*
		 * For DIO II space the fbaddr just computed is
		 * the offset from the select code base (regaddr)
		 * of the framebuffer.  Hence it is also implicitly
		 * the size of the set.
		 */
		regsize = (int)fb->fbaddr;
		fb->fbaddr += (int)fb->regaddr;
		fb->regkva = (caddr_t)fbr;
		fb->fbkva = (caddr_t)fbr + regsize;
	} else {
		/*
		 * For DIO space we need to map the separate
		 * framebuffer.
		 */
		fb->regkva = (caddr_t)fbr;
		fb->fbkva = iomap(fb->fbaddr, fb->fbsize);
		if (fb->fbkva == NULL)
			return (ENOMEM);
	}
	if (fb->dwidth == 0 || fb->dheight == 0) {
		fb->dwidth = (fbr->dwmsb << 8) | fbr->dwlsb;
		fb->dheight = (fbr->dhmsb << 8) | fbr->dhlsb;
	}

	/*
	 * Some displays, such as the DaVinci, appear to return a display
	 * height larger than the frame buffer height.
	 */
	if (fb->dwidth > fb->fbwidth)
		fb->dwidth = fb->fbwidth;
	if (fb->dheight > fb->fbheight)
		fb->dheight = fb->fbheight;

	return (0);
}

/*
 * PROM font setup
 */

void
diofb_fbsetup(struct diofb *fb)
{
	u_long fontaddr = getword(fb, getword(fb, FONTROM) + FONTADDR);

	/*
	 * Get font metrics.
	 */
	fb->ftheight = getbyte(fb, fontaddr + FONTHEIGHT);
	fb->ftwidth = getbyte(fb, fontaddr + FONTWIDTH);
	fb->ftscale = fb->ftwidth;
	fb->rows = fb->dheight / fb->ftheight;
	fb->cols = fb->dwidth / fb->ftwidth;

	/*
	 * Decide where to put the font in off-screen memory.
	 */
	if (fb->fbwidth > fb->dwidth) {
		/* Unpacked font will be to the right of the display */
		fb->fontx = fb->dwidth;
		fb->fonty = 0;
		fb->cpl = (fb->fbwidth - fb->dwidth) / fb->ftwidth;
		fb->cblankx = fb->dwidth;
	} else {
		/* Unpacked font will be below the display */
		fb->fontx = 0;
		fb->fonty = fb->dheight;
		fb->cpl = fb->fbwidth / fb->ftwidth;
		fb->cblankx = 0;
	}
	fb->cblanky = fb->fonty + ((FONTMAXCHAR / fb->cpl) + 1) * fb->ftheight;

	/*
	 * Clear display
	 */
	(*fb->bmv)(fb, 0, 0, 0, 0, fb->fbwidth, fb->fbheight, RR_CLEAR);
	fb->curvisible = 0;

	/*
	 * Setup inverted cursor.
	 */
	(*fb->bmv)(fb, charX(fb, ' '), charY(fb, ' '),
	    fb->cblankx, fb->cblanky, fb->ftwidth, fb->ftheight,
	    RR_COPYINVERTED);

	/*
	 * Default colormap
	 */
	bzero(&fb->cmap, sizeof(fb->cmap));
	fb->cmap.r[1] = 0xff;
	fb->cmap.g[1] = 0xff;
	fb->cmap.b[1] = 0xff;

	strlcpy(fb->wsd.name, "std", sizeof(fb->wsd.name));
	fb->wsd.ncols = fb->cols;
	fb->wsd.nrows = fb->rows;
	fb->wsd.textops = &diofb_emulops;
	fb->wsd.fontwidth = fb->ftwidth;
	fb->wsd.fontheight = fb->ftheight;
	fb->wsd.capabilities = WSSCREEN_REVERSE;
}

void
diofb_fontunpack(struct diofb *fb)
{
	char fontbuf[500];		/* XXX malloc not initialized yet */
	char *dp, *fbmem;
	int bytewidth, glyphsize;
	int c, i, romp;

	/*
	 * Unpack PROM font to the off-screen location.
	 */
	bytewidth = (((fb->ftwidth - 1) / 8) + 1);
	glyphsize = bytewidth * fb->ftheight;
	romp = getword(fb, getword(fb, FONTROM) + FONTADDR) + FONTDATA;
	for (c = 0; c < FONTMAXCHAR; c++) {
		fbmem = (char *)(FBBASE(fb) +
		     (fb->fonty + (c / fb->cpl) * fb->ftheight) * fb->fbwidth +
		     (fb->fontx + (c % fb->cpl) * fb->ftwidth));
		dp = fontbuf;
		for (i = 0; i < glyphsize; i++) {
			*dp++ = getbyte(fb, romp);
			romp += 2;
		}
		diofb_fontcopy(fb, fbmem, fontbuf);
	}
}

void
diofb_fontcopy(struct diofb *fb, char *fbmem, char *glyphp)
{
	int bn;
	int l, b;

	for (l = 0; l < fb->ftheight; l++) {
		bn = 7;
		for (b = 0; b < fb->ftwidth; b++) {
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
		fbmem -= fb->ftwidth;
		fbmem += fb->fbwidth;
	}
}

/*
 * Attachment helper
 */
void
diofb_end_attach(void *sc, struct wsdisplay_accessops *accessops,
    struct diofb *fb, int console, int planes, const char *descr)
{
	struct wsemuldisplaydev_attach_args waa;
	struct wsscreen_descr *scrlist[1];
	struct wsscreen_list screenlist;

	printf(": %dx%d", fb->dwidth, fb->dheight);

	if (planes == 0)
		planes = fb->planes;
	if (planes == 1)
		printf(" monochrome");
	else
		printf("x%d", planes);

	if (descr != NULL)
		printf(" %s", descr);
	printf(" frame buffer\n");

	scrlist[0] = &fb->wsd;
	screenlist.nscreens = 1;
	screenlist.screens = (const struct wsscreen_descr **)scrlist;

	waa.console = console;
	waa.scrdata = &screenlist;
	waa.accessops = accessops;
	waa.accesscookie = fb;

	config_found((struct device *)sc, &waa, wsemuldisplaydevprint);
}

/*
 * Common wsdisplay emulops for DIO frame buffers
 */

/* the cursor is just an inverted space */
#define flip_cursor(fb)							\
do {									\
	(*fb->bmv)((fb), (fb)->cblankx, (fb)->cblanky,			\
	    (fb)->cursorx * (fb)->ftwidth,				\
	    (fb)->cursory * (fb)->ftheight,				\
	    (fb)->ftwidth, (fb)->ftheight, RR_XOR);			\
} while (0)

int
diofb_alloc_attr(void *cookie, int fg, int bg, int flag, long *attrp)
{
	*attrp = flag & WSATTR_REVERSE;
	return (0);
}

int
diofb_mapchar(void *cookie, int c, unsigned int *cp)
{
	if (c < (int)' ' || c >= FONTMAXCHAR) {
		*cp = ' ';
		return (0);
	}

	*cp = c;
	return (5);
}

void
diofb_cursor(void *cookie, int on, int row, int col)
{
	struct diofb *fb = cookie;

	/* Turn old cursor off if necessary */
	if (fb->curvisible != 0)
		flip_cursor(fb);

	fb->cursorx = col;
	fb->cursory = row;

	if ((fb->curvisible = on) != 0)
		flip_cursor(fb);
}

void
diofb_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct diofb *fb = cookie;
	int wmrr;

	wmrr = (attr & WSATTR_REVERSE) ? RR_COPYINVERTED : RR_COPY;

	(*fb->bmv)(fb, charX(fb, uc), charY(fb, uc),
	    col * fb->ftwidth, row * fb->ftheight,
	    fb->ftwidth, fb->ftheight, wmrr);
}

void
diofb_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct diofb *fb = cookie;

	n *= fb->ftwidth;
	src *= fb->ftwidth;
	dst *= fb->ftwidth;
	row *= fb->ftheight;

	(*fb->bmv)(fb, src, row, dst, row, n, fb->ftheight, RR_COPY);
}

void
diofb_copyrows(void *cookie, int src, int dst, int n)
{
	struct diofb *fb = cookie;

	n *= fb->ftheight;
	src *= fb->ftheight;
	dst *= fb->ftheight;

	(*fb->bmv)(fb, 0, src, 0, dst, fb->dwidth, n, RR_COPY);
}

void
diofb_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct diofb *fb = cookie;

	num *= fb->ftwidth;
	col *= fb->ftwidth;
	row *= fb->ftheight;
	(*fb->bmv)(fb, col, row, col, row, num, fb->ftheight, RR_CLEAR);
}

void
diofb_eraserows(void *cookie, int row, int num, long attr)
{
	struct diofb *fb = cookie;

	row *= fb->ftheight;
	num *= fb->ftheight;
	(*fb->bmv)(fb, 0, row, 0, row, fb->dwidth, num, RR_CLEAR);
}

/*
 * Common wsdisplay accessops for DIO frame buffers
 */

int
diofb_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct diofb *fb = v;

	if (fb->nscreens > 0)
		return (ENOMEM);

	*cookiep = fb;
	*curxp = *curyp = 0;
	diofb_alloc_attr(fb, 0, 0, 0, attrp);
	fb->nscreens++;

	return (0);
}

void
diofb_free_screen(void *v, void *cookie)
{
	struct diofb *fb = v;

	fb->nscreens--;
}

int
diofb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

paddr_t
diofb_mmap(void * v, off_t offset, int prot)
{
	struct diofb *fb = v;

	if (offset & PGOFSET)
		return (-1);

	if (offset < 0 || offset >= fb->fbsize)
		return (-1);

	return (((paddr_t)fb->fbaddr + offset) >> PGSHIFT);
}
