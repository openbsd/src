/*	$OpenBSD: cgsix.c,v 1.29 2004/09/29 07:35:11 miod Exp $	*/
/*	$NetBSD: cgsix.c,v 1.33 1997/08/07 19:12:30 pk Exp $ */

/*
 * Copyright (c) 2002 Miodrag Vallat.  All rights reserved.
 * Copyright (c) 2001 Jason L. Wright (jason@thought.net)
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
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 *
 * Copyright (c) 1993
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
 *	@(#)cgsix.c	8.4 (Berkeley) 1/21/94
 */

/*
 * color display (cgsix) driver.
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
#if defined(SUN4)
#include <machine/eeprom.h>
#endif
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/cgsixreg.h>
#include <sparc/dev/sbusvar.h>
#if defined(SUN4)
#include <sparc/dev/pfourreg.h>
#endif

/* per-display variables */
struct cgsix_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	rom_reg sc_phys;	/* phys addr of h/w */
	volatile struct bt_regs *sc_bt;		/* Brooktree registers */
	volatile int *sc_fhc;			/* FHC register */
	volatile struct cgsix_thc *sc_thc;	/* THC registers */
	volatile struct cgsix_fbc *sc_fbc;	/* FBC registers */
	volatile struct cgsix_tec_xxx *sc_tec;	/* TEC registers */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	struct	intrhand sc_ih;
	int	sc_nscreens;
};

struct wsscreen_descr cgsix_stdscreen = {
	"std",
};

const struct wsscreen_descr *cgsix_scrlist[] = {
	&cgsix_stdscreen,
};

struct wsscreen_list cgsix_screenlist = {
	sizeof(cgsix_scrlist) / sizeof(struct wsscreen_descr *), cgsix_scrlist
};

int cgsix_ioctl(void *, u_long, caddr_t, int, struct proc *);
int cgsix_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void cgsix_free_screen(void *, void *);
int cgsix_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t cgsix_mmap(void *, off_t, int);
void cgsix_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
static __inline__ void cgsix_loadcmap_deferred(struct cgsix_softc *,
    u_int, u_int);
void cgsix_reset(struct cgsix_softc *, u_int);
void cgsix_burner(void *, u_int, u_int);
int cgsix_intr(void *);

void cgsix_ras_init(struct cgsix_softc *);
void cgsix_ras_copyrows(void *, int, int, int);
void cgsix_ras_copycols(void *, int, int, int, int);
void cgsix_ras_erasecols(void *, int, int, int, long int);
void cgsix_ras_eraserows(void *, int, int, long int);
void cgsix_ras_do_cursor(struct rasops_info *);

struct wsdisplay_accessops cgsix_accessops = {
	cgsix_ioctl,
	cgsix_mmap,
	cgsix_alloc_screen,
	cgsix_free_screen,
	cgsix_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgsix_burner,
};

int	cgsixmatch(struct device *, void *, void *);
void	cgsixattach(struct device *, struct device *, void *);

struct cfattach cgsix_ca = {
	sizeof(struct cgsix_softc), cgsixmatch, cgsixattach
};

struct cfdriver cgsix_cd = {
	NULL, "cgsix", DV_DULL
};

/*
 * Match a cgsix.
 */
int
cgsixmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	if (strcmp(ra->ra_name, cf->cf_driver->cd_name) &&
	    strcmp(ra->ra_name, "SUNW,cgsix"))
		return (0);

	if (ca->ca_bustype == BUS_SBUS)
		return (1);

#if defined(SUN4)
	if (CPU_ISSUN4 && (ca->ca_bustype == BUS_OBIO)) {
		void *tmp;

		/*
		 * Check for a pfour framebuffer.  This is done somewhat
		 * differently on the cgsix than other pfour framebuffers.
		 */
		bus_untmp();
		tmp = (caddr_t)mapdev(ra->ra_reg, TMPMAP_VA, CGSIX_FHC_OFFSET,
				      NBPG);
		if (probeget(tmp, 4) == -1)
			return (0);

		if (fb_pfour_id(tmp) == PFOUR_ID_FASTCOLOR) {
			cf->cf_flags |= FB_PFOUR;
			return (1);
		}
	}
