/*	$OpenBSD: macfb.c,v 1.19 2010/12/26 15:40:59 miod Exp $	*/
/* $NetBSD: macfb.c,v 1.11 2005/01/15 16:00:59 chs Exp $ */
/*
 * Copyright (c) 1998 Matt DeBergalis
 * All rights reserved.
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
 *      This product includes software developed by Matt DeBergalis
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <mac68k/dev/nubus.h>

#include <uvm/uvm_extern.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <mac68k/dev/macfbvar.h>

struct cfdriver macfb_cd = {
	NULL, "macfb", DV_DULL
};

int	macfb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	macfb_mmap(void *, off_t, int);
int	macfb_alloc_screen(void *, const struct wsscreen_descr *,
		    void **, int *, int *, long *);
void	macfb_free_screen(void *, void *);
int	macfb_show_screen(void *, void *, int,
		    void (*)(void *, int, int), void *);

const struct wsdisplay_accessops macfb_accessops = {
	macfb_ioctl,
	macfb_mmap,
	macfb_alloc_screen,
	macfb_free_screen,
	macfb_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL	/* burner */
};

int	macfb_alloc_cattr(void *, int, int, int, long *);
int	macfb_alloc_hattr(void *, int, int, int, long *);
int	macfb_alloc_mattr(void *, int, int, int, long *);
int	macfb_color_setup(struct macfb_devconfig *);
int	macfb_getcmap(struct macfb_devconfig *, struct wsdisplay_cmap *);
int	macfb_init(struct macfb_devconfig *);
int	macfb_is_console(paddr_t);
void	macfb_palette_setup(struct macfb_devconfig *);
int	macfb_putcmap(struct macfb_devconfig *, struct wsdisplay_cmap *);

paddr_t macfb_consaddr;

static struct macfb_devconfig macfb_console_dc;

/* From Booter via locore */
extern long		videoaddr;
extern long		videorowbytes;
extern long		videobitdepth;
extern u_long		videosize;
extern u_int32_t	mac68k_vidphys;
extern u_int32_t	mac68k_vidlen;

extern int rasops_alloc_cattr(void *, int, int, int, long *);

int
macfb_is_console(paddr_t addr)
{
	if (addr != macfb_consaddr && (addr >= NBBASE && addr < NBTOP)) {
		/*
		 * This is in the NuBus standard slot space range, so we
		 * may well have to look at 0xFssxxxxx, too.  Mask off the
		 * slot number and duplicate it in bits 20-23, per IM:V
		 * pp 459, 463, and IM:VI ch 30 p 17.
		 * Note:  this is an ugly hack and I wish I knew what
		 * to do about it.  -- sr
		 */
		addr = (paddr_t)(((u_long)addr & 0xff0fffff) |
		    (((u_long)addr & 0x0f000000) >> 4));
	}
	return ((mac68k_machine.serial_console & 0x03) == 0
	    && (addr == macfb_consaddr));
}

int
macfb_init(struct macfb_devconfig *dc)
{
	struct rasops_info *ri = &dc->dc_ri;
	int bgcolor;

	bzero(ri, sizeof(*ri));
	ri->ri_depth = dc->dc_depth;
	ri->ri_stride = dc->dc_rowbytes;
	ri->ri_flg = RI_CENTER;
	ri->ri_bits = (void *)(dc->dc_vaddr + dc->dc_offset);
	ri->ri_width = dc->dc_wid;
	ri->ri_height = dc->dc_ht;
	ri->ri_hw = dc;

	/* swap B and R if necessary */
	switch (ri->ri_depth) {
	case 16:
		ri->ri_rnum = 5;
		ri->ri_rpos = 11;
		ri->ri_gnum = 6;
		ri->ri_gpos = 5;
		ri->ri_bnum = 5;
		ri->ri_bpos = 0;
		break;
	case 24:
	case 32:
		ri->ri_rnum = 8;
		ri->ri_rpos = 16;
		ri->ri_gnum = 8;
		ri->ri_gpos = 8;
		ri->ri_bnum = 8;
		ri->ri_bpos = 0;
		break;
	}

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	if (rasops_init(ri, 160, 160) != 0)
		return (-1);

	bgcolor = macfb_color_setup(dc);

	/*
	 * Clear display. We can't pass RI_CLEAR in ri_flg and have rasops
	 * do it for us until we know how to setup the colormap first.
	 */
	memset((char *)dc->dc_vaddr + dc->dc_offset, bgcolor,
	     dc->dc_rowbytes * dc->dc_ht);

	strlcpy(dc->dc_wsd.name, "std", sizeof(dc->dc_wsd.name));
	dc->dc_wsd.ncols = ri->ri_cols;
	dc->dc_wsd.nrows = ri->ri_rows;
	dc->dc_wsd.textops = &ri->ri_ops;
	dc->dc_wsd.fontwidth = ri->ri_font->fontwidth;
	dc->dc_wsd.fontheight = ri->ri_font->fontheight;
	dc->dc_wsd.capabilities = ri->ri_caps;

	return (0);
}

