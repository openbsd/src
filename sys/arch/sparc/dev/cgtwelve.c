/*	$OpenBSD: cgtwelve.c,v 1.3 2002/09/20 11:17:56 fgsch Exp $	*/

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
 */

/*
 * cgtwelve (GS) accelerated 24-bit framebuffer driver.
 *
 * Enough experiments and SMI's cg12reg.h made this possible.
 */

/*
 * The cgtwelve framebuffer is a 3-slot SBUS card, that will fit only in
 * SPARCstation 1, 1+, 2 and 5, or in an xbox SBUS extension (untested).
 *
 * It is a 24-bit 3D accelerated framebuffer made by Matrox, featuring 4MB
 * (regular model) or 8MB (high-res model) of video memory, a complex windowing
 * engine, double buffering modes, three video planes (overlay, 8 bit and 24 bit
 * color), and a lot of colormap combinations.
 *
 * All of this is driven by a set of three Bt462 ramdacs, and a couple of
 * Matrox-specific chips.
 *
 * XXX The high res card is untested.
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

#include <sparc/dev/cgtwelvereg.h>
#include <sparc/dev/sbusvar.h>

#include <dev/cons.h>	/* for prom console hook */

/* per-display variables */
struct cgtwelve_softc {
	struct	sunfb	sc_sunfb;	/* common base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	rom_reg sc_phys;

	volatile struct cgtwelve_dpu *sc_dpu;
	volatile struct cgtwelve_apu *sc_apu;
	volatile struct cgtwelve_dac *sc_ramdac;	/* RAMDAC registers */
	volatile u_char *sc_overlay;	/* overlay or enable plane */
	volatile u_long *sc_inten;	/* true color plane */

	int	sc_highres;
	int	sc_nscreens;
};

struct wsscreen_descr cgtwelve_stdscreen = {
	"std",
	0, 0,	/* will be filled in */
	0,
	0, 0,
	WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

struct wsscreen_descr cgtwelve_monoscreen = {
	"std",
	0, 0,	/* will be filled in */
	0,
	0, 0,
	WSSCREEN_UNDERLINE | WSSCREEN_REVERSE
};

const struct wsscreen_descr *cgtwelve_scrlist[] = {
	&cgtwelve_stdscreen,
};

const struct wsscreen_descr *cgtwelve_monoscrlist[] = {
	&cgtwelve_monoscreen,
};

struct wsscreen_list cgtwelve_screenlist = {
	sizeof(cgtwelve_scrlist) / sizeof(struct wsscreen_descr *),
	    cgtwelve_scrlist
};

struct wsscreen_list cgtwelve_monoscreenlist = {
	sizeof(cgtwelve_monoscrlist) / sizeof(struct wsscreen_descr *),
	    cgtwelve_monoscrlist
};

int cgtwelve_ioctl(void *, u_long, caddr_t, int, struct proc *);
int cgtwelve_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void cgtwelve_free_screen(void *, void *);
int cgtwelve_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t cgtwelve_mmap(void *, off_t, int);
void cgtwelve_reset(struct cgtwelve_softc *);
void cgtwelve_burner(void *, u_int, u_int);
void cgtwelve_prom(void *);

static __inline__ void cgtwelve_ramdac_wraddr(struct cgtwelve_softc *sc,
    u_int32_t addr);
void cgtwelve_initcmap(struct cgtwelve_softc *);
void cgtwelve_darkcmap(struct cgtwelve_softc *);

struct wsdisplay_accessops cgtwelve_accessops = {
	cgtwelve_ioctl,
	cgtwelve_mmap,
	cgtwelve_alloc_screen,
	cgtwelve_free_screen,
	cgtwelve_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgtwelve_burner
};

int cgtwelvematch(struct device *, void *, void *);
void cgtwelveattach(struct device *, struct device *, void *);

struct cfattach cgtwelve_ca = {
	sizeof(struct cgtwelve_softc), cgtwelvematch, cgtwelveattach
};

struct cfdriver cgtwelve_cd = {
	NULL, "cgtwelve", DV_DULL
};


/*
 * Match a cgtwelve.
 */
int
cgtwelvematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	if (ca->ca_bustype == BUS_SBUS)
		return (1);

