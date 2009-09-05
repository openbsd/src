/*	$OpenBSD: zx.c,v 1.23 2009/09/05 14:09:35 miod Exp $	*/
/*	$NetBSD: zx.c,v 1.5 2002/10/02 16:52:46 thorpej Exp $	*/

/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 *
 * Derived from NetBSD syssrc/sys/dev/sbus/zx.c under the following licence
 * terms:
 *
 *  Copyright (c) 2002 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Andrew Doran.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the Sun ZX display adapter.  This would be called 'leo', but
 * NetBSD/amiga already has a driver by that name.  The XFree86 and Linux
 * drivers were used as "living documentation" when writing this; thanks
 * to the authors.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <dev/sbus/zxreg.h>
#include <sparc/dev/sbusvar.h>

#include <dev/cons.h>	/* for prom console hook */

#define	ZX_WID_SHARED_8		0
#define	ZX_WID_SHARED_24	1
#define	ZX_WID_DBL_8		2
#define	ZX_WID_DBL_24		3

/*
 * Per-instance data.
 */

struct zx_cmap {
	u_int8_t	cm_red[256];
	u_int8_t	cm_green[256];
	u_int8_t	cm_blue[256];
};

struct zx_softc {
	struct	sunfb	sc_sunfb;
	struct	rom_reg	sc_phys;

	struct	zx_cmap	sc_cmap;	/* shadow color map for overlay plane */

	volatile struct zx_command *sc_zc;
	volatile struct zx_cross *sc_zx;
	volatile struct zx_draw *sc_zd_ss0;
	volatile struct zx_draw_ss1 *sc_zd_ss1;
	volatile struct zx_cursor *sc_zcu;

	int	sc_mode;
};

void	zx_burner(void *, u_int, u_int);
int	zx_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	zx_mmap(void *v, off_t offset, int prot);
void	zx_prom(void *);
void	zx_reset(struct zx_softc *, u_int);
void	zx_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);

struct wsdisplay_accessops zx_accessops = {
	zx_ioctl,
	zx_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	zx_burner,
	NULL	/* pollc */
};

/* Force 32-bit writes. */
#define	SETREG(r, v)	(*((volatile u_int32_t *)&r) = (v))

#define	ZX_STD_ROP	(ZX_ROP_NEW | ZX_ATTR_WE_ENABLE | \
    ZX_ATTR_OE_ENABLE | ZX_ATTR_FORCE_WID)

#define	ZX_BWIDTH	13
#define	ZX_WWIDTH	11	/* word width */

#define	ZX_COORDS(x, y)	((x) | ((y) << ZX_WWIDTH))

void	zx_attach(struct device *, struct device *, void *);
int	zx_match(struct device *, void *, void *);

void	zx_copyrect(struct rasops_info *, int, int, int, int, int, int);
int	zx_cross_loadwid(struct zx_softc *, u_int, u_int, u_int);
int	zx_cross_wait(struct zx_softc *);
void	zx_fillrect(struct rasops_info *, int, int, int, int, long, int);
int	zx_intr(void *);
int	zx_putcmap(struct zx_softc *);

int	zx_copycols(void *, int, int, int, int);
int	zx_copyrows(void *, int, int, int);
int	zx_do_cursor(struct rasops_info *);
int	zx_erasecols(void *, int, int, int, long);
int	zx_eraserows(void *, int, int, long);
int	zx_putchar(void *, int, int, u_int, long);

struct cfattach zx_ca = {
	sizeof(struct zx_softc), zx_match, zx_attach
};

struct cfdriver zx_cd = {
	NULL, "zx", DV_DULL
};

int
zx_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	
	if (strcmp(ra->ra_name, "SUNW,leo") == 0)
		return (1);

	return (0);
}