int
macfb_color_setup(struct macfb_devconfig *dc)
{
	extern int rasops_alloc_cattr(void *, int, int, int, long *);
	struct rasops_info *ri = &dc->dc_ri;

	/* nothing to do for non-indexed modes... */
	if (ri->ri_depth > 8)
		return (0);	/* fill in black */

	if (dc->dc_setcolor == NULL || ISSET(dc->dc_flags, FB_MACOS_PALETTE) ||
	    ri->ri_depth < 2) {
		/*
		 * Until we know how to setup the colormap, or if we are
		 * already initialized (i.e. glass console), constrain ourselves
		 * to mono mode. Note that we need to use our own alloc_attr
		 * routine to compensate for inverted black and white colors.
		 */
		ri->ri_ops.alloc_attr = macfb_alloc_mattr;
		ri->ri_caps &= ~(WSSCREEN_WSCOLORS | WSSCREEN_HILIT);
		if (ri->ri_depth == 8)
			ri->ri_devcmap[15] = 0xffffffff;

		macfb_palette_setup(dc);

		return (0xff);	/* fill in black inherited from MacOS */
	}

	/* start from the rasops colormap */
	bcopy(rasops_cmap, dc->dc_cmap, 256 * 3);

	switch (ri->ri_depth) {
	case 2:
		/*
		 * 2bpp mode does not really have colors, only two gray
		 * shades in addition to black and white, to allow
		 * hilighting.
		 *
		 * Our palette needs to be:
		 *   00 black
		 *   01 dark gray (highlighted black, sort of)
		 *   02 light gray (normal white)
		 *   03 white (highlighted white)
		 */
		bcopy(dc->dc_cmap + (255 - WSCOL_WHITE) * 3,
		    dc->dc_cmap + 1 * 3, 3);
		bcopy(dc->dc_cmap + WSCOL_WHITE * 3, dc->dc_cmap + 2 * 3, 3);
		bcopy(dc->dc_cmap + (8 + WSCOL_WHITE) * 3,
		    dc->dc_cmap + 3 * 3, 3);
		ri->ri_caps |= WSSCREEN_HILIT;
		ri->ri_ops.alloc_attr = macfb_alloc_hattr;
		break;
	case 4:
		/*
		 * Tweak colormap
		 *
		 * Due to the way rasops cursor work, we need to provide
		 * inverted copies of the 8 basic colors as the other 8
		 * in 4bpp mode.
		 */
		bcopy(dc->dc_cmap + (256 - 8) * 3, dc->dc_cmap + 8 * 3, 8 * 3);
		ri->ri_caps |= WSSCREEN_WSCOLORS;
		ri->ri_ops.alloc_attr = macfb_alloc_cattr;
		break;
	default:
	case 8:
		break;
	}

	(*dc->dc_setcolor)(dc, 0, 1 << ri->ri_depth);

	return (WSCOL_BLACK);	/* fill in our own black */
}

/*
 * Initialize a black and white, MacOS compatible, shadow colormap.
 * This is necessary if we still want to be able to run X11 with colors.
 */
void
macfb_palette_setup(struct macfb_devconfig *dc)
{
	memset(dc->dc_cmap, 0xff, 3);		/* white */
	bzero(dc->dc_cmap + 3, 255 * 3);	/* black */
}

/*
 * Attribute allocator for monochrome displays (either 1bpp or no colormap
 * control). Note that the colors we return are indexes into ri_devcmap which
 * will select the actual bits.
 */
