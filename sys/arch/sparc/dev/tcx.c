/*	$OpenBSD: tcx.c,v 1.45 2010/05/13 20:35:43 miod Exp $	*/
/*	$NetBSD: tcx.c,v 1.8 1997/07/29 09:58:14 fair Exp $ */

/*
 * Copyright (c) 2002, 2003, 2008 Miodrag Vallat.  All rights reserved.
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
 *
 *  Copyright (c) 1996 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Paul Kranenburg.
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
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Color display (TCX) driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/cpu.h>
#include <machine/conf.h>
#include <sparc/sparc/asm.h>
#include <machine/ctlreg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/tcxreg.h>
#include <dev/ic/bt458reg.h>

#include <dev/cons.h>	/* for prom console hook */

/* per-display variables */
struct tcx_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	rom_reg sc_phys[2];	/* copy of prom ranges for mmap */
	volatile struct bt_regs *sc_bt;	/* Brooktree registers */
	volatile struct tcx_thc *sc_thc;/* THC registers */
	volatile u_int8_t *sc_dfb8;	/* 8 bit plane */
	paddr_t	sc_cplane;		/* S24 control plane PA */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	struct	intrhand sc_ih;

	/* acceleration parts */
	paddr_t	sc_stipple;		/* Stipple space PA */
	paddr_t	sc_blit;		/* Blitter space PA */
	int	sc_blit_width;		/* maximal blith width */
	int	(*sc_plain_copycols)(void *, int, int, int, int);
};

void	tcx_accel_init(struct tcx_softc *, struct confargs *);
void	tcx_accel_plug(struct tcx_softc *, struct confargs *);
void	tcx_blit(struct tcx_softc *, uint32_t, uint32_t, int);
void	tcx_burner(void *, u_int, u_int);
int	tcx_copyrows(void *, int, int, int);
int	tcx_copycols(void *, int, int, int, int);
int	tcx_do_cursor(struct rasops_info *);
int	tcx_erasecols(void *, int, int, int, long);
int	tcx_eraserows(void *, int, int, long);
int	tcx_intr(void *);
int	tcx_ioctl(void *, u_long, caddr_t, int, struct proc *);
static __inline__
void	tcx_loadcmap_deferred(struct tcx_softc *, u_int, u_int);
paddr_t	tcx_mmap(void *, off_t, int);
void	tcx_prom(void *);
int	tcx_putchar(void *, int, int, u_int, long);
void	tcx_reset(struct tcx_softc *, int);
void	tcx_s24_reset(struct tcx_softc *, int);
void	tcx_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
void	tcx_stipple(struct tcx_softc *, int, int, int, int, int);

struct wsdisplay_accessops tcx_accessops = {
	tcx_ioctl,
	tcx_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,   /* load_font */
	NULL,   /* scrollback */
	NULL,   /* getchar */
	tcx_burner,
	NULL	/* pollc */
};

int	tcxmatch(struct device *, void *, void *);
void	tcxattach(struct device *, struct device *, void *);

const struct cfattach tcx_ca = {
	sizeof(struct tcx_softc), tcxmatch, tcxattach
};

struct cfdriver tcx_cd = {
	NULL, "tcx", DV_DULL
};

/*
 * There are three ways to access the framebuffer memory of the S24:
 * - 26 bits per pixel, in 32-bit words; the low-order 24 bits are blue,
 *   green and red values, and the other two bits select the display modes,
 *   per pixel.
 *   This is the view we'll use to initialize the frame buffer.
 * - 24 bits per pixel, in 32-bit words; the high-order byte reads as zero,
 *   and is ignored on writes (so the mode bits can not be altered).
 *   This is the view available via mmap, for the X server.
 * - 8 bits per pixel, unpadded; writes to this space do not modify the
 *   other 18 bits, which are hidden.
 *   This is the view used for the console emulation mode, as well as for
 *   the X server on 8-bit only devices.
 *
 * The entry-level tcx found on the SPARCstation 4 can only provide the 8-bit
 * mode.
 *
 * Both flavours can be told out by the `tcx-8-bit' property; also, on
 * 8-bit tcx, the 24 bit color regions have a size of zero (or one with
 * the most recent PROM versions).
 */