	return (0);
}

/*
 * Attach a display.
 */
void
cgtwelveattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct cgtwelve_softc *sc = (struct cgtwelve_softc *)self;
	struct confargs *ca = args;
	struct wsemuldisplaydev_attach_args waa;
	int fb_depth, node;
	int isconsole = 0;
	char *ps;

	sc->sc_sunfb.sf_flags = self->dv_cfdata->cf_flags & FB_USERMASK;
	node = ca->ca_ra.ra_node;

	printf(": %s", getpropstring(node, "model"));
	ps = getpropstring(node, "dev_id");
	if (*ps != '\0')
		printf(" (%s)", ps);
	printf("\n");

	isconsole = node == fbnode;

	sc->sc_phys = ca->ca_ra.ra_reg[0];

	/*
	 * Map registers
	 */
	sc->sc_dpu = (struct cgtwelve_dpu *)mapiodev(ca->ca_ra.ra_reg,
	    CG12_OFF_DPU, sizeof(struct cgtwelve_dpu));
	sc->sc_apu = (struct cgtwelve_apu *)mapiodev(ca->ca_ra.ra_reg,
	    CG12_OFF_APU, sizeof(struct cgtwelve_apu));
	sc->sc_ramdac = (struct cgtwelve_dac *)mapiodev(ca->ca_ra.ra_reg,
	    CG12_OFF_DAC, sizeof(struct cgtwelve_dac));

	/*
	 * Compute framebuffer size
	 */
	if (ISSET(sc->sc_sunfb.sf_flags, FB_FORCELOW))
		fb_depth = 1;
	else
		fb_depth = 32;

	fb_setsize(&sc->sc_sunfb, fb_depth, CG12_WIDTH, CG12_HEIGHT,
	    node, ca->ca_bustype);

	if (fb_depth == 1 && sc->sc_sunfb.sf_depth == 32) {
		/* the prom will report depth == 32, so compensate */
		sc->sc_sunfb.sf_depth = 1;
		sc->sc_sunfb.sf_linebytes = sc->sc_sunfb.sf_width / 8;
		sc->sc_sunfb.sf_fbsize = sc->sc_sunfb.sf_height *
		    sc->sc_sunfb.sf_linebytes;
	}

	sc->sc_highres = sc->sc_sunfb.sf_width == CG12_WIDTH_HR;

	/*
	 * Map planes
	 */
	sc->sc_overlay = mapiodev(ca->ca_ra.ra_reg,
	    sc->sc_highres ? CG12_OFF_OVERLAY0_HR : CG12_OFF_OVERLAY0,
	    round_page(sc->sc_highres ? CG12_SIZE_OVERLAY_HR :
	        CG12_SIZE_OVERLAY));
	if (sc->sc_sunfb.sf_depth != 1)
		sc->sc_inten = mapiodev(ca->ca_ra.ra_reg,
		    sc->sc_highres ? CG12_OFF_INTEN_HR : CG12_OFF_INTEN,
		    round_page(sc->sc_highres ? CG12_SIZE_COLOR24_HR :
		        CG12_SIZE_COLOR24));

	/* reset cursor & frame buffer controls */
	cgtwelve_reset(sc);

	if (sc->sc_sunfb.sf_depth != 1) {
		/* enable video */
		cgtwelve_burner(sc, 1, 0);
	}

	if (sc->sc_sunfb.sf_depth == 1)
		sc->sc_sunfb.sf_ro.ri_bits = (void *)sc->sc_overlay;
	else 
		sc->sc_sunfb.sf_ro.ri_bits = (void *)sc->sc_inten;

	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, isconsole);

	cgtwelve_stdscreen.nrows = sc->sc_sunfb.sf_ro.ri_rows;
	cgtwelve_stdscreen.ncols = sc->sc_sunfb.sf_ro.ri_cols;
	cgtwelve_stdscreen.textops = &sc->sc_sunfb.sf_ro.ri_ops;

	if (isconsole) {
		if (sc->sc_sunfb.sf_depth == 1) {
			fbwscons_console_init(&sc->sc_sunfb,
			    &cgtwelve_stdscreen, -1, NULL, NULL);
		} else {
			/*
			 * Since the screen has been cleared, restart at the
			 * top of the screen.
			 */
			fbwscons_console_init(&sc->sc_sunfb,
			    &cgtwelve_stdscreen, 0, NULL, cgtwelve_burner);
		}
	}

	sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);

	printf("%s: %dx%d", self->dv_xname,
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);
	ps = getpropstring(node, "ucoderev");
	if (*ps != '\0')
		printf(", microcode rev. %s", ps);
	printf("\n");

	waa.console = isconsole;
	if (sc->sc_sunfb.sf_depth == 1)
		waa.scrdata = &cgtwelve_monoscreenlist;
	else
		waa.scrdata = &cgtwelve_screenlist;
	waa.accessops = &cgtwelve_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);
}

