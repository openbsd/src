/*	$OpenBSD: mgx.c,v 1.6 2004/11/29 22:07:37 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 */

/*
 * Driver for the Southlan Media Systems (now Quantum 3D) MGX and MGXPlus
 * frame buffers.
 *
 * Pretty crude, due to the lack of documentation. Works as a dumb frame
 * buffer in 8 bit mode, although the hardware can run in an 32 bit
 * accelerated mode. Also, interrupts are not handled.
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

#include <sparc/dev/sbusvar.h>

/*
 * MGX PROM register layout
 */

#define	MGX_NREG	9
#define	MGX_REG_CRTC	4	/* video control and ramdac */
#define	MGX_REG_CTRL	5	/* control engine */
#define	MGX_REG_VRAM8	8	/* 8-bit memory space */

/*
 * MGX CRTC empirical constants
 */
#define	ADDRESS_REVERSE(x)	((x) ^ 0x03)
#define	CRTC_COMMAND	ADDRESS_REVERSE(0x03c4)
#define	CRTC_DATA	ADDRESS_REVERSE(0x03c5)
#define	CRTC_CMAP	ADDRESS_REVERSE(0x03c9)
#define	CD_DISABLEVIDEO	0x0020

/* per-display variables */
struct mgx_softc {
	struct	sunfb	sc_sunfb;	/* common base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	rom_reg sc_phys;
	u_int8_t	sc_cmap[256 * 3];	/* shadow colormap */
	volatile u_int8_t *sc_vidc;	/* ramdac registers */

	int	sc_nscreens;
};

int mgx_ioctl(void *, u_long, caddr_t, int, struct proc *);
int mgx_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void mgx_free_screen(void *, void *);
int mgx_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t mgx_mmap(void *, off_t, int);
void mgx_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
int mgx_getcmap(u_int8_t *, struct wsdisplay_cmap *);
int mgx_putcmap(u_int8_t *, struct wsdisplay_cmap *);
void mgx_loadcmap(struct mgx_softc *, int, int);
void mgx_burner(void *, u_int ,u_int);

struct wsdisplay_accessops mgx_accessops = {
	mgx_ioctl,
	mgx_mmap,
	mgx_alloc_screen,
	mgx_free_screen,
	mgx_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	mgx_burner
};

int mgxmatch(struct device *, void *, void *);
void mgxattach(struct device *, struct device *, void *);

struct cfattach mgx_ca = {
	sizeof(struct mgx_softc), mgxmatch, mgxattach
};

struct cfdriver mgx_cd = {
	NULL, "mgx", DV_DULL
};

/*
 * Match an MGX or MGX+ card.
 */
int
mgxmatch(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, "SMSI,mgx") != 0 &&
	    strcmp(ra->ra_name, "mgx") != 0)
		return (0);

	return (1);
}

/*
 * Attach an MGX frame buffer.
 * This will keep the frame buffer in the actual PROM mode, and attach
 * a wsdisplay child device to itself.
 */
void
mgxattach(struct device *parent, struct device *self, void *args)
{
	struct mgx_softc *sc = (struct mgx_softc *)self;
	struct confargs *ca = args;
	int node, fbsize;
	int isconsole;

	node = ca->ca_ra.ra_node;

	printf(": %s", getpropstring(node, "model"));

	isconsole = node == fbnode;

	/* Check registers */
	if (ca->ca_ra.ra_nreg < MGX_NREG) {
		printf("\n%s: expected %d registers, got %d\n",
		    self->dv_xname, MGX_NREG, ca->ca_ra.ra_nreg);
		return;
	}

	sc->sc_vidc = (volatile u_int8_t *)mapiodev(
	    &ca->ca_ra.ra_reg[MGX_REG_CRTC], 0, PAGE_SIZE);

	/* enable video */
	mgx_burner(sc, 1, 0);

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, ca->ca_bustype);

	/* Sanity check frame buffer memory */
	fbsize = getpropint(node, "fb_size", 0);
	if (fbsize != 0 && sc->sc_sunfb.sf_fbsize > fbsize) {
		printf("\n%s: expected at least %d bytes of vram, but card "
		    "only provides %d\n",
		    self->dv_xname, sc->sc_sunfb.sf_fbsize, fbsize);
		return;
	}

	/* Map the frame buffer memory area we're interested in */
	sc->sc_phys = ca->ca_ra.ra_reg[MGX_REG_VRAM8];
	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(&sc->sc_phys,
	    0, round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	fbwscons_init(&sc->sc_sunfb, isconsole ? 0 : RI_CLEAR);

	bzero(sc->sc_cmap, sizeof(sc->sc_cmap));
	fbwscons_setcolormap(&sc->sc_sunfb, mgx_setcolor);

	printf(", %dx%d\n",
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, -1, mgx_burner);
	}

	sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);

	fbwscons_attach(&sc->sc_sunfb, &mgx_accessops, isconsole);
}

