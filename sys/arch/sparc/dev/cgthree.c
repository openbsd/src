/*	$OpenBSD: cgthree.c,v 1.27 2005/01/05 23:04:24 miod Exp $	*/
/*	$NetBSD: cgthree.c,v 1.33 1997/05/24 20:16:11 pk Exp $ */

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
 * Copyright (c) 1992, 1993
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
 *	@(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * Color display (cgthree) driver.
 * Works with the real Sun hardware, as well as various clones from Tatung,
 * Integrix (S20), and the Vigra VS10-EK.
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
#include <sparc/dev/cgthreereg.h>
#include <sparc/dev/sbusvar.h>

/* per-display variables */
struct cgthree_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	sbusdev sc_sd;		/* sbus device */
	struct rom_reg	sc_phys;	/* phys address description */
	volatile struct fbcontrol *sc_fbc;	/* Brooktree registers */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	struct intrhand sc_ih;
	int	sc_nscreens;
};

int cgthree_ioctl(void *, u_long, caddr_t, int, struct proc *);
int cgthree_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void cgthree_free_screen(void *, void *);
int cgthree_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t cgthree_mmap(void *, off_t, int);
void cgthree_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
static __inline__ void cgthree_loadcmap_deferred(struct cgthree_softc *,
    u_int, u_int);
void cgthree_burner(void *, u_int, u_int);
int cgthree_intr(void *);

struct wsdisplay_accessops cgthree_accessops = {
	cgthree_ioctl,
	cgthree_mmap,
	cgthree_alloc_screen,
	cgthree_free_screen,
	cgthree_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgthree_burner,
};

int	cgthreematch(struct device *, void *, void *);
void	cgthreeattach(struct device *, struct device *, void *);

struct cfattach cgthree_ca = {
	sizeof (struct cgthree_softc), cgthreematch, cgthreeattach
};

struct cfdriver cgthree_cd = {
	NULL, "cgthree", DV_DULL
};

/* Video control parameters */
struct cg3_videoctrl {
	u_int8_t	sense;
	u_int8_t	vctrl[12];
} cg3_videoctrl[] = {
	{	/* cpd-1790 */
		0x31,
		{ 0xbb, 0x2b, 0x04, 0x14, 0xae, 0x03,
		  0xa8, 0x24, 0x01, 0x05, 0xff, 0x01 },
	},
	{	/* gdm-20e20 */
		0x41,
		{ 0xb7, 0x27, 0x03, 0x0f, 0xae, 0x03,
		  0xae, 0x2a, 0x01, 0x09, 0xff, 0x01 },
	},
	{	/* defaults, should be last */
		0xff,
		{ 0xbb, 0x2b, 0x03, 0x0b, 0xb3, 0x03,
		  0xaf, 0x2b, 0x02, 0x0a, 0xff, 0x01 },
	},
};

/*
 * Match a cgthree.
 */
int
cgthreematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name) &&
	    strcmp("cgRDI", ra->ra_name))
		return (0);

	if (ca->ca_bustype == BUS_SBUS)
		return (1);

	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 4) != -1);
}

/*
 * Attach a display.
 */
