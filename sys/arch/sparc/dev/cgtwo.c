/*	$OpenBSD: cgtwo.c,v 1.38 2010/06/07 19:43:45 miod Exp $	*/
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
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>


/* per-display variables */
struct cgtwo_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	rom_reg	sc_phys;	/* display RAM (phys addr) */
	volatile struct cg2statusreg *sc_reg;	/* CG2 control registers */
	volatile u_short *sc_ppmask;
	volatile u_short *sc_cmap;
#define sc_redmap(cmap)		((cmap))
#define sc_greenmap(cmap)	((cmap) + CG2_CMSIZE)
#define sc_bluemap(cmap)	((cmap) + 2 * CG2_CMSIZE)
};

void	cgtwo_burner(void *, u_int, u_int);
int	cgtwo_getcmap(struct cgtwo_softc *, struct wsdisplay_cmap *);
int	cgtwo_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	cgtwo_mmap(void *, off_t, int);
int	cgtwo_putcmap(struct cgtwo_softc *, struct wsdisplay_cmap *);
void	cgtwo_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);

struct wsdisplay_accessops cgtwo_accessops = {
	cgtwo_ioctl,
	cgtwo_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgtwo_burner,
	NULL	/* pollc */
};

int	cgtwomatch(struct device *, void *, void *);
void	cgtwoattach(struct device *, struct device *, void *);

struct cfattach cgtwo_ca = {
	sizeof(struct cgtwo_softc), cgtwomatch, cgtwoattach
};

struct cfdriver cgtwo_cd = {
	NULL, "cgtwo", DV_DULL
};

int
cgtwomatch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	caddr_t tmp;

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

void
cgtwoattach(struct device *parent, struct device *self, void *args)
{
	struct cgtwo_softc *sc = (struct cgtwo_softc *)self;
	struct confargs *ca = args;
	int node = 0;
	int isconsole = 0;

	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->ee_diag.eed_console == EED_CONS_COLOR)
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

	sc->sc_ppmask = (volatile u_short *)
	    mapiodev(ca->ca_ra.ra_reg,
		CG2_ROPMEM_OFF + offsetof(struct cg2fb, ppmask.reg),
		sizeof(u_short));

	sc->sc_cmap = (volatile u_short *)
	    mapiodev(ca->ca_ra.ra_reg,
		     CG2_ROPMEM_OFF + offsetof(struct cg2fb, redmap),
		     3 * CG2_CMSIZE * sizeof(u_short));

	/* enable video */
	*sc->sc_ppmask = 0xffff;	/* enable all color planes... */
	cgtwo_burner(sc, 1, 0);		/* ... and video signals */

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);
	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(&sc->sc_phys, CG2_PIXMAP_OFF,
	    round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	printf(": %dx%d\n", sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	fbwscons_init(&sc->sc_sunfb, isconsole);
	fbwscons_setcolormap(&sc->sc_sunfb, cgtwo_setcolor);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, -1);
	}

	fbwscons_attach(&sc->sc_sunfb, &cgtwo_accessops, isconsole);
}

int
cgtwo_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct cgtwo_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNCG2;
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
		error = cgtwo_getcmap(sc, cm);
		if (error)
			return (error);
		break;

	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cgtwo_putcmap(sc, cm);
		if (error)
			return (error);
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

paddr_t
cgtwo_mmap(void *v, off_t offset, int prot)
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
cgtwo_burner(void *v, u_int on, u_int flags)
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
cgtwo_getcmap(struct cgtwo_softc *sc, struct wsdisplay_cmap *cmap)
{
	u_int index = cmap->index, count = cmap->count, i;
	u_char red[CG2_CMSIZE], green[CG2_CMSIZE], blue[CG2_CMSIZE];
	volatile u_short *hwcmap = sc->sc_cmap;
	int error;
	volatile u_short *p;


	if (index >= CG2_CMSIZE || count >= CG2_CMSIZE - index)
		return (EINVAL);

	while (sc->sc_reg->retrace == 0)
		DELAY(1);

	sc->sc_reg->update_cmap = 0;

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

	sc->sc_reg->update_cmap = 1;

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
cgtwo_putcmap(struct cgtwo_softc *sc, struct wsdisplay_cmap *cmap)
{
	u_int index = cmap->index, count = cmap->count, i;
	u_char red[CG2_CMSIZE], green[CG2_CMSIZE], blue[CG2_CMSIZE];
	volatile u_short *hwcmap = sc->sc_cmap;
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

	while (sc->sc_reg->retrace == 0)
		DELAY(1);

	sc->sc_reg->update_cmap = 0;

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

	sc->sc_reg->update_cmap = 1;

	return (0);
}

void
cgtwo_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct cgtwo_softc *sc = v;

	while (sc->sc_reg->retrace == 0)
		DELAY(1);

	sc->sc_reg->update_cmap = 0;

	sc_redmap(sc->sc_cmap)[index] = r;
	sc_greenmap(sc->sc_cmap)[index] = g;
	sc_bluemap(sc->sc_cmap)[index] = b;

	sc->sc_reg->update_cmap = 1;
}
