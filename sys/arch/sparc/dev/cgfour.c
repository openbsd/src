/*	$OpenBSD: cgfour.c,v 1.14 2002/08/12 10:44:03 miod Exp $	*/
/*	$NetBSD: cgfour.c,v 1.13 1997/05/24 20:16:06 pk Exp $	*/

/*
 * Coyrpight (c) 2002 Miodrag Vallat.  All rights reserved.
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1995 Theo de Raadt.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Theo de Raadt.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from @(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgfour) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
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

#include <uvm/uvm_extern.h>		/* NBPG */

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/eeprom.h>
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/pfourreg.h>

/* per-display variables */
struct cgfour_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct rom_reg	sc_phys;	/* display RAM (phys addr) */
	volatile struct fbcontrol *sc_fbc;	/* Brooktree registers */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	int	sc_nscreens;
};

struct wsscreen_descr cgfour_stdscreen = {
	"std",
	0, 0,	/* will be filled in */
	0,
	0, 0,
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *cgfour_scrlist[] = {
	&cgfour_stdscreen,
};

struct wsscreen_list cgfour_screenlist = {
	sizeof(cgfour_scrlist) / sizeof(struct wsscreen_descr *),
	    cgfour_scrlist
};

int cgfour_ioctl(void *, u_long, caddr_t, int, struct proc *);
int cgfour_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void cgfour_free_screen(void *, void *);
int cgfour_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t cgfour_mmap(void *, off_t, int);
void cgfour_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
void cgfour_burner(void *, u_int, u_int);

struct wsdisplay_accessops cgfour_accessops = {
	cgfour_ioctl,
	cgfour_mmap,
	cgfour_alloc_screen,
	cgfour_free_screen,
	cgfour_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgfour_burner,
};

void	cgfourattach(struct device *, struct device *, void *);
int	cgfourmatch(struct device *, void *, void *);

struct cfattach cgfour_ca = {
	sizeof(struct cgfour_softc), cgfourmatch, cgfourattach
};

struct cfdriver cgfour_cd = {
	NULL, "cgfour", DV_DULL
};

/*
 * Match a cgfour.
 */
int
cgfourmatch(parent, vcf, aux)
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

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	if (!CPU_ISSUN4 || ca->ca_bustype != BUS_OBIO)
		return (0);

	if (probeget(ra->ra_vaddr, 4) == -1)
		return (0);

	/*
	 * Check the pfour register.
	 */
	if (fb_pfour_id(ra->ra_vaddr) == PFOUR_ID_COLOR8P1) {
		cf->cf_flags |= FB_PFOUR;
		return (1);
	}

	return (0);
}

/*
 * Attach a display.
 */
void
cgfourattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct cgfour_softc *sc = (struct cgfour_softc *)self;
	struct confargs *ca = args;
	struct wsemuldisplaydev_attach_args waa;
	int node = 0, i;
	volatile struct bt_regs *bt;
	int isconsole = 0;

	sc->sc_sunfb.sf_flags = self->dv_cfdata->cf_flags;

	printf(": cgfour/p4");

	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;

		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->eeConsole == EE_CONS_P4OPT)
			isconsole = 1;
	}

	/* Map the pfour register. */
	sc->sc_sunfb.sf_pfour = (volatile u_int32_t *)
		mapiodev(ca->ca_ra.ra_reg, 0, sizeof(u_int32_t));

	/*
	 * When the ROM has mapped in a cgfour display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */

	sc->sc_phys = ca->ca_ra.ra_reg[0];

	/* grab initial (current) color map */
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3 / 4; i++)
		((char *)&sc->sc_cmap)[i] = bt->bt_cmap >> 24;

	/* enable video */
	cgfour_burner(sc, 1, 0);
	BT_INIT(bt, 24);

	/*
	 * XXX should initialize the enable plane, instead of expecting the
	 * PROM to do so for us
	 */

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);
	/*
	 * XXX this only maps the color plane, not the overlay or the enable
	 * planes
	 */
	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(ca->ca_ra.ra_reg,
	    PFOUR_COLOR_OFF_COLOR, round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, isconsole);

	cgfour_stdscreen.nrows = sc->sc_sunfb.sf_ro.ri_rows;
	cgfour_stdscreen.ncols = sc->sc_sunfb.sf_ro.ri_cols;
	cgfour_stdscreen.textops = &sc->sc_sunfb.sf_ro.ri_ops;

	printf(", %dx%d\n", sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, &cgfour_stdscreen, -1,
		    cgfour_setcolor, cgfour_burner);
	}

	waa.console = isconsole;
	waa.scrdata = &cgfour_screenlist;
	waa.accessops = &cgfour_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);
}

int
cgfour_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgfour_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
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
		bt_loadcmap(&sc->sc_cmap, &sc->sc_fbc->fbc_dac,
		    cm->index, cm->count, 1);
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
cgfour_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct cgfour_softc *sc = v;

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
cgfour_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct cgfour_softc *sc = v;

	sc->sc_nscreens--;
}

int
cgfour_show_screen(v, cookie, waitok, cb, cbarg)
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
cgfour_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct cgfour_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys,
		    PFOUR_COLOR_OFF_COLOR + offset) | PMAP_NC);
	}

	return (-1);
}

void
cgfour_setcolor(v, index, r, g, b)
	void *v;
	u_int index;
	u_int8_t r, g, b;
{
	struct cgfour_softc *sc = v;

	bt_setcolor(&sc->sc_cmap, &sc->sc_fbc->fbc_dac, index, r, g, b, 1);
}

void
cgfour_burner(v, on, flags)
	void *v;
	u_int on, flags;
{
	struct cgfour_softc *sc = v;

	fb_pfour_set_video(&sc->sc_sunfb, on);
}
