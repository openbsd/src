/*	$OpenBSD: diofb.c,v 1.17 2009/09/05 14:09:35 miod Exp $	*/

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

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <hp300/dev/diofbreg.h>
#include <hp300/dev/diofbvar.h>

extern int rasops_alloc_cattr(void *, int, int, int, long *);

int	diofb_alloc_attr(void *, int, int, int, long *);
int	diofb_copycols(void *, int, int, int, int);
int	diofb_erasecols(void *, int, int, int, long);
int	diofb_copyrows(void *, int, int, int);
int	diofb_eraserows(void *, int, int, long);
int	diofb_do_cursor(struct rasops_info *);

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

	fb->regkva = (caddr_t)fbr;
	fboff = (fbr->fbomsb << 8) | fbr->fbolsb;
	fb->fbaddr = (caddr_t) (*((u_char *)fbr + fboff) << 16);

	if (fb->regaddr >= (caddr_t)DIOII_BASE) {
		/*
		 * For DIO-II space the fbaddr just computed is
		 * the offset from the select code base (regaddr)
		 * of the framebuffer.  Hence it is also implicitly
		 * the size of the set.
		 */
		regsize = (int)fb->fbaddr;
		fb->fbaddr += (int)fb->regaddr;
		fb->fbkva = (caddr_t)fbr + regsize;
	} else {
		/*
		 * For internal or DIO-I space we need to map the separate
		 * framebuffer.
		 */
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

	fb->planes = fbr->num_planes;
	if (fb->planes > 8)
		fb->planes = 8;
	fb->planemask = (1 << fb->planes) - 1;

	fb->mapmode = WSDISPLAYIO_MODE_DUMBFB;

	return (0);
}

/*
 * Frame buffer rasops and colormap setup
 */

void
diofb_fbsetup(struct diofb *fb)
{
	struct rasops_info *ri = &fb->ri;

	/*
	 * Pretend we are an 8bpp frame buffer, unless ri_depth is already
	 * initialized, since this is how it is supposed to be addressed.
	 * (Hyperion forces 1bpp because it is really 1bpp addressed).
	 */
	if (ri->ri_depth == 0)
		ri->ri_depth = 8;
	ri->ri_stride = (fb->fbwidth * ri->ri_depth) / 8;

	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;
	/* We don't really support colors on less than 4bpp frame buffers */
	if (fb->planes < 4)
		ri->ri_flg |= RI_FORCEMONO;
	ri->ri_bits = fb->fbkva;
	ri->ri_width = fb->dwidth;
	ri->ri_height = fb->dheight;
	ri->ri_hw = fb;

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	rasops_init(ri, 160, 160);

	diofb_resetcmap(fb);

	/*
	 * For low depth frame buffers, since we have faked a 8bpp frame buffer
	 * to rasops, we actually have to remove capabilities.
	 */
	if (fb->planes == 4) {
		ri->ri_ops.alloc_attr = diofb_alloc_attr;
		ri->ri_caps &= ~WSSCREEN_HILIT;
	}
		
	ri->ri_ops.copycols = diofb_copycols;
	ri->ri_ops.erasecols = diofb_erasecols;
	if (ri->ri_depth != 1) {
		ri->ri_ops.copyrows = diofb_copyrows;
		ri->ri_ops.eraserows = diofb_eraserows;
		ri->ri_do_cursor = diofb_do_cursor;
	}

	/* Clear entire display, including non visible areas */
	(*fb->bmv)(fb, 0, 0, 0, 0, fb->fbwidth, fb->fbheight, RR_CLEAR, 0xff);

	strlcpy(fb->wsd.name, "std", sizeof(fb->wsd.name));
	fb->wsd.ncols = ri->ri_cols;
	fb->wsd.nrows = ri->ri_rows;
	fb->wsd.textops = &ri->ri_ops;
	fb->wsd.fontwidth = ri->ri_font->fontwidth;
	fb->wsd.fontheight = ri->ri_font->fontheight;
	fb->wsd.capabilities = ri->ri_caps;
}