#define	TCX_CTL_8_MAPPED	0x00000000	/* 8 bits, uses colormap */
#define	TCX_CTL_24_MAPPED	0x01000000	/* 24 bits, uses colormap */
#define	TCX_CTL_24_LEVEL	0x03000000	/* 24 bits, direct color */
#define	TCX_CTL_PIXELMASK	0x00ffffff	/* mask for index/level */

int
tcxmatch(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, "SUNW,tcx") != 0)
		return (0);

	return (1);
}

void
tcxattach(struct device *parent, struct device *self, void *args)
{
	struct tcx_softc *sc = (struct tcx_softc *)self;
	struct confargs *ca = args;
	int node, pri;
	int isconsole = 0;
	char *nam = NULL;
	vaddr_t thc_offset;

	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d: ", pri);

	node = ca->ca_ra.ra_node;
	isconsole = node == fbnode;

	if (ca->ca_ra.ra_nreg < TCX_NREG) {
		printf("expected %d registers, got %d\n",
		    TCX_NREG, ca->ca_ra.ra_nreg);
		return;
	}

	nam = getpropstring(node, "model");
	if (*nam != '\0')
		printf("%s, ", nam);

	/*
	 * Copy the register address spaces needed for mmap operation.
	 */
	sc->sc_phys[0] = ca->ca_ra.ra_reg[TCX_REG_DFB8];
	sc->sc_phys[1] = ca->ca_ra.ra_reg[TCX_REG_DFB24];

	/*
	 * Can't trust the PROM range len here, it is only 4 bytes on the
	 * 8-bit model. Not that it matters much anyway since we map in
	 * pages.
	 */
	sc->sc_bt = (volatile struct bt_regs *)
	    mapiodev(&ca->ca_ra.ra_reg[TCX_REG_CMAP], 0, sizeof *sc->sc_bt);

	/*
	 * For some reason S24 PROM sets up TEC and THC ranges at the
	 * right addresses (701000 and 301000), while 8 bit TCX doesn't
	 * (and uses 70000 and 30000) - be sure to only compensate on 8 bit
	 * models.
	 */
	if (((vaddr_t)ca->ca_ra.ra_reg[TCX_REG_THC].rr_paddr & 0x1000) != 0)
		thc_offset = 0;
	else
		thc_offset = 0x1000;
	sc->sc_thc = (volatile struct tcx_thc *)
	    mapiodev(&ca->ca_ra.ra_reg[TCX_REG_THC],
	        thc_offset, sizeof *sc->sc_thc);

	/*
	 * Find out frame buffer geometry, so that we know how much
	 * memory to map.
	 */
	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);

	sc->sc_dfb8 = mapiodev(&ca->ca_ra.ra_reg[TCX_REG_DFB8], 0,
	    round_page(sc->sc_sunfb.sf_fbsize));

	/*
	 * If the frame buffer advertizes itself as the 8 bit model, or
	 * if the PROM ranges are too small, limit ourselves to 8 bit
	 * operations.
	 *
	 * Further code needing to know which capabilities the frame buffer
	 * has will rely on sc_cplane being non-zero if 24 bit operation
	 * is possible.
	 */
	if (!node_has_property(node, "tcx-8-bit") &&
	    ca->ca_ra.ra_reg[TCX_REG_RDFB32].rr_len >=
	      sc->sc_sunfb.sf_fbsize * 4) {
		sc->sc_cplane =
		    (paddr_t)ca->ca_ra.ra_reg[TCX_REG_RDFB32].rr_paddr;
	}

	printf("%dx%dx%d\n",
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height,
	    sc->sc_cplane == 0 ? 8 : 24);

	/*
	 * Set up mappings for the acceleration code. This may fail.
	 */
	tcx_accel_init(sc, ca);

	/* reset cursor & frame buffer controls */
	tcx_reset(sc, 8);

	/* enable video */
	tcx_burner(sc, 1, 0);

	sc->sc_sunfb.sf_ro.ri_hw = sc;
	sc->sc_sunfb.sf_ro.ri_bits = (void *)sc->sc_dfb8;

	fbwscons_init(&sc->sc_sunfb, isconsole);
	fbwscons_setcolormap(&sc->sc_sunfb, tcx_setcolor);

	/*
	 * Now plug accelerated console routines, if possible.
	 */
	tcx_accel_plug(sc, ca);

	sc->sc_ih.ih_fun = tcx_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, IPL_FB, self->dv_xname);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, -1);
		shutdownhook_establish(tcx_prom, sc);
	}

	fbwscons_attach(&sc->sc_sunfb, &tcx_accessops, isconsole);
}

