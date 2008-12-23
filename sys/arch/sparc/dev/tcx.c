/*	$OpenBSD: tcx.c,v 1.32 2008/12/23 20:35:46 miod Exp $	*/
/*	$NetBSD: tcx.c,v 1.8 1997/07/29 09:58:14 fair Exp $ */

/*
 * Copyright (c) 2002, 2003 Miodrag Vallat.  All rights reserved.
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

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/tcxreg.h>
#include <sparc/dev/sbusvar.h>

#include <dev/cons.h>	/* for prom console hook */

/* per-display variables */
struct tcx_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	rom_reg sc_phys[2];	/* copy of prom ranges for mmap */
	volatile struct bt_regs *sc_bt;	/* Brooktree registers */
	volatile struct tcx_thc *sc_thc;/* THC registers */
	volatile u_int8_t *sc_dfb8;	/* 8 bit plane */
	volatile u_int32_t *sc_cplane;	/* S24 control plane */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	struct	intrhand sc_ih;
};

void	tcx_burner(void *, u_int, u_int);
int	tcx_intr(void *);
int	tcx_ioctl(void *, u_long, caddr_t, int, struct proc *);
static __inline__
void	tcx_loadcmap_deferred(struct tcx_softc *, u_int, u_int);
paddr_t	tcx_mmap(void *, off_t, int);
void	tcx_reset(struct tcx_softc *, int);
void	tcx_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
void	tcx_prom(void *);

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
 * 8-bit tcx, the 24 bit color regions have a size of zero.
 */
#define	TCX_CTL_8_MAPPED	0x00000000	/* 8 bits, uses colormap */
#define	TCX_CTL_24_MAPPED	0x01000000	/* 24 bits, uses colormap */
#define	TCX_CTL_24_LEVEL	0x03000000	/* 24 bits, true color */
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
	printf(" pri %d", pri);

	node = ca->ca_ra.ra_node;
	isconsole = node == fbnode;

	if (ca->ca_ra.ra_nreg < TCX_NREG) {
		printf(": expected %d registers, got %d\n",
		    TCX_NREG, ca->ca_ra.ra_nreg);
		return;
	}

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
	 * has will rely on sc_cplane being non NULL if 24 bit operation
	 * is possible.
	 */
	if (node_has_property(node, "tcx-8-bit") ||
	    ca->ca_ra.ra_reg[TCX_REG_RDFB32].rr_len <
	      sc->sc_sunfb.sf_fbsize * 4) {
		sc->sc_cplane = NULL;
	} else {
		sc->sc_cplane = mapiodev(&ca->ca_ra.ra_reg[TCX_REG_RDFB32], 0,
		    round_page(sc->sc_sunfb.sf_fbsize * 4));
	}

	/* reset cursor & frame buffer controls */
	tcx_reset(sc, 8);
	fbwscons_setcolormap(&sc->sc_sunfb, tcx_setcolor);

	/* enable video */
	tcx_burner(sc, 1, 0);

	sc->sc_sunfb.sf_ro.ri_hw = sc;
	sc->sc_sunfb.sf_ro.ri_bits = (void *)sc->sc_dfb8;
	fbwscons_init(&sc->sc_sunfb, isconsole ? 0 : RI_CLEAR);
	fbwscons_setcolormap(&sc->sc_sunfb, tcx_setcolor);

	sc->sc_ih.ih_fun = tcx_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, IPL_FB, self->dv_xname);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, -1);
		shutdownhook_establish(tcx_prom, sc);
	}

	nam = getpropstring(node, "model");
	if (*nam != '\0')
		printf(": %s\n%s", nam, self->dv_xname);
	printf(": %dx%d, id %d, rev %d, sense %d\n",
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height,
	    (sc->sc_thc->thc_config & THC_CFG_FBID) >> THC_CFG_FBID_SHIFT,
	    (sc->sc_thc->thc_config & THC_CFG_REV) >> THC_CFG_REV_SHIFT,
	    (sc->sc_thc->thc_config & THC_CFG_SENSE) >> THC_CFG_SENSE_SHIFT
	);

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
		wdf->cmsize = sc->sc_cplane == NULL ? 256 : 0;
		break;
	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		if (sc->sc_cplane != NULL)
			*(u_int *)data = WSDISPLAYIO_DEPTH_24_32;
		else
			return (-1);
		break;
	case WSDISPLAYIO_LINEBYTES:
		if (sc->sc_cplane == NULL)
			*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		else
			*(u_int *)data = sc->sc_sunfb.sf_linebytes * 4;
		break;

	case WSDISPLAYIO_GETCMAP:
		if (sc->sc_cplane == NULL) {
			cm = (struct wsdisplay_cmap *)data;
			error = bt_getcmap(&sc->sc_cmap, cm);
			if (error)
				return (error);
		}
		break;
	case WSDISPLAYIO_PUTCMAP:
		if (sc->sc_cplane == NULL) {
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
			if (sc->sc_cplane != NULL)
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
	bt->bt_addr = 0x06 << 24;
	bt->bt_ctrl |= 0x03 << 24;

	/*
	 * Change mode if appropriate
	 */
	if (sc->sc_sunfb.sf_depth != depth) {
		if (sc->sc_cplane != NULL) {
			volatile u_int32_t *cptr;
			u_int32_t pixel;
			int ramsize;

			cptr = sc->sc_cplane;
			ramsize = sc->sc_sunfb.sf_fbsize;

			if (depth == 8) {
				while (ramsize-- != 0) {
					pixel = (*cptr & TCX_CTL_PIXELMASK);
					*cptr++ = pixel | TCX_CTL_8_MAPPED;
				}
			} else {
				while (ramsize-- != 0) {
					*cptr++ = TCX_CTL_24_LEVEL;
				}
			}
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