/*
 * Setup default emulation mode colormap
 */
void
diofb_resetcmap(struct diofb *fb)
{
	const u_char *color;
	u_int i;

	/* start with the rasops colormap */
	color = (const u_char *)rasops_cmap;
	for (i = 0; i < 256; i++) {
		fb->cmap.r[i] = *color++;
		fb->cmap.g[i] = *color++;
		fb->cmap.b[i] = *color++;
	}

	/*
	 * Tweak colormap
	 *
	 * Due to the way rasops cursor work, we need to provide
	 * copies of the 8 or 16 basic colors at extra locations
	 * in 4bpp and 6bpp mode. This is because missing planes
	 * accept writes but read back as zero.
	 *
	 * So, in 6bpp mode:
	 *   00 gets inverted to ff, read back as 3f
	 *   3f gets inverted to c0, read back as 00
	 * and in 4bpp mode:
	 *   00 gets inverted to ff, read back as 0f
	 *   0f gets inverted to f0, read back as 00
	 */

	switch (fb->planes) {
	case 6:
		/*
		 * 00-0f normal colors
		 * 30-3f inverted colors
		 * c0-cf normal colors
		 * f0-ff inverted colors
		 */
		bcopy(fb->cmap.r + 0x00, fb->cmap.r + 0xc0, 0x10);
		bcopy(fb->cmap.g + 0x00, fb->cmap.g + 0xc0, 0x10);
		bcopy(fb->cmap.b + 0x00, fb->cmap.b + 0xc0, 0x10);
		bcopy(fb->cmap.r + 0xf0, fb->cmap.r + 0x30, 0x10);
		bcopy(fb->cmap.g + 0xf0, fb->cmap.g + 0x30, 0x10);
		bcopy(fb->cmap.b + 0xf0, fb->cmap.b + 0x30, 0x10);
		break;
	case 4:
		/*
		 * 00-07 normal colors
		 * 08-0f inverted colors
		 * highlighted colors are not available.
		 */
		bcopy(fb->cmap.r + 0xf8, fb->cmap.r + 0x08, 0x08);
		bcopy(fb->cmap.g + 0xf8, fb->cmap.g + 0x08, 0x08);
		bcopy(fb->cmap.b + 0xf8, fb->cmap.b + 0x08, 0x08);
		break;
	}
}

/*
 * Attachment helpers
 */

void
diofb_cnattach(struct diofb *fb)
{
	long defattr;
	struct rasops_info *ri;

	ri = &fb->ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&fb->wsd, ri, 0, 0, defattr);
}

void
diofb_end_attach(void *sc, struct wsdisplay_accessops *accessops,
    struct diofb *fb, int console, const char *descr)
{
	struct wsemuldisplaydev_attach_args waa;

	printf(": %dx%d", fb->dwidth, fb->dheight);

	if (fb->planes == 1)
		printf(" monochrome");
	else
		printf("x%d", fb->planes);

	if (descr != NULL)
		printf(" %s", descr);
	printf(" frame buffer\n");

	fb->scrlist[0] = &fb->wsd;
	fb->wsl.nscreens = 1;
	fb->wsl.screens = (const struct wsscreen_descr **)fb->scrlist;

	waa.console = console;
	waa.scrdata = &fb->wsl;
	waa.accessops = accessops;
	waa.accesscookie = fb;
	waa.defaultscreens = 0;

	config_found((struct device *)sc, &waa, wsemuldisplaydevprint);
}

/*
 * Common wsdisplay emulops for DIO frame buffers
 */

int
diofb_alloc_attr(void *cookie, int fg, int bg, int flg, long *attr)
{
	if ((flg & (WSATTR_BLINK | WSATTR_HILIT)) != 0)
		return (EINVAL);

	return (rasops_alloc_cattr(cookie, fg, bg, flg, attr));
}