int
tcx_ioctl(void *dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct tcx_softc *sc = dev;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int error;

	/*
	 * Note that, although the emulation (text) mode is running in 8-bit
	 * mode, if the frame buffer is able to run in 24-bit mode, it will
	 * be advertized as such.
	 */
	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNTCX;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width = sc->sc_sunfb.sf_width;
		wdf->depth = sc->sc_sunfb.sf_depth;
		wdf->cmsize = sc->sc_cplane == 0 ? 256 : 0;
		break;
	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		if (sc->sc_cplane != 0)
			*(u_int *)data = WSDISPLAYIO_DEPTH_24_32;
		else
			return (-1);
		break;
	case WSDISPLAYIO_LINEBYTES:
		if (sc->sc_cplane == 0)
			*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		else
			*(u_int *)data = sc->sc_sunfb.sf_linebytes * 4;
		break;

	case WSDISPLAYIO_GETCMAP:
		if (sc->sc_cplane == 0) {
			cm = (struct wsdisplay_cmap *)data;
			error = bt_getcmap(&sc->sc_cmap, cm);
			if (error)
				return (error);
		}
		break;
	case WSDISPLAYIO_PUTCMAP:
		if (sc->sc_cplane == 0) {
			cm = (struct wsdisplay_cmap *)data;
			error = bt_putcmap(&sc->sc_cmap, cm);
			if (error)
				return (error);
			tcx_loadcmap_deferred(sc, cm->index, cm->count);
		}
		break;

	case WSDISPLAYIO_SMODE:
		if (*(int *)data == WSDISPLAYIO_MODE_EMUL) {
			/* Back from X11 to text mode */
			tcx_reset(sc, 8);
		} else {
			/* Starting X11, try to switch to 24 bit mode */
			if (sc->sc_cplane != 0)
				tcx_reset(sc, 32);
		}
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
		break;

	default:
		return (-1);	/* not supported yet */
	}

	return (0);
}

void
tcx_reset(struct tcx_softc *sc, int depth)
{
	volatile struct bt_regs *bt;

	/* Hide the cursor, just in case */
	sc->sc_thc->thc_cursoraddr = THC_CURSOFF;

	/* Enable cursor in Brooktree DAC. */
	bt = sc->sc_bt;
	bt->bt_addr = BT_CR << 24;
	bt->bt_ctrl |= (BTCR_DISPENA_OV1 | BTCR_DISPENA_OV0) << 24;

	/*
	 * Change mode if appropriate
	 */
	if (sc->sc_sunfb.sf_depth != depth) {
		if (sc->sc_cplane != 0) {
			tcx_s24_reset(sc, depth);
		}

		if (depth == 8)
			fbwscons_setcolormap(&sc->sc_sunfb, tcx_setcolor);
	}

	sc->sc_sunfb.sf_depth = depth;
}

void
tcx_prom(void *v)
{
	struct tcx_softc *sc = v;
	extern struct consdev consdev_prom;

	if (sc->sc_sunfb.sf_depth != 8) {
		/*
	 	 * Select 8-bit mode.
	 	 */
		tcx_reset(sc, 8);

		/*
	 	 * Go back to prom output for the last few messages, so they
	 	 * will be displayed correctly.
	 	 */
		cn_tab = &consdev_prom;
	}
}