#endif

	return (0);
}

/*
 * Attach a display.
 */
void
cgsixattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct cgsix_softc *sc = (struct cgsix_softc *)self;
	struct confargs *ca = args;
	struct wsemuldisplaydev_attach_args waa;
	int node = 0, i;
	volatile struct bt_regs *bt;
	int isconsole = 0, sbus = 1;
	char *nam = NULL;
	u_int fhcrev;

	sc->sc_sunfb.sf_flags = self->dv_cfdata->cf_flags;

	/*
	 * Map just BT, FHC, FBC, THC, and video RAM.
	 */
	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bt = bt = (volatile struct bt_regs *)
	   mapiodev(ca->ca_ra.ra_reg, CGSIX_BT_OFFSET, CGSIX_BT_SIZE);
	sc->sc_fhc = (volatile int *)
	   mapiodev(ca->ca_ra.ra_reg, CGSIX_FHC_OFFSET, CGSIX_FHC_SIZE);
	sc->sc_thc = (volatile struct cgsix_thc *)
	   mapiodev(ca->ca_ra.ra_reg, CGSIX_THC_OFFSET, CGSIX_THC_SIZE);
	sc->sc_fbc = (volatile struct cgsix_fbc *)
	   mapiodev(ca->ca_ra.ra_reg, CGSIX_FBC_OFFSET, CGSIX_FBC_SIZE);
	sc->sc_tec = (volatile struct cgsix_tec_xxx *)
	   mapiodev(ca->ca_ra.ra_reg, CGSIX_TEC_OFFSET, CGSIX_TEC_SIZE);

	switch (ca->ca_bustype) {
	case BUS_OBIO:
		sbus = node = 0;
		if (ISSET(sc->sc_sunfb.sf_flags, FB_PFOUR))
			nam = "cgsix/p4";
		else
			nam = "cgsix";

#if defined(SUN4M)
		if (CPU_ISSUN4M) {   /* 4m has framebuffer on obio */
			node = ca->ca_ra.ra_node;
			nam = getpropstring(node, "model");
			break;
		}
#endif
		break;

	case BUS_VME32:
	case BUS_VME16:
		sbus = node = 0;
		nam = "cgsix";
		break;

	case BUS_SBUS:
		node = ca->ca_ra.ra_node;
		nam = getpropstring(node, "model");
		break;

	case BUS_MAIN:
		printf("cgsix on mainbus?\n");
		return;
	}

	printf(": %s", nam);

#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		int constype = ISSET(sc->sc_sunfb.sf_flags, FB_PFOUR) ?
		    EE_CONS_P4OPT : EE_CONS_COLOR;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->eeConsole == constype)
			isconsole = 1;
	}