int
cgtwelve_ioctl(dev, cmd, data, flags, p)
	void *dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgtwelve_softc *sc = dev;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		if (sc->sc_sunfb.sf_depth == 1)
			*(u_int *)data = WSDISPLAY_TYPE_SUNBW;
		else
			*(u_int *)data = WSDISPLAY_TYPE_SUN24;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width = sc->sc_sunfb.sf_width;
		wdf->depth = sc->sc_sunfb.sf_depth;
		wdf->cmsize = 0;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
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
cgtwelve_reset(sc)
	struct cgtwelve_softc *sc;
{
	if (sc->sc_sunfb.sf_depth == 1)
		return;

	/*
	 * Select the overlay plane as sc_overlay.
	 */
	sc->sc_apu->hpage =
	    sc->sc_highres ? CG12_HPAGE_OVERLAY_HR : CG12_HPAGE_OVERLAY;
	sc->sc_apu->haccess = CG12_HACCESS_OVERLAY;
	sc->sc_dpu->pln_sl_host = CG12_PLN_SL_OVERLAY;
	sc->sc_dpu->pln_rd_msk_host = CG12_PLN_RD_OVERLAY;
	sc->sc_dpu->pln_wr_msk_host = CG12_PLN_WR_OVERLAY;

	/*
	 * Do not attempt to somewhat preserve screen contents - reading the
	 * overlay plane and writing to the color plane at the same time is not
	 * reliable, and allocating memory to save a copy of the overlay plane
	 * would be awful.
	 */
	bzero((void *)sc->sc_overlay,
	    sc->sc_highres ? CG12_SIZE_OVERLAY_HR : CG12_SIZE_OVERLAY);

	/*
	 * Select the enable plane as sc_overlay, and clear it.
	 */
	sc->sc_apu->hpage =
	    sc->sc_highres ? CG12_HPAGE_ENABLE_HR : CG12_HPAGE_ENABLE;
	sc->sc_apu->haccess = CG12_HACCESS_ENABLE;
	sc->sc_dpu->pln_sl_host = CG12_PLN_SL_ENABLE;
	sc->sc_dpu->pln_rd_msk_host = CG12_PLN_RD_ENABLE;
	sc->sc_dpu->pln_wr_msk_host = CG12_PLN_WR_ENABLE;

	bzero((void *)sc->sc_overlay,
	    sc->sc_highres ? CG12_SIZE_ENABLE_HR : CG12_SIZE_ENABLE);

	/*
	 * Select the intensity (color) plane, and clear it.
	 */
	sc->sc_apu->hpage =
	    sc->sc_highres ? CG12_HPAGE_24BIT_HR : CG12_HPAGE_24BIT;
	sc->sc_apu->haccess = CG12_HACCESS_24BIT;
	sc->sc_dpu->pln_sl_host = CG12_PLN_SL_24BIT;
	sc->sc_dpu->pln_rd_msk_host = CG12_PLN_RD_24BIT;
	sc->sc_dpu->pln_wr_msk_host = CG12_PLN_WR_24BIT;

	memset((void *)sc->sc_inten, 0x00ffffff,
	    sc->sc_highres ? CG12_SIZE_COLOR24_HR : CG12_SIZE_COLOR24);

