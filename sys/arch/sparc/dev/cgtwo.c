/*	$OpenBSD: cgtwo.c,v 1.22 2002/08/12 10:44:03 miod Exp $	*/
/*	$NetBSD: cgtwo.c,v 1.22 1997/05/24 20:16:12 pk Exp $ */

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
 *	from: @(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgtwo) driver.
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

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#if defined(SUN4)
#include <machine/eeprom.h>
#endif
#include <machine/conf.h>

#include <sparc/dev/cgtworeg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>


/* per-display variables */
struct cgtwo_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	rom_reg	sc_phys;	/* display RAM (phys addr) */
	volatile struct cg2statusreg *sc_reg;	/* CG2 control registers */
	volatile u_short *sc_cmap;
#define sc_redmap(cmap)		((u_short *)(cmap))
#define sc_greenmap(cmap)	((u_short *)(cmap) + CG2_CMSIZE)
#define sc_bluemap(cmap)	((u_short *)(cmap) + 2 * CG2_CMSIZE)
	int	sc_nscreens;
};

struct wsscreen_descr cgtwo_stdscreen = {
	"std",
	0, 0,	/* will be filled in */
	0,
	0, 0,
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *cgtwo_scrlist[] = {
	&cgtwo_stdscreen,
};

struct wsscreen_list cgtwo_screenlist = {
	sizeof(cgtwo_scrlist) / sizeof(struct wsscreen_descr *),
	    cgtwo_scrlist
};

int cgtwo_ioctl(void *, u_long, caddr_t, int, struct proc *);
int cgtwo_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void cgtwo_free_screen(void *, void *);
int cgtwo_show_screen(void *, void *, int, void (*)(void *, int, int), void *);
paddr_t cgtwo_mmap(void *, off_t, int);
int cgtwo_putcmap(volatile u_short *, struct wsdisplay_cmap *);
int cgtwo_getcmap(volatile u_short *, struct wsdisplay_cmap *);
void cgtwo_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
void cgtwo_burner(void *, u_int, u_int);

struct wsdisplay_accessops cgtwo_accessops = {
	cgtwo_ioctl,
	cgtwo_mmap,
	cgtwo_alloc_screen,
	cgtwo_free_screen,
	cgtwo_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgtwo_burner,
};

int cgtwomatch(struct device *, void *, void *);
void cgtwoattach(struct device *, struct device *, void *);

struct cfattach cgtwo_ca = {
	sizeof(struct cgtwo_softc), cgtwomatch, cgtwoattach
};

struct cfdriver cgtwo_cd = {
	NULL, "cgtwo", DV_DULL
};

/*
 * Match a cgtwo.
 */
int
cgtwomatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	caddr_t tmp;

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	if (!CPU_ISSUN4 || ca->ca_bustype != BUS_VME16)
		return (0);

	/* XXX - Must do our own mapping at CG2_CTLREG_OFF */
	bus_untmp();
	tmp = (caddr_t)mapdev(ra->ra_reg, TMPMAP_VA, CG2_CTLREG_OFF, NBPG);
	if (probeget(tmp, 2) != -1)
		return (1);

	return (0);
}

/*
 * Attach a display.
 */
void
cgtwoattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct cgtwo_softc *sc = (struct cgtwo_softc *)self;
	struct confargs *ca = args;
	struct wsemuldisplaydev_attach_args waa;
	int node = 0;
	int isconsole = 0;
	char *nam = NULL;

	sc->sc_sunfb.sf_flags = self->dv_cfdata->cf_flags;

	switch (ca->ca_bustype) {
	case BUS_VME16:
		node = 0;
		nam = "cgtwo";
		break;

	default:
		panic("cgtwoattach: impossible bustype");
		/* NOTREACHED */
	}

	printf(": %s", nam);

	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->eeConsole == EE_CONS_COLOR)
			isconsole = 1;
	}

	/*
	 * When the ROM has mapped in a cgtwo display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.
	 */
	sc->sc_phys = ca->ca_ra.ra_reg[0];
	/* Apparently, the pixels are 32-bit data space */
	sc->sc_phys.rr_iospace = PMAP_VME32;

	sc->sc_reg = (volatile struct cg2statusreg *)
	    mapiodev(ca->ca_ra.ra_reg,
		     CG2_ROPMEM_OFF + offsetof(struct cg2fb, status.reg),
		     sizeof(struct cg2statusreg));

	sc->sc_cmap = (volatile u_short *)
	    mapiodev(ca->ca_ra.ra_reg,
		     CG2_ROPMEM_OFF + offsetof(struct cg2fb, redmap[0]),
		     3 * CG2_CMSIZE);

	/* enable video */
	cgtwo_burner(sc, 1, 0);

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);
	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(&sc->sc_phys, CG2_PIXMAP_OFF,
	    round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, isconsole);

	cgtwo_stdscreen.nrows = sc->sc_sunfb.sf_ro.ri_rows;
	cgtwo_stdscreen.ncols = sc->sc_sunfb.sf_ro.ri_cols;
	cgtwo_stdscreen.textops = &sc->sc_sunfb.sf_ro.ri_ops;

	printf(", %dx%d\n", sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, &cgtwo_stdscreen, -1,
		    cgtwo_setcolor, cgtwo_burner);
	}

	waa.console = isconsole;
	waa.scrdata = &cgtwo_screenlist;
	waa.accessops = &cgtwo_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);
}