#endif

	if (CPU_ISSUN4COR4M)
		isconsole = node == fbnode;

	fhcrev = (*sc->sc_fhc >> FHC_REV_SHIFT) &
	    (FHC_REV_MASK >> FHC_REV_SHIFT);

	sc->sc_ih.ih_fun = cgsix_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih, IPL_FB,
	    self->dv_xname);

	/* reset cursor & frame buffer controls */
	cgsix_reset(sc, fhcrev);

	/* grab initial (current) color map */
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3; i++)
		((char *)&sc->sc_cmap)[i] = bt->bt_cmap >> 24;

	/* enable video */
	cgsix_burner(sc, 1, 0);

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);
        sc->sc_sunfb.sf_ro.ri_bits = mapiodev(ca->ca_ra.ra_reg,
	    CGSIX_VID_OFFSET, round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	/*
	 * If the framebuffer width is under 1024x768, we will switch from the
	 * PROM font to the more adequate 8x16 font here.
	 * However, we need to adjust two things in this case:
	 * - the display row should be overrided from the current PROM metrics,
	 *   to prevent us from overwriting the last few lines of text.
	 * - if the 80x34 screen would make a large margin appear around it,
	 *   choose to clear the screen rather than keeping old prom output in
	 *   the margins.
	 * XXX there should be a rasops "clear margins" feature
	 */
	fbwscons_init(&sc->sc_sunfb, isconsole &&
	    (sc->sc_sunfb.sf_width >= 1024) ? 0 : RI_CLEAR);
	fbwscons_setcolormap(&sc->sc_sunfb, cgsix_setcolor);

	/*
	 * Old rev. cg6 cards do not like the current acceleration code.
	 *
	 * Some hints from Sun point out at timing and cache problems, which
	 * will be investigated later.
	 */
	if (fhcrev >= 5) {
		sc->sc_sunfb.sf_ro.ri_ops.copyrows = cgsix_ras_copyrows;
		sc->sc_sunfb.sf_ro.ri_ops.copycols = cgsix_ras_copycols;
		sc->sc_sunfb.sf_ro.ri_ops.eraserows = cgsix_ras_eraserows;
		sc->sc_sunfb.sf_ro.ri_ops.erasecols = cgsix_ras_erasecols;
		sc->sc_sunfb.sf_ro.ri_do_cursor = cgsix_ras_do_cursor;
		cgsix_ras_init(sc);
	}

	cgsix_stdscreen.capabilities = sc->sc_sunfb.sf_ro.ri_caps;
	cgsix_stdscreen.nrows = sc->sc_sunfb.sf_ro.ri_rows;
	cgsix_stdscreen.ncols = sc->sc_sunfb.sf_ro.ri_cols;
	cgsix_stdscreen.textops = &sc->sc_sunfb.sf_ro.ri_ops;

	printf(", %dx%d, rev %d\n", sc->sc_sunfb.sf_width,
	    sc->sc_sunfb.sf_height, fhcrev);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, &cgsix_stdscreen,
		    sc->sc_sunfb.sf_width >= 1024 ? -1 : 0, cgsix_burner);
	}

#if defined(SUN4C) || defined(SUN4M)
	if (sbus)
		sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);
#endif

	waa.console = isconsole;
	waa.scrdata = &cgsix_screenlist;
	waa.accessops = &cgsix_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);
}

int
cgsix_ioctl(dev, cmd, data, flags, p)
	void *dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgsix_softc *sc = dev;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNCG6;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width  = sc->sc_sunfb.sf_width;
		wdf->depth  = sc->sc_sunfb.sf_depth;
		wdf->cmsize = 256;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = bt_getcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		break;

	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = bt_putcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		cgsix_loadcmap_deferred(sc, cm->index, cm->count);
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

int
cgsix_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct cgsix_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_sunfb.sf_ro;
	*curyp = 0;
	*curxp = 0;
	sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
	    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, attrp);
	sc->sc_nscreens++;
	return (0);
}

void
cgsix_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct cgsix_softc *sc = v;

	sc->sc_nscreens--;
}

int
cgsix_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

/*
 * Clean up hardware state (e.g., after bootup or after X crashes).
 */
void
cgsix_reset(sc, fhcrev)
	struct cgsix_softc *sc;
	u_int fhcrev;
{
	volatile struct cgsix_tec_xxx *tec;
	int fhc;
	volatile struct bt_regs *bt;

	/* hide the cursor, just in case */
	sc->sc_thc->thc_cursxy = THC_CURSOFF;

	/* turn off frobs in transform engine (makes X11 work) */
	tec = sc->sc_tec;
	tec->tec_mv = 0;
	tec->tec_clip = 0;
	tec->tec_vdc = 0;

	/* take care of hardware bugs in old revisions */
	if (fhcrev < 5) {
		/*
		 * Keep current resolution; set cpu to 68020, set test
		 * window (size 1Kx1K), and for rev 1, disable dest cache.
		 */
		fhc = (*sc->sc_fhc & FHC_RES_MASK) | FHC_CPU_68020 |
		    FHC_TEST |
		    (11 << FHC_TESTX_SHIFT) | (11 << FHC_TESTY_SHIFT);
		if (fhcrev < 2)
			fhc |= FHC_DST_DISABLE;
		*sc->sc_fhc = fhc;
	}

	/* Enable cursor in Brooktree DAC. */
	bt = sc->sc_bt;
	bt->bt_addr = 0x06 << 24;
	bt->bt_ctrl |= 0x03 << 24;
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
cgsix_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct cgsix_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, offset + CGSIX_VID_OFFSET) |
		    PMAP_NC);
	}

	return (-1);	/* not a user-map offset */
}