void
zx_attach(struct device *parent, struct device *self, void *args)
{
	struct zx_softc *sc = (struct zx_softc *)self;
	struct confargs *ca = args;
	struct rasops_info *ri;
	int node, isconsole = 0;
	const char *nam;

	ri = &sc->sc_sunfb.sf_ro;
	node = ca->ca_ra.ra_node;

	/*
	 * Map the various parts of the card.
	 */
	sc->sc_phys = ca->ca_ra.ra_reg[0];

	sc->sc_zc = (struct zx_command *)
	    mapiodev(ca->ca_ra.ra_reg, ZX_OFF_LC_SS0_USR,
	        sizeof(struct zx_command));
	sc->sc_zd_ss0 = (struct zx_draw *)
	    mapiodev(ca->ca_ra.ra_reg, ZX_OFF_LD_SS0,
	        sizeof(struct zx_draw));
	sc->sc_zd_ss1 = (struct zx_draw_ss1 *)
	    mapiodev(ca->ca_ra.ra_reg, ZX_OFF_LD_SS1,
	        sizeof(struct zx_draw_ss1));
	sc->sc_zx = (struct zx_cross *)
	    mapiodev(ca->ca_ra.ra_reg, ZX_OFF_LX_CROSS,
	        sizeof(struct zx_cross));
	sc->sc_zcu = (struct zx_cursor *)
	    mapiodev(ca->ca_ra.ra_reg, ZX_OFF_LX_CURSOR,
	        sizeof(struct zx_cursor));

	nam = getpropstring(node, "model");
	if (*nam == '\0')
		nam = ca->ca_ra.ra_name;
	printf(": %s", nam);

	isconsole = node == fbnode;

	/*
	 * The console is using the 8-bit overlay plane, while the prom
	 * will correctly report 32 bit depth.
	 * The following is an equivalent for
	 *    fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);
	 * forcing the depth value not to be overwritten.
	 * Furthermore, the linebytes value is in fact 8192 bytes.
	 */
	sc->sc_sunfb.sf_depth = 8;
	sc->sc_sunfb.sf_width = getpropint(node, "width", 1152);
	sc->sc_sunfb.sf_height = getpropint(node, "height", 900);
	sc->sc_sunfb.sf_linebytes = 1 << ZX_BWIDTH;
	sc->sc_sunfb.sf_fbsize = sc->sc_sunfb.sf_height << ZX_BWIDTH;

	printf(", %dx%d\n", sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	ri->ri_bits = mapiodev(ca->ca_ra.ra_reg,
	    ZX_OFF_SS0, round_page(sc->sc_sunfb.sf_fbsize));
	ri->ri_hw = sc;

	fbwscons_init(&sc->sc_sunfb, isconsole);

	/*
	 * Watch out! rasops_init() invoked via fbwscons_init() did not
	 * compute ri_bits correctly when centering the display, because
	 * it has been tricked with the low depth value.
	 * Recompute now.
	 */
	ri->ri_emustride = ri->ri_emuwidth * 4;
	ri->ri_delta = ri->ri_stride - ri->ri_emustride;
	ri->ri_pelbytes = 4;
	ri->ri_xscale = ri->ri_font->fontwidth * 4;
	ri->ri_bits = ri->ri_origbits;
	ri->ri_bits += (((ri->ri_width * 4) - ri->ri_emustride) >> 1) & ~3;
	ri->ri_bits += ((ri->ri_height - ri->ri_emuheight) >> 1) *
	    ri->ri_stride;
	ri->ri_yorigin = (int)(ri->ri_bits - ri->ri_origbits)
	    / ri->ri_stride;
	ri->ri_xorigin = (((int)(ri->ri_bits - ri->ri_origbits)
	    % ri->ri_stride) / 4);

	ri->ri_ops.copyrows = zx_copyrows;
	ri->ri_ops.copycols = zx_copycols;
	ri->ri_ops.eraserows = zx_eraserows;
	ri->ri_ops.erasecols = zx_erasecols;
	ri->ri_ops.putchar = zx_putchar;
	ri->ri_do_cursor = zx_do_cursor;

	if (isconsole) {
		/* zx_reset() below will clear screen, so restart at 1st row */
		fbwscons_console_init(&sc->sc_sunfb, 0);
		shutdownhook_establish(zx_prom, sc);
	}

	/* reset cursor & frame buffer controls */
	sc->sc_mode = ~WSDISPLAYIO_MODE_EMUL;	/* force action */
	zx_reset(sc, WSDISPLAYIO_MODE_EMUL);

	/* enable video */
	zx_burner(sc, 1, 0);

	fbwscons_attach(&sc->sc_sunfb, &zx_accessops, isconsole);
}

int
zx_ioctl(void *dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct zx_softc *sc = dev;
	struct wsdisplay_fbinfo *wdf;

	/*
	 * Note that, although the emulation (text) mode is running in
	 * a 8-bit plane, we advertize the frame buffer as the full-blown
	 * 32-bit beast it is.
	 */
	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNLEO;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width = sc->sc_sunfb.sf_width;
		wdf->depth = 32;
		wdf->cmsize = 0;
		break;
	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		*(u_int *)data = WSDISPLAYIO_DEPTH_24_32;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		break;

	case WSDISPLAYIO_SMODE:
		zx_reset(sc, *(u_int *)data);
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
		break;

	default:
		return (-1);
	}

	return (0);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
zx_mmap(void *v, off_t offset, int prot)
{
	struct zx_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, ZX_OFF_SS0 + offset) | PMAP_NC);
	}

	return (-1);
}