	shutdownhook_establish(cgtwelve_prom, sc);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
cgtwelve_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct cgtwelve_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		if (sc->sc_sunfb.sf_depth == 1) {
			return (REG2PHYS(&sc->sc_phys,
			    (sc->sc_highres ? CG12_OFF_OVERLAY0_HR :
			    CG12_OFF_OVERLAY0) + offset) | PMAP_NC);
		} else {
			return (REG2PHYS(&sc->sc_phys,
			    (sc->sc_highres ? CG12_OFF_INTEN_HR :
			    CG12_OFF_INTEN) + offset) | PMAP_NC);
		}
	}

	return (-1);	/* not a user-map offset */
}

int
cgtwelve_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct cgtwelve_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_sunfb.sf_ro;
	*curyp = 0;
	*curxp = 0;
	sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
	     0, 0, 0, attrp);
	sc->sc_nscreens++;
	return (0);
}

void
cgtwelve_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct cgtwelve_softc *sc = v;

	sc->sc_nscreens--;
}

int
cgtwelve_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

/*
 * Simple Bt462 programming routines.
 */

static __inline__ void 
cgtwelve_ramdac_wraddr(struct cgtwelve_softc *sc, u_int32_t addr)
{
	sc->sc_ramdac->addr_lo = (addr & 0xff);
	sc->sc_ramdac->addr_hi = ((addr >> 8) & 0xff);
}

void
cgtwelve_initcmap(sc)
	struct cgtwelve_softc *sc;
{
	u_int32_t c;

	/*
	 * Since we are using the framebuffer in true color mode, there is
	 * theoretically no ramdac initialisation to do.
	 * In practice, we have to load a ramp on each ramdac first.
	 * Fortunately they are latched on each other at this point, so by
	 * loading one single ramp, all of them get initialized.
	 */
	cgtwelve_ramdac_wraddr(sc, 0);
	for (c = 0; c < 256; c++)
		sc->sc_ramdac->color = c | (c << 8) | (c << 16);
}

void
cgtwelve_darkcmap(sc)
	struct cgtwelve_softc *sc;
{
	u_int32_t c;

	cgtwelve_ramdac_wraddr(sc, 0);
	for (c = 0; c < 256; c++)
		sc->sc_ramdac->color = 0;
}

void cgtwelve_burner(v, on, flags)
	void *v;
	u_int on, flags;
{
	struct cgtwelve_softc *sc = v;

	if (sc->sc_sunfb.sf_depth == 1)
		return;

	if (on)
		cgtwelve_initcmap(sc);
	else
		cgtwelve_darkcmap(sc);
}

/*
 * Shutdown hook used to restore PROM-compatible video mode on shutdown,
 * so that the PROM prompt is visible again.
 */
void
cgtwelve_prom(v)
	void *v;
{
	struct cgtwelve_softc *sc = v;
	int c;
	extern struct consdev consdev_prom;

	/*
	 * Select the overlay plane.
	 */
	sc->sc_apu->hpage =
	    sc->sc_highres ? CG12_HPAGE_OVERLAY_HR : CG12_HPAGE_OVERLAY;
	sc->sc_apu->haccess = CG12_HACCESS_OVERLAY;
	sc->sc_dpu->pln_sl_host = CG12_PLN_SL_OVERLAY;
	sc->sc_dpu->pln_rd_msk_host = CG12_PLN_RD_OVERLAY;
	sc->sc_dpu->pln_wr_msk_host = CG12_PLN_WR_OVERLAY;

	/*
	 * Do not touch enable and intensity planes, so that kernel
	 * messages can still be read when back to the prom.
	 * However, we need to fix the colormap, or the prompt will come
	 * back as white on white.
	 */
	cgtwelve_ramdac_wraddr(sc, 0);
	sc->sc_ramdac->color = 0x00ffffff;
	for (c = 1; c < 256; c++)
		sc->sc_ramdac->color = 0x00000000;

	/*
	 * Go back to prom output for the last few messages, so they
	 * will be displayed correctly.
	 */
	cn_tab = &consdev_prom;
}