void
cgsix_setcolor(v, index, r, g, b)
	void *v;
	u_int index;
	u_int8_t r, g, b;
{
	struct cgsix_softc *sc = v;

	bt_setcolor(&sc->sc_cmap, sc->sc_bt, index, r, g, b, 1);
}

static __inline__ void
cgsix_loadcmap_deferred(struct cgsix_softc *sc, u_int start, u_int ncolors)
{
	u_int32_t thcm;

	thcm = sc->sc_thc->thc_misc;
	thcm &= ~THC_MISC_RESET;
	thcm |= THC_MISC_INTEN;
	sc->sc_thc->thc_misc = thcm;
}

void
cgsix_burner(v, on, flags)
	void *v;
	u_int on, flags;
{
	struct cgsix_softc *sc = v;
	int s;
	u_int32_t thcm;

	s = splhigh();
	thcm = sc->sc_thc->thc_misc;
	if (on)
		thcm |= THC_MISC_VIDEN | THC_MISC_SYNCEN;
	else {
		thcm &= ~THC_MISC_VIDEN;
		if (flags & WSDISPLAY_BURN_VBLANK)
			thcm &= ~THC_MISC_SYNCEN;
	}
	sc->sc_thc->thc_misc = thcm;
	splx(s);
}

int
cgsix_intr(v)
	void *v;
{
	struct cgsix_softc *sc = v;
	u_int32_t thcm;

	thcm = sc->sc_thc->thc_misc;
	if ((thcm & (THC_MISC_INTEN | THC_MISC_INTR)) !=
	    (THC_MISC_INTEN | THC_MISC_INTR)) {
		/* Not expecting an interrupt, it's not for us. */
		return (0);
	}

	/* Acknowledge the interrupt and disable it. */
	thcm &= ~(THC_MISC_RESET | THC_MISC_INTEN);
	thcm |= THC_MISC_INTR;
	sc->sc_thc->thc_misc = thcm;
	bt_loadcmap(&sc->sc_cmap, sc->sc_bt, 0, 256, 1);
	return (1);
}

/*
 * Specifics rasops handlers for accelerated console
 */

#define	CG6_BLIT_WAIT(fbc) \
	while (((fbc)->fbc_blit & (FBC_BLIT_UNKNOWN | FBC_BLIT_GXFULL)) == \
	    (FBC_BLIT_UNKNOWN | FBC_BLIT_GXFULL))
#define	CG6_DRAW_WAIT(fbc) \
	while (((fbc)->fbc_draw & (FBC_DRAW_UNKNOWN | FBC_DRAW_GXFULL)) == \
	    (FBC_DRAW_UNKNOWN | FBC_DRAW_GXFULL))
#define	CG6_DRAIN(fbc) \
	while ((fbc)->fbc_s & FBC_S_GXINPROGRESS)

void
cgsix_ras_init(sc)
	struct cgsix_softc *sc;
{
	u_int32_t m;

	CG6_DRAIN(sc->sc_fbc);
	m = sc->sc_fbc->fbc_mode;
	m &= ~FBC_MODE_MASK;
	m |= FBC_MODE_VAL;
	sc->sc_fbc->fbc_mode = m;
}

void
cgsix_ras_copyrows(cookie, src, dst, n)
	void *cookie;
	int src, dst, n;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;

	if (dst == src)
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src + n > ri->ri_rows)
		n = ri->ri_rows - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst + n > ri->ri_rows)
		n = ri->ri_rows - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;

	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = FBC_ALU_COPY;
	fbc->fbc_x0 = ri->ri_xorigin;
	fbc->fbc_y0 = ri->ri_yorigin + src;
	fbc->fbc_x1 = ri->ri_xorigin + ri->ri_emuwidth - 1;
	fbc->fbc_y1 = ri->ri_yorigin + src + n - 1;
	fbc->fbc_x2 = ri->ri_xorigin;
	fbc->fbc_y2 = ri->ri_yorigin + dst;
	fbc->fbc_x3 = ri->ri_xorigin + ri->ri_emuwidth - 1;
	fbc->fbc_y3 = ri->ri_yorigin + dst + n - 1;
	CG6_BLIT_WAIT(fbc);
	CG6_DRAIN(fbc);
}