int
cgtwo_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgtwo_softc *sc = v;
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
		wdf->cmsize = CG2_CMSIZE;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;
		
	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cgtwo_getcmap(sc->sc_cmap, cm);
		if (error)
			return (error);
		break;

	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cgtwo_putcmap(sc->sc_cmap, cm);
		if (error)
			return (error);
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
cgtwo_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct cgtwo_softc *sc = v;

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
cgtwo_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct cgtwo_softc *sc = v;

	sc->sc_nscreens--;
}

int
cgtwo_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

paddr_t
cgtwo_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct cgtwo_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys,
		    CG2_PIXMAP_OFF + offset) | PMAP_NC);
	}

	return (-1);
}

void
cgtwo_burner(v, on, flags)
	void *v;
	u_int on, flags;
{
	struct cgtwo_softc *sc = v;
	int s;

	s = splhigh();
	if (on)
		sc->sc_reg->video_enab = 1;
	else
		sc->sc_reg->video_enab = 0;
	splx(s);
}

int
cgtwo_getcmap(hwcmap, cmap)
	volatile u_short *hwcmap;
	struct wsdisplay_cmap *cmap;
{
	u_int index = cmap->index, count = cmap->count, i;
	u_char red[CG2_CMSIZE], green[CG2_CMSIZE], blue[CG2_CMSIZE];
	int error;
	volatile u_short *p;


	if (index >= CG2_CMSIZE || count >= CG2_CMSIZE - index)
		return (EINVAL);

	/* XXX - Wait for retrace? */

	/* Copy hardware to local arrays. */
	p = &sc_redmap(hwcmap)[index];
	for (i = 0; i < count; i++)
		red[i] = *p++;
	p = &sc_greenmap(hwcmap)[index];
	for (i = 0; i < count; i++)
		green[i] = *p++;
	p = &sc_bluemap(hwcmap)[index];
	for (i = 0; i < count; i++)
		blue[i] = *p++;

	/* Copy local arrays to user space. */
	if ((error = copyout(red, cmap->red, count)) != 0)
		return (error);
	if ((error = copyout(green, cmap->green, count)) != 0)
		return (error);
	if ((error = copyout(blue, cmap->blue, count)) != 0)
		return (error);

	return (0);
}

int
cgtwo_putcmap(hwcmap, cmap)
	volatile u_short *hwcmap;
	struct wsdisplay_cmap *cmap;
{
	u_int index = cmap->index, count = cmap->count, i;
	u_char red[CG2_CMSIZE], green[CG2_CMSIZE], blue[CG2_CMSIZE];
	int error;
	volatile u_short *p;

	if (index >= CG2_CMSIZE || count >= CG2_CMSIZE - index)
		return (EINVAL);

	/* Copy from user space to local arrays. */
	if ((error = copyin(cmap->red, red, count)) != 0)
		return (error);
	if ((error = copyin(cmap->green, green, count)) != 0)
		return (error);
	if ((error = copyin(cmap->blue, blue, count)) != 0)
		return (error);

	/* XXX - Wait for retrace? */

	/* Copy from local arrays to hardware. */
	p = &sc_redmap(hwcmap)[index];
	for (i = 0; i < count; i++)
		*p++ = red[i];
	p = &sc_greenmap(hwcmap)[index];
	for (i = 0; i < count; i++)
		*p++ = green[i];
	p = &sc_bluemap(hwcmap)[index];
	for (i = 0; i < count; i++)
		*p++ = blue[i];

	return (0);
}

void
cgtwo_setcolor(v, index, r, g, b)
	void *v;
	u_int index;
	u_int8_t r, g, b;
{
	struct cgtwo_softc *sc = v;
#if 0
	struct wsdisplay_cmap cm;

	cm.red = &r;
	cm.green = &g;
	cm.blue = &b;
	cm.index = index;
	cm.count = 1;

	cgtwo_putcmap(sc->sc_cmap, &cm);
#else

	/* XXX - Wait for retrace? */

	sc_redmap(sc->sc_cmap)[index] = r;
	sc_greenmap(sc->sc_cmap)[index] = g;
	sc_bluemap(sc->sc_cmap)[index] = b;
#endif
}