int
diofb_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct diofb *fb = ri->ri_hw;

	n *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	(*fb->bmv)(fb, ri->ri_xorigin + src, ri->ri_yorigin + row,
	    ri->ri_xorigin + dst, ri->ri_yorigin + row,
	    n, ri->ri_font->fontheight, RR_COPY, 0xff);

	return 0;
}

int
diofb_copyrows(void *cookie, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct diofb *fb = ri->ri_hw;

	n *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;

	(*fb->bmv)(fb, ri->ri_xorigin, ri->ri_yorigin + src,
	    ri->ri_xorigin, ri->ri_yorigin + dst,
	    ri->ri_emuwidth, n, RR_COPY, 0xff);

	return 0;
}

int
diofb_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct diofb *fb = ri->ri_hw;
	int fg, bg;
	int snum, scol, srow;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	snum = num * ri->ri_font->fontwidth;
	scol = col * ri->ri_font->fontwidth + ri->ri_xorigin;
	srow = row * ri->ri_font->fontheight + ri->ri_yorigin;

	/*
	 * If this is too tricky for the simple raster ops engine,
	 * pass the fun to rasops.
	 */
	if ((*fb->bmv)(fb, scol, srow, scol, srow, snum,
	    ri->ri_font->fontheight, RR_CLEAR, 0xff ^ bg) != 0)
		rasops_erasecols(cookie, row, col, num, attr);

	return 0;
}

int
diofb_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct diofb *fb = ri->ri_hw;
	int fg, bg;
	int srow, snum;
	int rc;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	bg ^= 0xff;

	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR)) {
		rc = (*fb->bmv)(fb, 0, 0, 0, 0, ri->ri_width, ri->ri_height,
		    RR_CLEAR, bg);
	} else {
		srow = row * ri->ri_font->fontheight + ri->ri_yorigin;
		snum = num * ri->ri_font->fontheight;
		rc = (*fb->bmv)(fb, ri->ri_xorigin, srow, ri->ri_xorigin,
		    srow, ri->ri_emuwidth, snum, RR_CLEAR, bg);
	}
	if (rc != 0)
		rasops_eraserows(cookie, row, num, attr);

	return 0;
}

int
diofb_do_cursor(struct rasops_info *ri)
{
	struct diofb *fb = ri->ri_hw;
	int x, y;

	x = ri->ri_ccol * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = ri->ri_crow * ri->ri_font->fontheight + ri->ri_yorigin;
	(*fb->bmv)(fb, x, y, x, y, ri->ri_font->fontwidth,
	    ri->ri_font->fontheight, RR_INVERT, 0xff);

	return 0;
}

/*
 * Common wsdisplay accessops for DIO frame buffers
 */

int
diofb_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct diofb *fb = v;
	struct rasops_info *ri = &fb->ri;

	if (fb->nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, attrp);
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

	if ((offset & PAGE_MASK) != 0)
		return (-1);

	switch (fb->mapmode) {
	case WSDISPLAYIO_MODE_MAPPED:
		if (offset >= 0 && offset < DIOFB_REGSPACE)
			return (((paddr_t)fb->regaddr + offset) >> PGSHIFT);
		offset -= DIOFB_REGSPACE;
		/* FALLTHROUGH */
	case WSDISPLAYIO_MODE_DUMBFB:
		if (offset >= 0 && offset < fb->fbsize)
			return (((paddr_t)fb->fbaddr + offset) >> PGSHIFT);
		break;
	}

	return (-1);
}

int
diofb_getcmap(struct diofb *fb, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count;
	u_int colcount = 1 << fb->planes;
	int error;

	if (index >= colcount || count > colcount - index)
		return (EINVAL);

	if ((error = copyout(fb->cmap.r + index, cm->red, count)) != 0)
		return (error);
	if ((error = copyout(fb->cmap.g + index, cm->green, count)) != 0)
		return (error);
	if ((error = copyout(fb->cmap.b + index, cm->blue, count)) != 0)
		return (error);

	return (0);
}