/*
 * wsdisplay operations
 */

int
mgx_ioctl(void *dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct mgx_softc *sc = dev;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_MGX;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width = sc->sc_sunfb.sf_width;
		wdf->depth = sc->sc_sunfb.sf_depth;
		wdf->cmsize = 256;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = mgx_getcmap(sc->sc_cmap, cm);
		if (error != 0)
			return (error);
		break;
	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = mgx_putcmap(sc->sc_cmap, cm);
		if (error != 0)
			return (error);
		mgx_loadcmap(sc, cm->index, cm->count);
		break;

	default:
		return (-1);
	}

	return (0);
}

paddr_t
mgx_mmap(void *v, off_t offset, int prot)
{
	struct mgx_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, offset) | PMAP_NC);
	}

	return (-1);
}

int
mgx_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct mgx_softc *sc = v;

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
mgx_free_screen(void *v, void *cookie)
{
	struct mgx_softc *sc = v;

	sc->sc_nscreens--;
}

int
mgx_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

void
mgx_burner(void *v, u_int on, u_int flags)
{
	struct mgx_softc *sc = v;

	sc->sc_vidc[CRTC_COMMAND] = 1;	/* trigger? */
	if (on)
		sc->sc_vidc[CRTC_DATA] &= ~CD_DISABLEVIDEO;
	else
		sc->sc_vidc[CRTC_DATA] |= CD_DISABLEVIDEO;
}

/*
 * Colormap handling routines
 */

void
mgx_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct mgx_softc *sc = v;

	index *= 3;
	sc->sc_cmap[index++] = r;
	sc->sc_cmap[index++] = g;
	sc->sc_cmap[index] = b;

	mgx_loadcmap(sc, index, 1);
}

void
mgx_loadcmap(struct mgx_softc *sc, int start, int ncolors)
{
	u_int8_t *color;
	int i;

	/*
	 * Apparently there is no way to load an incomplete cmap to this
	 * DAC. What a waste.
	 */
	for (color = sc->sc_cmap, i = 0; i < 256 * 3; i++)
		sc->sc_vidc[CRTC_CMAP] = *color++;
}

int
mgx_getcmap(u_int8_t *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	for (i = 0; i < count; i++) {
		if ((error =
		    copyout(cm + (index + i) * 3 + 0, &rcm->red[i], 1)) != 0)
			return (error);
		if ((error =
		    copyout(cm + (index + i) * 3 + 1, &rcm->green[i], 1)) != 0)
			return (error);
		if ((error =
		    copyout(cm + (index + i) * 3 + 2, &rcm->blue[i], 1)) != 0)
			return (error);
	}

	return (0);
}

int
mgx_putcmap(u_int8_t *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	for (i = 0; i < count; i++) {
		if ((error =
		    copyin(&rcm->red[i], cm + (index + i) * 3 + 0, 1)) != 0)
			return (error);
		if ((error =
		    copyin(&rcm->green[i], cm + (index + i) * 3 + 1, 1)) != 0)
			return (error);
		if ((error =
		    copyin(&rcm->blue[i], cm + (index + i) * 3 + 2, 1)) != 0)
			return (error);
	}

	return (0);
}
