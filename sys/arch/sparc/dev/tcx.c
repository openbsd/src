/*	$OpenBSD: tcx.c,v 1.13 2002/10/12 01:09:43 krw Exp $	*/
/*	$NetBSD: tcx.c,v 1.8 1997/07/29 09:58:14 fair Exp $ */

/*
 * Copyright (c) 2002 Miodrag Vallat.  All rights reserved.
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
 *  3. All advertising materials mentioning features or use of this software
 *     must display the following acknowledgement:
 *         This product includes software developed by the NetBSD
 *         Foundation, Inc. and its contributors.
 *  4. Neither the name of The NetBSD Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
 * color display (TCX) driver.
 *
 * XXX Use of the vertical retrace interrupt to update the colormap is not
 * enabled by default, as it hangs the system on some machines.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/cpu.h>
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/tcxreg.h>
#include <sparc/dev/sbusvar.h>

#include <dev/cons.h>	/* for prom console hook */

/* per-display variables */
struct tcx_softc {
	struct	sunfb sc_sunfb;			/* common base part */
	struct	sbusdev sc_sd;			/* sbus device */
	struct	rom_reg sc_phys[TCX_NREG];	/* phys addr of h/w */
	volatile struct bt_regs *sc_bt;		/* Brooktree registers */
	volatile struct tcx_thc *sc_thc;	/* THC registers */
	volatile u_int32_t *sc_cplane;		/* S24 control plane */
	union	bt_cmap sc_cmap;		/* Brooktree color map */
	struct	intrhand sc_ih;
	int	sc_nscreens;
};

struct wsscreen_descr tcx_stdscreen = {
	"std",
};

const struct wsscreen_descr *tcx_scrlist[] = {
	&tcx_stdscreen,
	/* XXX other formats? */
};

struct wsscreen_list tcx_screenlist = {
	sizeof(tcx_scrlist) / sizeof(struct wsscreen_descr *), tcx_scrlist
};

int tcx_ioctl(void *, u_long, caddr_t, int, struct proc *);
int tcx_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void tcx_free_screen(void *, void *);
int tcx_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t tcx_mmap(void *, off_t, int);
void tcx_reset(struct tcx_softc *);
void tcx_burner(void *, u_int, u_int);
void tcx_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
static __inline__ void tcx_loadcmap_deferred(struct tcx_softc *, u_int, u_int);
int tcx_intr(void *);
void tcx_prom(void *);

struct wsdisplay_accessops tcx_accessops = {
	tcx_ioctl,
	tcx_mmap,
	tcx_alloc_screen,
	tcx_free_screen,
	tcx_show_screen,
	NULL,   /* load_font */
	NULL,   /* scrollback */
	NULL,   /* getchar */
	tcx_burner,
};

int tcxmatch(struct device *, void *, void *);
void tcxattach(struct device *, struct device *, void *);

struct cfattach tcx_ca = {
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
 * - 24 bits per pixel, in 32-bit words; the high-order byte reads as zero,
 *   and is ignored on writes (so the mode bits can not be altered).
 * - 8 bits per pixel, unpadded; writes to this space do not modify the
 *   other 18 bits, which are hidden.
 *
 * The entry-level tcx found on the SPARCstation 4 can only provide the 8-bit
 * mode.
 */
#define	TCX_CTL_8_MAPPED	0x00000000	/* 8 bits, uses colormap */
#define	TCX_CTL_24_MAPPED	0x01000000	/* 24 bits, uses colormap */
#define	TCX_CTL_24_LEVEL	0x03000000	/* 24 bits, true color */
#define	TCX_CTL_PIXELMASK	0x00ffffff	/* mask for index/level */

/*
 * Match a tcx.
 */
int
tcxmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, "SUNW,tcx"))
		return (0);

	if (ca->ca_bustype == BUS_SBUS)
		return (1);

	return (0);
}

/*
 * Attach a display.
 */