int
macfb_alloc_mattr(void *cookie, int fg, int bg, int flg, long *attr)
{
	if ((flg & (WSATTR_BLINK | WSATTR_HILIT | WSATTR_WSCOLORS)) != 0)
		return (EINVAL);

	/*
	 * Default values are white on black. However, on indexed displays,
	 * 0 is white and all bits set is black.
	 */
	if ((flg & WSATTR_REVERSE) != 0) {
		fg = 15;
		bg = 0;
	} else {
		fg = 0;
		bg = 15;
	}

	*attr = (bg << 16) | (fg << 24) | ((flg & WSATTR_UNDERLINE) ? 7 : 6);
	return (0);
}

/*
 * Attribute allocator for 2bpp displays.
 * Note that the colors we return are indexes into ri_devcmap which will
 * select the actual bits.
 */
int
macfb_alloc_hattr(void *cookie, int fg, int bg, int flg, long *attr)
{
	if ((flg & (WSATTR_BLINK | WSATTR_WSCOLORS)) != 0)
		return (EINVAL);

	if ((flg & WSATTR_REVERSE) != 0) {
		fg = WSCOL_BLACK;
		bg = WSCOL_WHITE;
	} else {
		fg = WSCOL_WHITE;
		bg = WSCOL_BLACK;
	}

	if ((flg & WSATTR_HILIT) != 0)
		fg += 8;

	*attr = (bg << 16) | (fg << 24) | ((flg & WSATTR_UNDERLINE) ? 7 : 6);
	return (0);
}

/*
 * Attribute allocator for 4bpp displays.
 */
int
macfb_alloc_cattr(void *cookie, int fg, int bg, int flg, long *attr)
{
	if ((flg & (WSATTR_BLINK | WSATTR_HILIT)) != 0)
		return (EINVAL);

	return (rasops_alloc_cattr(cookie, fg, bg, flg, attr));
}

void
macfb_attach_common(struct macfb_softc *sc, struct macfb_devconfig *dc)
{
	struct wsemuldisplaydev_attach_args waa;
	int isconsole;

	/* Print hardware characteristics. */
	printf("%s: %dx%d, ", sc->sc_dev.dv_xname, dc->dc_wid, dc->dc_ht);
	if (dc->dc_depth == 1)
		printf("monochrome");
	else
		printf("%dbit color", dc->dc_depth);
	printf(" display\n");

	isconsole = macfb_is_console(sc->sc_basepa + dc->dc_offset);

	if (isconsole) {
		macfb_console_dc.dc_setcolor = dc->dc_setcolor;
		macfb_console_dc.dc_cmapregs = dc->dc_cmapregs;
		free(dc, M_DEVBUF);
		dc = sc->sc_dc = &macfb_console_dc;
		dc->dc_nscreens = 1;
		macfb_color_setup(dc);
		/* XXX at this point we should reset the emulation to have
		 * it pick better attributes for kernel messages. Oh well. */
	} else {
		sc->sc_dc = dc;
		if (macfb_init(dc) != 0)
			return;
	}

	dc->dc_scrlist[0] = &dc->dc_wsd;
	dc->dc_screenlist.nscreens = 1;
	dc->dc_screenlist.screens =
	    (const struct wsscreen_descr **)dc->dc_scrlist;

	waa.console = isconsole;
	waa.scrdata = &dc->dc_screenlist;
	waa.accessops = &macfb_accessops;
	waa.accesscookie = sc;
	waa.defaultscreens = 0;

	config_found((struct device *)sc, &waa, wsemuldisplaydevprint);
}

int
macfb_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct macfb_softc *sc = v;
	struct macfb_devconfig *dc = sc->sc_dc;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = WSDISPLAY_TYPE_MAC68K;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = dc->dc_ri.ri_height;
		wdf->width = dc->dc_ri.ri_width;
		wdf->depth = dc->dc_ri.ri_depth;
		if (dc->dc_ri.ri_depth > 8 || dc->dc_setcolor == NULL)
			wdf->cmsize = 0;
		else
			wdf->cmsize = 1 << dc->dc_ri.ri_depth;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = dc->dc_ri.ri_stride;
		break;
	case WSDISPLAYIO_GETCMAP:
		if (dc->dc_ri.ri_depth > 8 || dc->dc_setcolor == NULL)
			return (0);
		return (macfb_getcmap(dc, (struct wsdisplay_cmap *)data));
	case WSDISPLAYIO_PUTCMAP:
		if (dc->dc_ri.ri_depth > 8 || dc->dc_setcolor == NULL)
			return (0);
		return (macfb_putcmap(dc, (struct wsdisplay_cmap *)data));
	case WSDISPLAYIO_SMODE:
		if (dc->dc_ri.ri_depth > 8 || dc->dc_setcolor == NULL)
			return (0);
		if (*(u_int *)data == WSDISPLAYIO_MODE_EMUL &&
		    ISSET(dc->dc_flags, FB_MACOS_PALETTE)) {
			macfb_palette_setup(dc);
			(*dc->dc_setcolor)(dc, 0, 1 << dc->dc_ri.ri_depth);
			/* clear display */
			memset((char *)dc->dc_vaddr + dc->dc_offset, 0xff,
			     dc->dc_rowbytes * dc->dc_ht);
		}
		break;
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;
	default:
		return (-1);
	}

	return (0);
}