void
tcx_burner(void *v, u_int on, u_int flags)
{
	struct tcx_softc *sc = v;
	int s;
	u_int32_t thcm;

	s = splhigh();
	thcm = sc->sc_thc->thc_hcmisc;
	if (on) {
		thcm |= THC_MISC_VIDEN;
		thcm &= ~(THC_MISC_VSYNC_DISABLE | THC_MISC_HSYNC_DISABLE);
	} else {
		thcm &= ~THC_MISC_VIDEN;
		if (flags & WSDISPLAY_BURN_VBLANK)
			thcm |= THC_MISC_VSYNC_DISABLE | THC_MISC_HSYNC_DISABLE;
	}
	sc->sc_thc->thc_hcmisc = thcm;
	splx(s);
}

paddr_t
tcx_mmap(void *v, off_t offset, int prot)
{
	struct tcx_softc *sc = v;
	int regno;

	if (offset & PGOFSET || offset < 0)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (sc->sc_sunfb.sf_depth == 8 && offset < sc->sc_sunfb.sf_fbsize)
		regno = 0;	/* copy of TCX_REG_DFB8 */
	else if (sc->sc_sunfb.sf_depth != 8 &&
	    offset < sc->sc_sunfb.sf_fbsize * 4)
		regno = 1;	/* copy of TCX_REG_RDFB32 */
	else
		return (-1);

	return (REG2PHYS(&sc->sc_phys[regno], offset) | PMAP_NC);
}

void
tcx_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct tcx_softc *sc = v;

	bt_setcolor(&sc->sc_cmap, sc->sc_bt, index, r, g, b, 1);
}

static __inline__ void
tcx_loadcmap_deferred(struct tcx_softc *sc, u_int start, u_int ncolors)
{
	u_int32_t thcm;

	thcm = sc->sc_thc->thc_hcmisc;
	thcm |= THC_MISC_INTEN;
	sc->sc_thc->thc_hcmisc = thcm;
}

int
tcx_intr(void *v)
{
	struct tcx_softc *sc = v;
	u_int32_t thcm;

	thcm = sc->sc_thc->thc_hcmisc;
	if (thcm & THC_MISC_INTEN) {
		thcm &= ~(THC_MISC_INTR | THC_MISC_INTEN);

		/* Acknowledge the interrupt */
		sc->sc_thc->thc_hcmisc = thcm | THC_MISC_INTR;

		bt_loadcmap(&sc->sc_cmap, sc->sc_bt, 0, 256, 1);

		/* Disable further interrupts now */
		sc->sc_thc->thc_hcmisc = thcm;

		return (1);
	}

	return (0);
}

/*
 * Switch video mode and clear screen
 */
void
tcx_s24_reset(struct tcx_softc *sc, int depth)
{
	struct rasops_info *ri = &sc->sc_sunfb.sf_ro;
	uint32_t pixel;
	paddr_t dst;
	int n;

	if (depth == 8)
		pixel = TCX_CTL_8_MAPPED | (ri->ri_devcmap[WSCOL_WHITE] & 0xff);
	else
		pixel = TCX_CTL_24_LEVEL | 0xffffff;

	/*
	 * Set the first 32 pixels as white in the intended mode, using the
	 * control plane.
	 */
	dst = sc->sc_cplane;
	for (n = 32; n != 0; n--) {
		sta(dst, ASI_BYPASS, pixel);
		dst += 4;
	}

	/*
	 * Do the remaining pixels: either with the blitter if we can use it,
	 * or continuing manual writes if we can't.
	 */
	if (sc->sc_blit != 0) {
		dst = sc->sc_blit + (32 << 3);
		pixel = ((sc->sc_blit_width - 1) << 24) | 0;
		for (n = sc->sc_sunfb.sf_fbsize - 32; n != 0;
		    n -= sc->sc_blit_width) {
			stda(dst, ASI_BYPASS, pixel);
			dst += sc->sc_blit_width << 3;
		}
	} else {
		/* this relies on video memory being contiguous */
		for (n = sc->sc_sunfb.sf_fbsize - 32; n != 0; n--) {
			sta(dst, ASI_BYPASS, pixel);
			dst += 4;
		}
	}
}