void
zx_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct zx_softc *sc = v;

	sc->sc_cmap.cm_red[index] = r;
	sc->sc_cmap.cm_green[index] = g;
	sc->sc_cmap.cm_blue[index] = b;
}

void
zx_reset(struct zx_softc *sc, u_int mode)
{
	volatile struct zx_draw *zd;
	volatile struct zx_command *zc;
	u_int32_t i;
	const u_char *color;
	u_int8_t *r, *g, *b;

	if (mode == sc->sc_mode)
		return;

	zd = sc->sc_zd_ss0;
	zc = sc->sc_zc;

	if (mode == WSDISPLAYIO_MODE_EMUL) {
		/* Back from X11 to emulation mode, or first reset */
		zx_cross_loadwid(sc, ZX_WID_DBL_8, 0, 0x2c0);
		zx_cross_loadwid(sc, ZX_WID_DBL_8, 1, 0x30);
		zx_cross_loadwid(sc, ZX_WID_DBL_8, 2, 0x20);
		zx_cross_loadwid(sc, ZX_WID_DBL_24, 1, 0x30);

		i = sc->sc_zd_ss1->zd_misc;
		i |= ZX_SS1_MISC_ENABLE;
		SETREG(sc->sc_zd_ss1->zd_misc, i);

		/*
		 * XXX
		 * If zc_fill is not set to that value, there will be black
		 * bars left in the margins. But then with this value, the
		 * screen gets cleared. Go figure.
		 */
		SETREG(zd->zd_wid, 0xffffffff);
		SETREG(zd->zd_wmask, 0xffff);
		SETREG(zd->zd_vclipmin, 0);
		SETREG(zd->zd_vclipmax, (sc->sc_sunfb.sf_width - 1) |
		    ((sc->sc_sunfb.sf_height - 1) << 16));
		SETREG(zd->zd_fg, 0);
		SETREG(zd->zd_planemask, 0xff000000);
		SETREG(zd->zd_rop, ZX_STD_ROP);
		SETREG(zd->zd_widclip, 0);

		SETREG(zc->zc_extent, ZX_COORDS(sc->sc_sunfb.sf_width - 1,
		    sc->sc_sunfb.sf_height - 1));
		SETREG(zc->zc_addrspace, ZX_ADDRSPC_FONT_OBGR);
		SETREG(zc->zc_fill, ZX_COORDS(0, 0) | ZX_EXTENT_DIR_BACKWARDS);
		SETREG(zc->zc_fontt, 0);

		while ((zc->zc_csr & ZX_CSR_BLT_BUSY) != 0)
			;

		/*
		 * Initialize the 8-bit colormap
		 */
		r = sc->sc_cmap.cm_red;
		g = sc->sc_cmap.cm_green;
		b = sc->sc_cmap.cm_blue;
		color = rasops_cmap;
		for (i = 0; i < 256; i++) {
			*r++ = *color++;
			*g++ = *color++;
			*b++ = *color++;
		}
		fbwscons_setcolormap(&sc->sc_sunfb, zx_setcolor);
		zx_putcmap(sc);
	} else {
		/* Starting X11 - switch to 24bit WID */
		SETREG(zd->zd_wid, 1);
		SETREG(zd->zd_widclip, 0);
		SETREG(zd->zd_wmask, 0xffff);
		SETREG(zd->zd_planemask, 0x00ffffff);
		SETREG(zc->zc_extent, ZX_COORDS(sc->sc_sunfb.sf_width - 1,
		    sc->sc_sunfb.sf_height - 1));
		SETREG(zc->zc_fill, 0);
		while ((zc->zc_csr & ZX_CSR_BLT_BUSY) != 0)
			;

		SETREG(zc->zc_addrspace, ZX_ADDRSPC_OBGR);
		SETREG(zd->zd_rop, ZX_ATTR_RGBE_ENABLE |
		    ZX_ROP_NEW /* | ZX_ATTR_FORCE_WID */);
	}

	sc->sc_mode = mode;
}

