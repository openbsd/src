/*	$OpenBSD: cgsix.c,v 1.42 2010/06/07 19:43:45 miod Exp $	*/
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
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/cgsixreg.h>
#include <dev/ic/bt458reg.h>

#if defined(SUN4)
#include <sparc/dev/pfourreg.h>
#endif

/* per-display variables */
struct cgsix_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	rom_reg sc_phys;	/* phys addr of h/w */
	volatile struct bt_regs *sc_bt;		/* Brooktree registers */
	volatile int *sc_fhc;			/* FHC register */
	volatile struct cgsix_thc *sc_thc;	/* THC registers */
	volatile struct cgsix_fbc *sc_fbc;	/* FBC registers */
	volatile struct cgsix_tec_xxx *sc_tec;	/* TEC registers */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	struct	intrhand sc_ih;
};

void	cgsix_burner(void *, u_int, u_int);
int	cgsix_intr(void *);
int	cgsix_ioctl(void *, u_long, caddr_t, int, struct proc *);
static __inline__
void	cgsix_loadcmap_deferred(struct cgsix_softc *, u_int, u_int);
paddr_t	cgsix_mmap(void *, off_t, int);
void	cgsix_reset(struct cgsix_softc *, u_int);
void	cgsix_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);

int	cgsix_ras_copyrows(void *, int, int, int);
int	cgsix_ras_copycols(void *, int, int, int, int);
int	cgsix_ras_do_cursor(struct rasops_info *);
int	cgsix_ras_erasecols(void *, int, int, int, long);
int	cgsix_ras_eraserows(void *, int, int, long);
void	cgsix_ras_init(struct cgsix_softc *);

struct wsdisplay_accessops cgsix_accessops = {
	cgsix_ioctl,
	cgsix_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgsix_burner,
	NULL	/* pollc */
};

int	cgsixmatch(struct device *, void *, void *);
void	cgsixattach(struct device *, struct device *, void *);

struct cfattach cgsix_ca = {
	sizeof(struct cgsix_softc), cgsixmatch, cgsixattach
};

struct cfdriver cgsix_cd = {
	NULL, "cgsix", DV_DULL
};

int
cgsixmatch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, cf->cf_driver->cd_name) &&
	    strcmp(ra->ra_name, "SUNW,cgsix"))
		return (0);

	switch (ca->ca_bustype) {
	case BUS_SBUS:
		return (1);
	case BUS_OBIO:
#if defined(SUN4)
		if (CPU_ISSUN4) {
			void *tmp;

			/*
			 * Check for a pfour framebuffer.  This is done
			 * somewhat differently on the cgsix than other
			 * pfour framebuffers.
			 */
			bus_untmp();
			tmp = (caddr_t)mapdev(ra->ra_reg, TMPMAP_VA,
			    CGSIX_FHC_OFFSET, NBPG);
			if (probeget(tmp, 4) == -1)
				return (0);

			if (fb_pfour_id(tmp) == PFOUR_ID_FASTCOLOR)
				return (1);
		}
#endif
		/* FALLTHROUGH */
	default:
		return (0);
	}
}

void
cgsixattach(struct device *parent, struct device *self, void *args)
{
	struct cgsix_softc *sc = (struct cgsix_softc *)self;
	struct confargs *ca = args;
	int node, pri;
	int isconsole = 0, sbus = 1;
	char *nam;
	u_int fhcrev;

	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d: ", pri);

	/*
	 * Map just BT, FHC, FBC, THC, and video RAM.
	 */
	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bt = (volatile struct bt_regs *)
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
		SET(sc->sc_sunfb.sf_flags, FB_PFOUR);
		nam = "p4";
		break;
	case BUS_SBUS:
		node = ca->ca_ra.ra_node;
		nam = getpropstring(node, "model");
		break;
	}

	if (*nam != '\0')
		printf("%s, ", nam);

#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		int constype = ISSET(sc->sc_sunfb.sf_flags, FB_PFOUR) ?
		    EED_CONS_P4 : EED_CONS_COLOR;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->ee_diag.eed_console == constype)
			isconsole = 1;
	}