/*
 * Accelerated console operations
 *
 * Most of the TCX and S24 frame buffers can not perform simple ROPs
 * besides GXcopy. Those which can, have a ``stip-rop'' property,
 * and appear to support at least copy and invert rops.
 *
 * We use the blitter for block moves: copycols, copyrows; and the
 * stipple for solid fills: erasecols, eraserows, putchar.
 *
 * This work has been made possible thanks to the information collected in
 *   http://ftp.rodents-montreal.org/mouse/docs/Sun/S24/memory-map
 * and experiments.
 *
 * It turns out that frame buffer memory is contiguous, so addressing
 * of a given (x,y) pixel is always (y * width + x), regardless of the
 * actual frame buffer resolution.
 */

void
tcx_accel_init(struct tcx_softc *sc, struct confargs *ca)
{
	int stipple_align, regno;

	/*
	 * On S24, try and map raw blit and raw stipple spaces.
	 * We prefer the raw spaces so that we can eventually switch
	 * between 8 bit and 24 bit modes with blitter operations.
	 *
	 * However, on 8-bit TCX, these spaces are missing (and empty!),
	 * so we should fallback to non-raw spaces in this case.
	 *
	 * Since this frame buffer can only exist on SS4 and SS5,
	 * we can rely upon the fact this code will only run on sun4m,
	 * and use stda() bypassing the MMU to access these spaces,
	 * instead of mapping them (8MB KVA each, after all, even more
	 * on an SS4 with the resolution extender VSIMM).
	 */

	sc->sc_blit_width = getpropint(ca->ca_ra.ra_node, "blit-width", 0);
	if (sc->sc_blit_width > 5)
		sc->sc_blit_width = 5;	/* paranoia */
	if (sc->sc_blit_width <= 3)	/* not worth until more than 8 pixels */
		sc->sc_blit_width = 0;
	else {
		sc->sc_blit_width = 1 << sc->sc_blit_width;

		regno = sc->sc_cplane == 0 ? TCX_REG_BLIT : TCX_REG_RBLIT;
		if (ca->ca_ra.ra_reg[regno].rr_len >=
		    sc->sc_sunfb.sf_fbsize * 8)
			sc->sc_blit = (paddr_t)ca->ca_ra.ra_reg[regno].rr_paddr;
	}

	/*
	 * Do not assume the stipple space is usable unless there is a
	 * `stipple-align' property.  Also, don't try to use it if the
	 * alignment is not 32 - our code may not behave correctly.
	 */

	stipple_align = getpropint(ca->ca_ra.ra_node, "stipple-align", 0);
	if (stipple_align == 5) {
		regno = sc->sc_cplane == 0 ? TCX_REG_STIP : TCX_REG_RSTIP;
		if (ca->ca_ra.ra_reg[regno].rr_len >=
		    sc->sc_sunfb.sf_fbsize * 8)
			sc->sc_stipple =
			    (paddr_t)ca->ca_ra.ra_reg[regno].rr_paddr;
	}
}

/*
 * After rasops has initialized, override its basic operations with
 * accelerated routines whenever possible (this will depend on the
 * frame buffer capabilities and our success at using blit and
 * stipple spaces).
 */