int
zx_cross_wait(struct zx_softc *sc)
{
	volatile struct zx_cross *zx;
	int i;

	zx = sc->sc_zx;

	for (i = 300000; i != 0; i--) {
		if ((zx->zx_csr & ZX_CROSS_CSR_PROGRESS) == 0)
			break;
		DELAY(1);
	}

	if (i == 0)
		printf("%s: zx_cross_wait: timed out\n",
		    sc->sc_sunfb.sf_dev.dv_xname);

	return (i);
}

int
zx_cross_loadwid(struct zx_softc *sc, u_int type, u_int index, u_int value)
{
	volatile struct zx_cross *zx;
	u_int tmp;

	zx = sc->sc_zx;
	SETREG(zx->zx_type, ZX_CROSS_TYPE_WID);

	if (!zx_cross_wait(sc))
		return (1);

	if (type == ZX_WID_DBL_8)
		tmp = (index & 0x0f) + 0x40;
	else if (type == ZX_WID_DBL_24)
		tmp = index & 0x3f;

	SETREG(zx->zx_type, 0x5800 + tmp);
	SETREG(zx->zx_value, value);
	SETREG(zx->zx_type, ZX_CROSS_TYPE_WID);
	SETREG(zx->zx_csr, ZX_CROSS_CSR_UNK | ZX_CROSS_CSR_UNK2);

	return (0);
}

int
zx_putcmap(struct zx_softc *sc)
{
	volatile struct zx_cross *zx;
	u_int32_t i;
	u_int8_t *r, *g, *b;

	zx = sc->sc_zx;

	SETREG(zx->zx_type, ZX_CROSS_TYPE_CLUT0);
	if (!zx_cross_wait(sc))
		return (1);

	SETREG(zx->zx_type, ZX_CROSS_TYPE_CLUTDATA);

	r = sc->sc_cmap.cm_red;
	g = sc->sc_cmap.cm_green;
	b = sc->sc_cmap.cm_blue;
	for (i = 0; i < 256; i++) {
		SETREG(zx->zx_value, *r++ | (*g++ << 8) | (*b++ << 16));
	}

	SETREG(zx->zx_type, ZX_CROSS_TYPE_CLUT0);
	i = zx->zx_csr;
	i = i | ZX_CROSS_CSR_UNK | ZX_CROSS_CSR_UNK2;
	SETREG(zx->zx_csr, i);
	return (0);
}