#endif

	if (CPU_ISSUN4COR4M)
		isconsole = node == fbnode;

	fhcrev = (*sc->sc_fhc >> FHC_REV_SHIFT) &
	    (FHC_REV_MASK >> FHC_REV_SHIFT);

	sc->sc_ih.ih_fun = cgsix_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, IPL_FB, self->dv_xname);

	/* reset cursor & frame buffer controls */
	cgsix_reset(sc, fhcrev);

	/* enable video */
	cgsix_burner(sc, 1, 0);

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);
        sc->sc_sunfb.sf_ro.ri_bits = mapiodev(ca->ca_ra.ra_reg,
	    CGSIX_VID_OFFSET, round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	printf("%dx%d, rev %d\n", sc->sc_sunfb.sf_width,
	    sc->sc_sunfb.sf_height, fhcrev);

	fbwscons_init(&sc->sc_sunfb, isconsole);
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

	if (isconsole)
		fbwscons_console_init(&sc->sc_sunfb, -1);

	fbwscons_attach(&sc->sc_sunfb, &cgsix_accessops, isconsole);
}

int
cgsix_ioctl(void *dev, u_long cmd, caddr_t data, int flags, struct proc *p)
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
		break;

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
cgsix_reset(struct cgsix_softc *sc, u_int fhcrev)
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
	bt->bt_addr = BT_CR << 24;
	bt->bt_ctrl |= (BTCR_DISPENA_OV1 | BTCR_DISPENA_OV0) << 24;
}

paddr_t
cgsix_mmap(void *v, off_t offset, int prot)
{
	struct cgsix_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, offset + CGSIX_VID_OFFSET) |
		    PMAP_NC);
	}

	return (-1);
}

void
cgsix_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
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
cgsix_burner(void *v, u_int on, u_int flags)
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
cgsix_intr(void *v)
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
cgsix_ras_init(struct cgsix_softc *sc)
{
	u_int32_t m;

	CG6_DRAIN(sc->sc_fbc);
	m = sc->sc_fbc->fbc_mode;
	m &= ~FBC_MODE_MASK;
	m |= FBC_MODE_VAL;
	sc->sc_fbc->fbc_mode = m;
}

int
cgsix_ras_copyrows(void *cookie, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;

	if (dst == src)
		return 0;
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
		return 0;
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

	return 0;
}

int
cgsix_ras_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;

	if (dst == src)
		return 0;
	if ((row < 0) || (row >= ri->ri_rows))
		return 0;
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
		return 0;
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

	return 0;
}

int
cgsix_ras_erasecols(void *cookie, int row, int col, int n, long attr)
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;
	int fg, bg;

	if ((row < 0) || (row >= ri->ri_rows))
		return 0;
	if (col < 0) {
		n += col;
		col = 0;
	}
	if (col + n > ri->ri_cols)
		n = ri->ri_cols - col;
	if (n <= 0)
		return 0;
	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = FBC_ALU_FILL;
	fbc->fbc_fg = ri->ri_devcmap[bg];
	fbc->fbc_arecty = ri->ri_yorigin + row;
	fbc->fbc_arectx = ri->ri_xorigin + col;
	fbc->fbc_arecty = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	fbc->fbc_arectx = ri->ri_xorigin + col + n - 1;
	CG6_DRAW_WAIT(fbc);
	CG6_DRAIN(fbc);

	return 0;
}

int
cgsix_ras_eraserows(void *cookie, int row, int n, long attr)
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;
	volatile struct cgsix_fbc *fbc = sc->sc_fbc;
	int fg, bg;

	if (row < 0) {
		n += row;
		row = 0;
	}
	if (row + n > ri->ri_rows)
		n = ri->ri_rows - row;
	if (n <= 0)
		return 0;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = FBC_ALU_FILL;
	fbc->fbc_fg = ri->ri_devcmap[bg];
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

	return 0;
}

int
cgsix_ras_do_cursor(struct rasops_info *ri)
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

	return 0;
}