void
tcx_accel_plug(struct tcx_softc *sc, struct confargs *ca)
{
	struct rasops_info *ri = &sc->sc_sunfb.sf_ro;

	if (sc->sc_stipple != 0) {
		sc->sc_sunfb.sf_ro.ri_ops.eraserows = tcx_eraserows;
		sc->sc_sunfb.sf_ro.ri_ops.erasecols = tcx_erasecols;
		if (node_has_property(ca->ca_ra.ra_node, "stip-rop")) {
			/* needs GXinvert support */
			sc->sc_sunfb.sf_ro.ri_do_cursor = tcx_do_cursor;
		}
		if (ri->ri_font->fontwidth <= 16) {
			/*
			 * The code in tcx_putchar really can handle up
			 * to 32 bit font width if one extends the font
			 * bits loading part to support more than 16 bits,
			 * but then there is no such font in wsfont at the
			 * moment, so why bother.
			 */
			sc->sc_sunfb.sf_ro.ri_ops.putchar = tcx_putchar;
		}
	}

	if (sc->sc_blit != 0) {
		sc->sc_sunfb.sf_ro.ri_ops.copyrows = tcx_copyrows;
		sc->sc_plain_copycols = sc->sc_sunfb.sf_ro.ri_ops.copycols;
		sc->sc_sunfb.sf_ro.ri_ops.copycols = tcx_copycols;
	}
}

/*
 * Blitter operations
 *
 * Since the blitter only operates on 1 pixel height areas, we need
 * to invoke it many times, and handle overlapping regions ourselves.
 */

int
tcx_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct tcx_softc *sc = ri->ri_hw;
	int h;
	uint32_t dstaddr, srcaddr;

	if (dst > src && dst < src + n) {
		/* Areas overlap, do it the slow but safe way */
		return (*sc->sc_plain_copycols)(cookie, row, src, dst, n);
	}

	/* Areas do not overlap dangerously, copy forwards */

	/* actual columns */
	n *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;

	row *= ri->ri_font->fontheight;
	srcaddr = dstaddr = ri->ri_xorigin +
	    (ri->ri_yorigin + row) * sc->sc_sunfb.sf_width;
	srcaddr += src;
	dstaddr += dst;

	for (h = ri->ri_font->fontheight; h != 0; h--) {
		tcx_blit(sc, dstaddr, srcaddr, n);
		srcaddr += sc->sc_sunfb.sf_width;
		dstaddr += sc->sc_sunfb.sf_width;
	}

	return 0;
}

int
tcx_copyrows(void *cookie, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct tcx_softc *sc = ri->ri_hw;
	int reverse;
	uint32_t dstaddr, srcaddr;

	/* actual lines */
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;
	n *= ri->ri_font->fontheight;

	if (dst > src && dst < src + n) {
		/* Areas overlap, copy backwards */
		dst += n - 1;
		src += n - 1;
		reverse = 1;
	} else {
		/* Areas do not overlap dangerously, copy forwards */
		reverse = 0;
	}

	dstaddr = (ri->ri_yorigin + dst) * sc->sc_sunfb.sf_width;
	srcaddr = (ri->ri_yorigin + src) * sc->sc_sunfb.sf_width;

	dstaddr += ri->ri_xorigin;
	srcaddr += ri->ri_xorigin;

	for (; n != 0; n--) {
		tcx_blit(sc, dstaddr, srcaddr, ri->ri_emuwidth);
		if (reverse) {
			dstaddr -= sc->sc_sunfb.sf_width;
			srcaddr -= sc->sc_sunfb.sf_width;
		} else {
			dstaddr += sc->sc_sunfb.sf_width;
			srcaddr += sc->sc_sunfb.sf_width;
		}
	}

	return 0;
}

/*
 * Perform a blit operation, copying the line starting at computed
 * position src to computed position dst, for a length of len pixels.
 */
void
tcx_blit(struct tcx_softc *sc, uint32_t dst, uint32_t src, int len)
{
	int cx;
	uint32_t addr;

	addr = sc->sc_blit + (dst << 3);

	/* do the incomplete chunk first if needed */
	cx = len & (sc->sc_blit_width - 1);
	if (cx == 0)
		cx = sc->sc_blit_width;

	while (len != 0) {
		stda(addr, ASI_BYPASS, ((cx - 1) << 24) | src);
		src += cx;
		addr += cx << 3;
		len -= cx;
		/* and then full steam ahead for the others */
		cx = sc->sc_blit_width;
	}	
}