void
tcxattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct tcx_softc *sc = (struct tcx_softc *)self;
	struct confargs *ca = args;
	struct wsemuldisplaydev_attach_args waa;
	int fb_depth, node = 0, i;
	volatile struct bt_regs *bt;
	int isconsole = 0;
	char *nam = NULL;

	sc->sc_sunfb.sf_flags = self->dv_cfdata->cf_flags & FB_USERMASK;

	if (ca->ca_ra.ra_nreg < TCX_NREG)
		panic("tcx: expected %d registers, got %d", TCX_NREG,
		    ca->ca_ra.ra_nreg);

	/* Copy register address spaces */
	for (i = 0; i < TCX_NREG; i++)
		sc->sc_phys[i] = ca->ca_ra.ra_reg[i];

	sc->sc_bt = bt = (volatile struct bt_regs *)
	    mapiodev(&ca->ca_ra.ra_reg[TCX_REG_CMAP], 0, sizeof *sc->sc_bt);
	sc->sc_thc = (volatile struct tcx_thc *)
	    mapiodev(&ca->ca_ra.ra_reg[TCX_REG_THC],
	        0x1000, sizeof *sc->sc_thc);

	switch (ca->ca_bustype) {
	case BUS_SBUS:
		node = ca->ca_ra.ra_node;
		nam = getpropstring(node, "model");
		break;

	default:
		printf("TCX on bus 0x%x?\n", ca->ca_bustype);
		return;
	}

	printf(": %s\n", nam);

	isconsole = node == fbnode;

	if (ISSET(sc->sc_sunfb.sf_flags, FB_FORCELOW))
		fb_depth = 8;
	else
		fb_depth = node_has_property(node, "tcx-8-bit") ?  8 : 32;

	fb_setsize(&sc->sc_sunfb, fb_depth, 1152, 900, node, ca->ca_bustype);

	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(&ca->ca_ra.ra_reg[
	    sc->sc_sunfb.sf_depth == 8 ? TCX_REG_DFB8 : TCX_REG_DFB24],
	    0, round_page(sc->sc_sunfb.sf_fbsize));
	
	/* map the control plane for S24 framebuffers */
	if (sc->sc_sunfb.sf_depth != 8) {
		sc->sc_cplane = mapiodev(&ca->ca_ra.ra_reg[TCX_REG_RDFB32],
		    0, round_page(sc->sc_sunfb.sf_fbsize));
	}

	/* reset cursor & frame buffer controls */
	tcx_reset(sc);

	/* grab initial (current) color map */
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3; i++)
		((char *)&sc->sc_cmap)[i] = bt->bt_cmap >> 24;

	/* enable video */
	tcx_burner(sc, 1, 0);

	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, isconsole);

	tcx_stdscreen.capabilities = sc->sc_sunfb.sf_ro.ri_caps;
	tcx_stdscreen.nrows = sc->sc_sunfb.sf_ro.ri_rows;
	tcx_stdscreen.ncols = sc->sc_sunfb.sf_ro.ri_cols;
	tcx_stdscreen.textops = &sc->sc_sunfb.sf_ro.ri_ops;

	if (ISSET(sc->sc_sunfb.sf_flags, TCX_INTR)) {
		sc->sc_ih.ih_fun = tcx_intr;
		sc->sc_ih.ih_arg = sc;
		intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih,
		    IPL_FB);
	}

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, &tcx_stdscreen, -1,
		    tcx_setcolor, tcx_burner);
	}

	sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);

	printf("%s: %dx%d, depth %d, id %d, rev %d, sense %d\n",
	    self->dv_xname, sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height,
	    sc->sc_sunfb.sf_depth,
	    (sc->sc_thc->thc_config & THC_CFG_FBID) >> THC_CFG_FBID_SHIFT,
	    (sc->sc_thc->thc_config & THC_CFG_REV) >> THC_CFG_REV_SHIFT,
	    (sc->sc_thc->thc_config & THC_CFG_SENSE) >> THC_CFG_SENSE_SHIFT
	);

	waa.console = isconsole;
	waa.scrdata = &tcx_screenlist;
	waa.accessops = &tcx_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);
}

int
tcx_ioctl(dev, cmd, data, flags, p)
	void *dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct tcx_softc *sc = dev;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUN24;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width = sc->sc_sunfb.sf_width;
		wdf->depth = sc->sc_sunfb.sf_depth;
		wdf->cmsize = (sc->sc_sunfb.sf_depth == 8) ? 256 : 0;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		if (sc->sc_sunfb.sf_depth == 8) {
			cm = (struct wsdisplay_cmap *)data;
			error = bt_getcmap(&sc->sc_cmap, cm);
			if (error)
				return (error);
		}
		break;
	case WSDISPLAYIO_PUTCMAP:
		if (sc->sc_sunfb.sf_depth == 8) {
			cm = (struct wsdisplay_cmap *)data;
			error = bt_putcmap(&sc->sc_cmap, cm);
			if (error)
				return (error);
			if (ISSET(sc->sc_sunfb.sf_flags, TCX_INTR)) {
				tcx_loadcmap_deferred(sc, cm->index, cm->count);
			} else {
				bt_loadcmap(&sc->sc_cmap, sc->sc_bt,
				    cm->index, cm->count, 1);
			}
		}
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return (-1);	/* not supported yet */
	}

	return (0);
}