void
zx_burner(void *v, u_int on, u_int flags)
{
	struct zx_softc *sc = v;
	volatile struct zx_cross *zx;
	u_int32_t i;

	zx = sc->sc_zx;

	SETREG(zx->zx_type, ZX_CROSS_TYPE_VIDEO);
	i = zx->zx_csr;
	if (on) {
		i |= ZX_CROSS_CSR_ENABLE;
	} else {
		i &= ~ZX_CROSS_CSR_ENABLE;
	}
	SETREG(zx->zx_csr, i);
}

void
zx_fillrect(struct rasops_info *ri, int x, int y, int w, int h, long attr,
    int rop)
{
	struct zx_softc *sc;
	volatile struct zx_command *zc;
	volatile struct zx_draw *zd;
	int fg, bg;

	sc = ri->ri_hw;
	zc = sc->sc_zc;
	zd = sc->sc_zd_ss0;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);
	x = x * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = y * ri->ri_font->fontheight + ri->ri_yorigin;
	w = ri->ri_font->fontwidth * w - 1;
	h = ri->ri_font->fontheight * h - 1;

	while ((zc->zc_csr & ZX_CSR_BLT_BUSY) != 0)
		;

	SETREG(zd->zd_rop, rop);
	SETREG(zd->zd_fg, ri->ri_devcmap[bg] << 24);
	SETREG(zc->zc_extent, ZX_COORDS(w, h));
	SETREG(zc->zc_fill, ZX_COORDS(x, y) | ZX_EXTENT_DIR_BACKWARDS);
}

void
zx_copyrect(struct rasops_info *ri, int sx, int sy, int dx, int dy, int w,
    int h)
{
	struct zx_softc *sc;
	volatile struct zx_command *zc;
	volatile struct zx_draw *zd;
	int dir;

	sc = ri->ri_hw;
	zc = sc->sc_zc;
	zd = sc->sc_zd_ss0;

	sx = sx * ri->ri_font->fontwidth + ri->ri_xorigin;
	sy = sy * ri->ri_font->fontheight + ri->ri_yorigin;
	dx = dx * ri->ri_font->fontwidth + ri->ri_xorigin;
	dy = dy * ri->ri_font->fontheight + ri->ri_yorigin;
	w = w * ri->ri_font->fontwidth - 1;
	h = h * ri->ri_font->fontheight - 1;

	if (sy < dy || sx < dx) {
		dir = ZX_EXTENT_DIR_BACKWARDS;
		sx += w;
		sy += h;
		dx += w;
		dy += h;
	} else
		dir = ZX_EXTENT_DIR_FORWARDS;

	while ((zc->zc_csr & ZX_CSR_BLT_BUSY) != 0)
		;

	SETREG(zd->zd_rop, ZX_STD_ROP);
	SETREG(zc->zc_extent, ZX_COORDS(w, h) | dir);
	SETREG(zc->zc_src, ZX_COORDS(sx, sy));
	SETREG(zc->zc_copy, ZX_COORDS(dx, dy));
}

int
zx_do_cursor(struct rasops_info *ri)
{

	zx_fillrect(ri, ri->ri_ccol, ri->ri_crow, 1, 1, WSCOL_BLACK << 16,
	    ZX_ROP_NEW_XOR_OLD | ZX_ATTR_WE_ENABLE | ZX_ATTR_OE_ENABLE |
	    ZX_ATTR_FORCE_WID);

	return 0;
}

int
zx_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

	zx_fillrect(ri, col, row, num, 1, attr, ZX_STD_ROP);

	return 0;
}