/*
 * Stipple operations
 */

/* canonical rop values */
#define	GXcopy		0x03
#define	GXinvert	0x0a

#define	STIPPLE_ROP_SHIFT	28

/*
 * Perform a stipple operation rop from (x, y) to (x + cnt - 1, y).
 *
 * We probably should honour the stipple alignment property (stipple-align),
 * in case it is different than 32 (1 << 5). However, due to the way
 * the stipple space is accessed, it is not possible to have a stricter
 * alignment requirement, so let's settle for 32. There probably haven't
 * been TCX boards with relaxed alignment rules anyway.
 */
void
tcx_stipple(struct tcx_softc *sc, int x, int y, int cnt, int rop, int bg)
{
	int rx;		/* aligned x */
	int lbcnt;	/* count of untouched pixels on the left */
	int rbcnt;	/* count of untouched pixels on the right */
	uint32_t wmask;	/* write mask */
	uint32_t soffs;	/* stipple offset */
	uint64_t scmd;

	scmd = rop << STIPPLE_ROP_SHIFT;
	scmd |= TCX_CTL_8_MAPPED | bg;	/* pixel bits, here in 8-bit mode */
	scmd <<= 32;

	/*
	 * The first pass needs to align the position to a 32 pixel
	 * boundary, which explains why the loop is a bit unnatural
	 * at first glance.
	 */
	rx = x & ~(32 - 1);
	soffs = sc->sc_stipple + ((y * sc->sc_sunfb.sf_width + rx) << 3);
	lbcnt = x - rx;
	wmask = 0xffffffff >> lbcnt;

	while (cnt != 0) {
		rbcnt = (32 - lbcnt) - cnt;
		if (rbcnt < 0)
			rbcnt = 0;
		if (rbcnt != 0)
			wmask &= ~((1 << rbcnt) - 1);

		stda(soffs, ASI_BYPASS, scmd | wmask);

		cnt -= (32 - lbcnt) - rbcnt;
		soffs += 32 << 3;

		/* further loops are aligned */
		lbcnt = 0;
		wmask = 0xffffffff;
	}
}

int
tcx_erasecols(void *cookie, int row, int col, int n, long attr)
{
	struct rasops_info *ri = cookie;
	struct tcx_softc *sc = ri->ri_hw;
	int fg, bg, h, cury, sx;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	bg = ri->ri_devcmap[bg] & 0xff;	/* 8 bit palette index */

	n *= ri->ri_font->fontwidth;
	sx = ri->ri_xorigin + col * ri->ri_font->fontwidth;

	cury = ri->ri_yorigin + row * ri->ri_font->fontheight;
	for (h = ri->ri_font->fontheight; h != 0; cury++, h--)
		tcx_stipple(sc, sx, cury, n, GXcopy, bg);

	return 0;
}

int
tcx_eraserows(void *cookie, int row, int n, long attr)
{
	struct rasops_info *ri = cookie;
	struct tcx_softc *sc = ri->ri_hw;
	int fg, bg, x, y, w;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	bg = ri->ri_devcmap[bg] & 0xff;	/* 8 bit palette index */

	if ((n == ri->ri_rows) && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
		n = ri->ri_height;
		x = y = 0;
		w = ri->ri_width;
	} else {
		n *= ri->ri_font->fontheight;
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + row * ri->ri_font->fontheight;
		w = ri->ri_emuwidth;
	}

	for (; n != 0; y++, n--)
		tcx_stipple(sc, x, y, w, GXcopy, bg);

	return 0;
}

int
tcx_do_cursor(struct rasops_info *ri)
{
	struct tcx_softc *sc = ri->ri_hw;
	int x, y, n;

	x = ri->ri_ccol * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = ri->ri_crow * ri->ri_font->fontheight + ri->ri_yorigin;

	for (n = ri->ri_font->fontheight; n != 0; y++, n--)
		tcx_stipple(sc, x, y, ri->ri_font->fontwidth, GXinvert, 0xff);

	return 0;
}