void
cgthreeattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct cgthree_softc *sc = (struct cgthree_softc *)self;
	struct confargs *ca = args;
	int node = 0, isrdi = 0, i;
	volatile struct bt_regs *bt;
	int isconsole = 0, sbus = 1;
	char *nam = NULL;

	switch (ca->ca_bustype) {
	case BUS_OBIO:
		if (CPU_ISSUN4M) {	/* 4m has framebuffer on obio */
			sbus = 0;
			node = ca->ca_ra.ra_node;
			nam = getpropstring(node, "model");
			if (*nam == '\0')
				nam = "cgthree";
			break;
		}
	case BUS_VME32:
	case BUS_VME16:
		sbus = node = 0;
		nam = "cgthree";
		break;

	case BUS_SBUS:
		node = ca->ca_ra.ra_node;
		nam = getpropstring(node, "model");
		if (*nam == '\0')
			nam = "cgthree";
		break;
	}

	if (!strcmp(ca->ca_ra.ra_name, "cgRDI")) {
		isrdi = 1;
		nam = "cgRDI";
	}

	printf(": %s", nam);

	isconsole = node == fbnode;

	sc->sc_fbc = (volatile struct fbcontrol *)
	    mapiodev(ca->ca_ra.ra_reg, CG3REG_REG,
		     sizeof(struct fbcontrol));

	/* Transfer video magic to board, if it's not running */
	if (isrdi == 0 && (sc->sc_fbc->fbc_ctrl & FBC_TIMING) == 0)
		for (i = 0; i < sizeof(cg3_videoctrl)/sizeof(cg3_videoctrl[0]);
		     i++) {
			volatile struct fbcontrol *fbc = sc->sc_fbc;
			if (cg3_videoctrl[i].sense == 0xff ||
			    (fbc->fbc_status & FBS_MSENSE) ==
			     cg3_videoctrl[i].sense) {
				int j;
#ifdef DEBUG
				printf(" (setting video ctrl)");
#endif
				for (j = 0; j < 12; j++)
					fbc->fbc_vcontrol[j] =
						cg3_videoctrl[i].vctrl[j];
				fbc->fbc_ctrl |= FBC_TIMING;
				break;
			}
		}

	sc->sc_phys = ca->ca_ra.ra_reg[0];

	sc->sc_ih.ih_fun = cgthree_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih, IPL_FB,
	    self->dv_xname);

	/* grab initial (current) color map */
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3 / 4; i++)
		sc->sc_cmap.cm_chip[i] = bt->bt_cmap;

	/* enable video */
	cgthree_burner(sc, 1, 0);
	BT_INIT(bt, 0);

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);
	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(ca->ca_ra.ra_reg, CG3REG_MEM,
	    round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	printf(", %dx%d\n", sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	/*
	 * If the framebuffer width is under 1024x768, which is the case for
	 * some clones on laptops, as well as with the VS10-EK, switch from
	 * the PROM font to the more adequate 8x16 font here.
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
	fbwscons_setcolormap(&sc->sc_sunfb, cgthree_setcolor);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb,
		    sc->sc_sunfb.sf_width >= 1024 ? -1 : 0, cgthree_burner);
	}

#if defined(SUN4C) || defined(SUN4M)
	if (sbus)
		sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);
#endif

	fbwscons_attach(&sc->sc_sunfb, &cgthree_accessops, isconsole);
}

int
cgthree_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgthree_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNCG3;
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
		cgthree_loadcmap_deferred(sc, cm->index, cm->count);
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

int
cgthree_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct cgthree_softc *sc = v;

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
cgthree_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct cgthree_softc *sc = v;

	sc->sc_nscreens--;
}

int
cgthree_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
cgthree_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct cgthree_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys,
		    CG3REG_MEM + offset) | PMAP_NC);
	}

	return (-1);
}

void
cgthree_setcolor(v, index, r, g, b)
	void *v;
	u_int index;
	u_int8_t r, g, b;
{
	struct cgthree_softc *sc = v;

	bt_setcolor(&sc->sc_cmap, &sc->sc_fbc->fbc_dac, index, r, g, b, 0);
}

static __inline__ void
cgthree_loadcmap_deferred(struct cgthree_softc *sc, u_int start, u_int ncolors)
{

	sc->sc_fbc->fbc_ctrl |= FBC_IENAB;
}

void
cgthree_burner(v, on, flags)
	void *v;
	u_int on, flags;
{
	struct cgthree_softc *sc = v;
	int s;

	s = splhigh();
	if (on)
		sc->sc_fbc->fbc_ctrl |= FBC_VENAB | FBC_TIMING;
	else {
		sc->sc_fbc->fbc_ctrl &= ~FBC_VENAB;
		if (flags & WSDISPLAY_BURN_VBLANK)
			sc->sc_fbc->fbc_ctrl &= ~FBC_TIMING;
	}
	splx(s);
}

int
cgthree_intr(v)
	void *v;
{
	struct cgthree_softc *sc = v;

	if (!ISSET(sc->sc_fbc->fbc_ctrl, FBC_IENAB) ||
	    !ISSET(sc->sc_fbc->fbc_status, FBS_INTR)) {
		/* Not expecting an interrupt, it's not for us. */
		return (0);
	}

	/* Acknowledge the interrupt and disable it. */
	sc->sc_fbc->fbc_ctrl &= ~FBC_IENAB;

	bt_loadcmap(&sc->sc_cmap, &sc->sc_fbc->fbc_dac, 0, 256, 0);
	return (1);
}