void
cgsix_ras_copycols(cookie, row, src, dst, n)
	void *cookie;
	int row, src, dst, n;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;

	if (dst == src)
		return;
	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src + n > ri->ri_cols)
		n = ri->ri_cols - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst + n > ri->ri_cols)
		n = ri->ri_cols - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = FBC_ALU_COPY;
	fbc->fbc_x0 = ri->ri_xorigin + src;
	fbc->fbc_y0 = ri->ri_yorigin + row;
	fbc->fbc_x1 = ri->ri_xorigin + src + n - 1;
	fbc->fbc_y1 = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	fbc->fbc_x2 = ri->ri_xorigin + dst;
	fbc->fbc_y2 = ri->ri_yorigin + row;
	fbc->fbc_x3 = ri->ri_xorigin + dst + n - 1;
	fbc->fbc_y3 = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	CG6_BLIT_WAIT(fbc);
	CG6_DRAIN(fbc);
}

void
cgsix_ras_erasecols(cookie, row, col, n, attr)
	void *cookie;
	int row, col, n;
	long int attr;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;

	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (col < 0) {
		n += col;
		col = 0;
	}
	if (col + n > ri->ri_cols)
		n = ri->ri_cols - col;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = FBC_ALU_FILL;
	fbc->fbc_fg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fbc->fbc_arecty = ri->ri_yorigin + row;
	fbc->fbc_arectx = ri->ri_xorigin + col;
	fbc->fbc_arecty = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	fbc->fbc_arectx = ri->ri_xorigin + col + n - 1;
	CG6_DRAW_WAIT(fbc);
	CG6_DRAIN(fbc);
}

void
cgsix_ras_eraserows(cookie, row, n, attr)
	void *cookie;
	int row, n;
	long int attr;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;

	if (row < 0) {
		n += row;
		row = 0;
	}
	if (row + n > ri->ri_rows)
		n = ri->ri_rows - row;
	if (n <= 0)
		return;

	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = FBC_ALU_FILL;
	fbc->fbc_fg = ri->ri_devcmap[(attr >> 16) & 0xf];
	if ((n == ri->ri_rows) && (ri->ri_flg & RI_FULLCLEAR)) {
		fbc->fbc_arecty = 0;
		fbc->fbc_arectx = 0;
		fbc->fbc_arecty = ri->ri_height - 1;
		fbc->fbc_arectx = ri->ri_width - 1;
	} else {
		row *= ri->ri_font->fontheight;
		fbc->fbc_arecty = ri->ri_yorigin + row;
		fbc->fbc_arectx = ri->ri_xorigin;
		fbc->fbc_arecty =
		    ri->ri_yorigin + row + (n * ri->ri_font->fontheight) - 1;
		fbc->fbc_arectx =
		    ri->ri_xorigin + ri->ri_emuwidth - 1;
	}
	CG6_DRAW_WAIT(fbc);
	CG6_DRAIN(fbc);
}

void
cgsix_ras_do_cursor(ri)
	struct rasops_info *ri;
{
	struct cgsix_softc *sc = ri->ri_hw;
	int row, col;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;

	row = ri->ri_crow * ri->ri_font->fontheight;
	col = ri->ri_ccol * ri->ri_font->fontwidth;
	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = FBC_ALU_FLIP;
	fbc->fbc_arecty = ri->ri_yorigin + row;
	fbc->fbc_arectx = ri->ri_xorigin + col;
	fbc->fbc_arecty = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	fbc->fbc_arectx = ri->ri_xorigin + col + ri->ri_font->fontwidth - 1;
	CG6_DRAW_WAIT(fbc);
	CG6_DRAIN(fbc);
}