int
tcx_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct tcx_softc *sc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int fg, bg, ul;
	int h, x, y;
	uint8_t *fontbits;
	uint32_t fgpattern, bgpattern;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, &ul);
	fg = ri->ri_devcmap[fg] & 0xff;	/* 8 bit palette index */
	bg = ri->ri_devcmap[bg] & 0xff;	/* 8 bit palette index */

	x = ri->ri_xorigin + col * font->fontwidth;
	y = ri->ri_yorigin + row * font->fontheight;

	if (uc == ' ') {
		/* inline tcx_erasecols(cookie, row, col, 1, attr) */
		for (h = font->fontheight; h != 0; y++, h--)
			tcx_stipple(sc, x, y, font->fontwidth, GXcopy,
			    h == 2 && ul != 0 ? fg : bg);
	} else {
		int rx;
		int lbcnt, rbcnt;
		uint32_t soffs;
		uint64_t stmpl, scmd;

		stmpl = (GXcopy << STIPPLE_ROP_SHIFT) | TCX_CTL_8_MAPPED;

		fontbits = (uint8_t *)font->data +
		    (uc - font->firstchar) * ri->ri_fontscale;

		rx = x & ~(32 - 1);
		lbcnt = x - rx;
		rbcnt = (32 - lbcnt) - font->fontwidth; /* may be negative */
		soffs = sc->sc_stipple +
		    ((y * sc->sc_sunfb.sf_width + rx) << 3);

		for (h = font->fontheight; h != 0; y++, h--) {
			if (font->fontwidth <= 8)
				fgpattern = *(uint8_t *)fontbits >>
				    (8 - font->fontwidth);
			else /* if (font->fontwidth <= 16) */
				fgpattern = *(uint16_t *)fontbits >>
				    (16 - font->fontwidth);
			/* see tcx_accel_plug() for the reason why
			   larger font sizes are not supported, yet */
			fontbits += font->stride;

			/* underline */
			if (ul && h == 2)
				fgpattern = 0xffffffff &
				    ((1 << font->fontwidth) - 1);

			bgpattern = ~fgpattern &
			    ((1 << font->fontwidth) - 1);

			/*
			 * We have a pattern of font->fontwidth bits in
			 * the low bits of `fgpattern' and its one-complement
			 * in `bgpattern'. The bgpattern bits need to be
			 * painted with the background colour, while the
			 * fgpattern bits need to be painted with the
			 * foreground colour.
			 *
			 * The particular character cell position might
			 * span two stipple cells, so we have to account
			 * for this.
			 */

			if (rbcnt >= 0) {
				/* everything fits in one stipple cell. */

				/* foreground */
				scmd = (stmpl | fg) << 32;
				stda(soffs, ASI_BYPASS,
				    scmd | (fgpattern << rbcnt));

				/* background */
				scmd = (stmpl | bg) << 32;
				stda(soffs, ASI_BYPASS,
				    scmd | (bgpattern << rbcnt));
			} else {
				/* needs two stipple cells. */

				/* foreground, first stipple cell */
				scmd = (stmpl | fg) << 32;
				stda(soffs, ASI_BYPASS,
				    scmd | (fgpattern >> -rbcnt));

				/* background, first stipple cell */
				scmd = (stmpl | bg) << 32;
				stda(soffs, ASI_BYPASS,
				    scmd | (bgpattern >> -rbcnt));

				/* rotate patterns, relying on 32 bit size */
				fgpattern <<= (32 + rbcnt);
				bgpattern <<= (32 + rbcnt);

				/* foreground, second stipple cell */
				scmd = (stmpl | fg) << 32;
				stda(soffs + (32 << 3), ASI_BYPASS,
				    scmd | fgpattern);

				/* background, second stipple cell */
				scmd = (stmpl | bg) << 32;
				stda(soffs + (32 << 3), ASI_BYPASS,
				    scmd | bgpattern);
			}

			soffs += sc->sc_sunfb.sf_width << 3;
		}
	}

	return 0;
}