int
zx_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri;
	struct zx_softc *sc;
	volatile struct zx_command *zc;
	volatile struct zx_draw *zd;
	int fg, bg;

	ri = (struct rasops_info *)cookie;

	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR)) {
		sc = ri->ri_hw;
		zc = sc->sc_zc;
		zd = sc->sc_zd_ss0;

		ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

		while ((zc->zc_csr & ZX_CSR_BLT_BUSY) != 0)
			;

		SETREG(zd->zd_rop, ZX_STD_ROP);
		SETREG(zd->zd_fg, ri->ri_devcmap[bg] << 24);
		SETREG(zc->zc_extent,
		    ZX_COORDS(ri->ri_width - 1, ri->ri_height - 1));
		SETREG(zc->zc_fill, ZX_COORDS(0, 0) | ZX_EXTENT_DIR_BACKWARDS);
	} else
		zx_fillrect(ri, 0, row, ri->ri_cols, num, attr, ZX_STD_ROP);

	return 0;
}

int
zx_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

	zx_copyrect(ri, 0, src, 0, dst, ri->ri_cols, num);

	return 0;
}

int
zx_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

	zx_copyrect(ri, src, row, dst, row, num, 1);

	return 0;
}

int
zx_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri;
	struct zx_softc *sc;
	struct wsdisplay_font *font;
	volatile struct zx_command *zc;
	volatile struct zx_draw *zd;
	volatile u_int32_t *dp;
	u_int8_t *fb;
	int fs, i, fg, bg, ul;

	ri = (struct rasops_info *)cookie;
	font = ri->ri_font;
	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, &ul);
	fg = ri->ri_devcmap[fg];
	bg = ri->ri_devcmap[bg];

	dp = (volatile u_int32_t *)ri->ri_bits +
	    ZX_COORDS(col * font->fontwidth, row * font->fontheight);

	if (uc == ' ') {
		zx_fillrect(ri, col, row, 1, 1, attr, ZX_STD_ROP);
		if (ul == 0)
			return 0;

		dp += font->fontheight << ZX_WWIDTH;

		while ((zc->zc_csr & ZX_CSR_BLT_BUSY) != 0)
			;

		SETREG(zd->zd_rop, ZX_STD_ROP);
		SETREG(zd->zd_fg, fg << 24);
		SETREG(zd->zd_bg, bg << 24);
		SETREG(zc->zc_fontmsk, 0xffffffff << (32 - font->fontwidth));
	} else {
		sc = ri->ri_hw;
		zc = sc->sc_zc;
		zd = sc->sc_zd_ss0;

		fb = (u_int8_t *)font->data + (uc - font->firstchar) *
		    ri->ri_fontscale;
		fs = font->stride;

		while ((zc->zc_csr & ZX_CSR_BLT_BUSY) != 0)
			;

		SETREG(zd->zd_rop, ZX_STD_ROP);
		SETREG(zd->zd_fg, fg << 24);
		SETREG(zd->zd_bg, bg << 24);
		SETREG(zc->zc_fontmsk, 0xffffffff << (32 - font->fontwidth));

		if (font->fontwidth <= 8) {
			for (i = font->fontheight; i != 0;
			    i--, dp += 1 << ZX_WWIDTH) {
				*dp = *fb << 24;
				fb += fs;
			}
		} else {
			for (i = font->fontheight; i != 0;
			    i--, dp += 1 << ZX_WWIDTH) {
				*dp = *((u_int16_t *)fb) << 16;
				fb += fs;
			}
		}
	}

	/* underline */
	if (ul) {
		dp -= 2 << ZX_WWIDTH;
		*dp = 0xffffffff;
	}

	return 0;
}

void
zx_prom(void *v)
{
	struct zx_softc *sc = v;
	extern struct consdev consdev_prom;

	if (sc->sc_mode != WSDISPLAYIO_MODE_EMUL) {
		/*
	 	 * Select 8-bit mode.
	 	 */
		zx_reset(sc, WSDISPLAYIO_MODE_EMUL);

		/*
	 	 * Go back to prom output for the last few messages, so they
	 	 * will be displayed correctly.
	 	 */
		cn_tab = &consdev_prom;
	}
}