paddr_t
macfb_mmap(void *v, off_t offset, int prot)
{
	struct macfb_softc *sc = v;
	struct macfb_devconfig *dc = sc->sc_dc;
	paddr_t addr;

	if (offset >= 0 &&
	    offset < round_page(dc->dc_size))
		addr = (dc->dc_paddr + dc->dc_offset + offset);
	else
		addr = (-1);

	return addr;
}

int
macfb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct macfb_softc *sc = v;
	struct rasops_info *ri = &sc->sc_dc->dc_ri;

	if (sc->sc_dc->dc_nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, defattrp);
	sc->sc_dc->dc_nscreens++;

	return (0);
}

void
macfb_free_screen(void *v, void *cookie)
{
	struct macfb_softc *sc = v;

	sc->sc_dc->dc_nscreens--;
}

int
macfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

int
macfb_getcmap(struct macfb_devconfig *dc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count;
	u_int colcount = 1 << dc->dc_ri.ri_depth;
	int i, error;
	u_int8_t ramp[256], *c, *r;

	if (index >= colcount || count > colcount - index)
		return (EINVAL);

	/* extract reds */
	c = dc->dc_cmap + 0 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c, c += 3;
	if ((error = copyout(ramp, cm->red, count)) != 0)
		return (error);

	/* extract greens */
	c = dc->dc_cmap + 1 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c, c += 3;
	if ((error = copyout(ramp, cm->green, count)) != 0)
		return (error);

	/* extract blues */
	c = dc->dc_cmap + 2 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c, c += 3;
	if ((error = copyout(ramp, cm->blue, count)) != 0)
		return (error);

	return (0);
}

int
macfb_putcmap(struct macfb_devconfig *dc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count;
	u_int colcount = 1 << dc->dc_ri.ri_depth;
	int i, error;
	u_int8_t r[256], g[256], b[256], *nr, *ng, *nb, *c;

	if (index >= colcount || count > colcount - index)
		return (EINVAL);

	if ((error = copyin(cm->red, r, count)) != 0)
		return (error);
	if ((error = copyin(cm->green, g, count)) != 0)
		return (error);
	if ((error = copyin(cm->blue, b, count)) != 0)
		return (error);

	nr = r, ng = g, nb = b;
	c = dc->dc_cmap + index * 3;
	for (i = count; i != 0; i--) {
		*c++ = *nr++;
		*c++ = *ng++;
		*c++ = *nb++;
	}

	(*dc->dc_setcolor)(dc, index, index + count);

	return (0);
}

int
macfb_cnattach()
{
	struct macfb_devconfig *dc = &macfb_console_dc;
	long defattr;
	struct rasops_info *ri;

	dc->dc_vaddr = trunc_page(videoaddr);
	dc->dc_paddr = trunc_page(mac68k_vidphys);
	dc->dc_offset = m68k_page_offset(mac68k_vidphys);
	dc->dc_wid = videosize & 0xffff;
	dc->dc_ht = (videosize >> 16) & 0xffff;
	dc->dc_depth = videobitdepth;
	dc->dc_rowbytes = videorowbytes;
	dc->dc_size = (mac68k_vidlen > 0) ?
	    mac68k_vidlen : dc->dc_ht * dc->dc_rowbytes;

	/* set up the display */
	dc->dc_flags |= FB_MACOS_PALETTE;
	if (macfb_init(dc) != 0)
		return (-1);

	ri = &dc->dc_ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&dc->dc_wsd, ri, 0, 0, defattr);

	macfb_consaddr = mac68k_vidphys;
	return (0);
}