/*
 * Clean up hardware state (e.g., after bootup or after X crashes).
 */
void
tcx_reset(sc)
	struct tcx_softc *sc;
{
	volatile struct bt_regs *bt;

	/* Hide the cursor, just in case */
	sc->sc_thc->thc_cursoraddr = THC_CURSOFF;

	/* Enable cursor in Brooktree DAC. */
	bt = sc->sc_bt;
	bt->bt_addr = 0x06 << 24;
	bt->bt_ctrl |= 0x03 << 24;

	/*
	 * Select 24-bit mode if appropriate.
	 */
	if (sc->sc_sunfb.sf_depth != 8) {
		volatile u_int32_t *cptr;
		u_int32_t pixel;
		int ramsize;

		ramsize = sc->sc_sunfb.sf_fbsize / sizeof(u_int32_t);
		cptr = sc->sc_cplane;

		/*
		 * Since the prom output so far has only been white (0)
		 * or black (255), we can promote the 8 bit plane contents
		 * to full color.
		 * Of course, the 24 bit plane uses 0 for black, so a
		 * reversal is necessary. Blame Sun.
		 */
		while (ramsize-- != 0) {
			pixel = 255 - ((*cptr & TCX_CTL_PIXELMASK) & 0xff);
			pixel = (pixel << 16) | (pixel << 8) | pixel;
			*cptr++ = pixel | TCX_CTL_24_LEVEL;
		}

		shutdownhook_establish(tcx_prom, sc);
	}
}

void
tcx_prom(v)
	void *v;
{
	struct tcx_softc *sc = v;
	extern struct consdev consdev_prom;

	/*
	 * Select 8-bit mode.
	 */
	if (sc->sc_sunfb.sf_depth != 8) {
		volatile u_int32_t *cptr;
		u_int32_t pixel;
		int ramsize;

		ramsize = sc->sc_sunfb.sf_fbsize / sizeof(u_int32_t);
		cptr = sc->sc_cplane;

		while (ramsize-- != 0) {
			pixel = (*cptr & TCX_CTL_PIXELMASK);
			*cptr++ = pixel | TCX_CTL_8_MAPPED;
		}
	}

	/*
	 * Go back to prom output for the last few messages, so they
	 * will be displayed correctly.
	 */
	cn_tab = &consdev_prom;
}

void
tcx_burner(v, on, flags)
	void *v;
	u_int on, flags;
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

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
tcx_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct tcx_softc *sc = v;
	int reg;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		reg = (sc->sc_sunfb.sf_depth == 8) ?
		    TCX_REG_DFB8 : TCX_REG_DFB24;
		return (REG2PHYS(&sc->sc_phys[reg], offset) | PMAP_NC);
	}

	return (-1);	/* not a user-map offset */
}

void
tcx_setcolor(v, index, r, g, b)
	void *v;
	u_int index;
	u_int8_t r, g, b;
{
	struct tcx_softc *sc = v;

	bt_setcolor(&sc->sc_cmap, sc->sc_bt, index, r, g, b, 1);
}

int
tcx_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct tcx_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_sunfb.sf_ro;
	*curyp = 0;
	*curxp = 0;
	if (sc->sc_sunfb.sf_depth == 8) {
		sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
		    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, attrp);
	} else {
		sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
		    0, 0, 0, attrp);
	}
	sc->sc_nscreens++;
	return (0);
}

void
tcx_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct tcx_softc *sc = v;

	sc->sc_nscreens--;
}

int
tcx_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

static __inline__ void
tcx_loadcmap_deferred(struct tcx_softc *sc, u_int start, u_int ncolors)
{
	u_int32_t thcm;

	thcm = sc->sc_thc->thc_hcmisc;
	thcm &= ~THC_MISC_RESET;
	thcm |= THC_MISC_INTEN;
	sc->sc_thc->thc_hcmisc = thcm;
}

int
tcx_intr(v)
	void *v;
{
	struct tcx_softc *sc = v;
	u_int32_t thcm;

	thcm = sc->sc_thc->thc_hcmisc;
	if ((thcm & (THC_MISC_INTEN | THC_MISC_INTR)) !=
	    (THC_MISC_INTEN | THC_MISC_INTR)) {
		/* Not expecting an interrupt, it's not for us. */
		return (0);
	}

	/* Acknowledge the interrupt and disable it. */
	thcm &= ~(THC_MISC_RESET | THC_MISC_INTEN);
	thcm |= THC_MISC_INTR;
	sc->sc_thc->thc_hcmisc = thcm;

	bt_loadcmap(&sc->sc_cmap, sc->sc_bt, 0, 256, 1);

	return (1);
}
