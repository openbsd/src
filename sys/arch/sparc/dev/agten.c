/*	$OpenBSD: agten.c,v 1.7 2005/01/05 23:04:24 miod Exp $	*/
/*
 * Copyright (c) 2002, 2003, Miodrag Vallat.
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
 * Fujitsu AG-10 framebuffer driver.
 *
 * The AG-10 is mostly made of:
 * - a 3DLabs 300SX Glint chip, with two 6MB independent framebuffer spaces
 * - a Number Nine Imagine 128 chip with its own 4MB framebuffer space
 * - a Weitek P9100 with its own 2MB of framebuffer memory
 * - an IBM PaletteDAC 561 ramdac
 * - an Analog Devices ADSP-21062
 *
 * All of these chips (memory, registers, etc) are mapped in the SBus
 * memory space associated to the board. What is unexpected, however, is
 * that there are also PCI registers mappings for the first three chips!
 *
 * The three graphics chips act as overlays of each other, for the final
 * video output.
 *
 * The PROM initialization will use the I128 framebuffer memory for output,
 * which is ``above'' the P9100. The P9100 seems to only be there to provide
 * a simple RAMDAC interface, but its frame buffer memory is accessible and
 * will appear as an ``underlay'' plane.
 */

/*
 * TODO
 * - initialize the I128 in 32bit mode
 * - use the i128 acceleration features
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

#include <dev/ic/p9000.h>
#include <dev/ic/ibm561reg.h>

#include <sparc/dev/sbusvar.h>

struct agten_cmap {
	u_int8_t	cm_red[256];
	u_int8_t	cm_green[256];
	u_int8_t	cm_blue[256];
};

/* per-display variables */
struct agten_softc {
	struct	sunfb sc_sunfb;			/* common base part */
	struct	sbusdev sc_sd;			/* sbus device */

	struct	rom_reg sc_phys;		/* physical address and */
	off_t	sc_physoffset;			/* offset for frame buffer */

	volatile u_int8_t *sc_p9100;
	struct agten_cmap sc_cmap;		/* shadow color map */

	volatile u_int32_t *sc_i128_fb;

	int	sc_nscreens;
};

int agten_ioctl(void *, u_long, caddr_t, int, struct proc *);
int agten_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void agten_free_screen(void *, void *);
int agten_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t agten_mmap(void *, off_t, int);
void agten_reset(struct agten_softc *);
void agten_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);

static __inline__ void ibm561_write(struct agten_softc *, u_int32_t, u_int32_t);
int agten_getcmap(struct agten_cmap *, struct wsdisplay_cmap *);
int agten_putcmap(struct agten_cmap *, struct wsdisplay_cmap *);
void agten_loadcmap(struct agten_softc *, u_int, u_int);

struct wsdisplay_accessops agten_accessops = {
	agten_ioctl,
	agten_mmap,
	agten_alloc_screen,
	agten_free_screen,
	agten_show_screen,
	NULL,   /* load_font */
	NULL,   /* scrollback */
	NULL,   /* getchar */
	NULL,	/* burner */
};

int agtenmatch(struct device *, void *, void *);
void agtenattach(struct device *, struct device *, void *);

struct cfattach agten_ca = {
	sizeof(struct agten_softc), agtenmatch, agtenattach
};

struct cfdriver agten_cd = {
	NULL, "agten", DV_DULL
};

int
agtenmatch(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, "PFU,aga") != 0)
		return (0);

	return (1);
}

void
agtenattach(struct device *parent, struct device *self, void *args)
{
	struct agten_softc *sc = (struct agten_softc *)self;
	struct confargs *ca = args;
	int node, isconsole;
	char *nam;

	node = ca->ca_ra.ra_node;
	nam = getpropstring(node, "model");
	printf(": model %s", nam);

	isconsole = node == fbnode;

	sc->sc_phys = ca->ca_ra.ra_reg[0];

	/*
	 * Map the various beasts of this card we are interested in.
	 */

	sc->sc_physoffset =
	    (off_t)getpropint(node, "i128_fb_physaddr", 0x8000000);
	sc->sc_i128_fb = mapiodev(ca->ca_ra.ra_reg, sc->sc_physoffset,
	    getpropint(node, "i128_fb_size", 0x400000));
	sc->sc_p9100 = mapiodev(ca->ca_ra.ra_reg,
	    getpropint(node, "p9100_reg_physaddr", 0x10a0000),
	    0x4000);

	/*
	 * For some reason the agten does not use the canonical name for
	 * properties, but uses an ffb_ prefix; and the linebytes property is
	 * missing.
	 * The following is a specific version of
	 *   fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, BUS_SBUS);
	 * using the correct property names.
	 */
#ifdef notyet
	sc->sc_sunfb.sf_depth = 32;
#else
	sc->sc_sunfb.sf_depth = getpropint(node, "ffb_depth", 8);
#endif
	sc->sc_sunfb.sf_width = getpropint(node, "ffb_width", 1152);
	sc->sc_sunfb.sf_height = getpropint(node, "ffb_height", 900);
	sc->sc_sunfb.sf_linebytes =
	    roundup(sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_depth) *
	        sc->sc_sunfb.sf_depth / 8;
	sc->sc_sunfb.sf_fbsize =
	    sc->sc_sunfb.sf_height * sc->sc_sunfb.sf_linebytes;

	printf(", %dx%d, depth %d\n",
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height,
	    sc->sc_sunfb.sf_depth);

	sc->sc_sunfb.sf_ro.ri_bits = (void *)sc->sc_i128_fb;
	
	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, isconsole ? 0 : RI_CLEAR);
	fbwscons_setcolormap(&sc->sc_sunfb, agten_setcolor);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, -1, NULL);
	}

	sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);

	fbwscons_attach(&sc->sc_sunfb, &agten_accessops, isconsole);
}

int
agten_ioctl(dev, cmd, data, flags, p)
	void *dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct agten_softc *sc = dev;
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
			error = agten_getcmap(&sc->sc_cmap, cm);
			if (error)
				return (error);
		}
		break;
	case WSDISPLAYIO_PUTCMAP:
		if (sc->sc_sunfb.sf_depth == 8) {
			cm = (struct wsdisplay_cmap *)data;
			error = agten_putcmap(&sc->sc_cmap, cm);
			if (error)
				return (error);
			agten_loadcmap(sc, 0, 256);
		}
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
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
agten_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct agten_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, sc->sc_physoffset + offset) |
		    PMAP_NC);
	}

	return (-1);	/* not a user-map offset */
}

void
agten_setcolor(v, index, r, g, b)
	void *v;
	u_int index;
	u_int8_t r, g, b;
{
	struct agten_softc *sc = v;

	sc->sc_cmap.cm_red[index] = r;
	sc->sc_cmap.cm_green[index] = g;
	sc->sc_cmap.cm_blue[index] = b;

	agten_loadcmap(sc, index, 1);
}

int
agten_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct agten_softc *sc = v;

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
agten_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct agten_softc *sc = v;

	sc->sc_nscreens--;
}

int
agten_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

int
agten_getcmap(struct agten_cmap *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count;
	int error;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	if ((error = copyout(&cm->cm_red[index], rcm->red, count)) != 0)
		return (error);
	if ((error = copyout(&cm->cm_green[index], rcm->green, count)) != 0)
		return (error);
	if ((error = copyout(&cm->cm_blue[index], rcm->blue, count)) != 0)
		return (error);

	return (0);
}

int
agten_putcmap(struct agten_cmap *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count;
	int error;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	if ((error = copyin(rcm->red, &cm->cm_red[index], count)) != 0)
		return (error);
	if ((error = copyin(rcm->green, &cm->cm_green[index], count)) != 0)
		return (error);
	if ((error = copyin(rcm->blue, &cm->cm_blue[index], count)) != 0)
		return (error);

	return (0);
}

static __inline__ void
ibm561_write(struct agten_softc *sc, u_int32_t reg, u_int32_t value)
{
	/*
	 * For some design reason the IBM561 PaletteDac needs to be fed
	 * values shifted left by 16 bits. What happened to simplicity?
	 */
	*(volatile u_int32_t *)(sc->sc_p9100 + P9100_RAMDAC_REGISTER(reg)) =
	    (value) << 16;
}

void
agten_loadcmap(struct agten_softc *sc, u_int start, u_int ncolors)
{
	int i;
	u_int8_t *red, *green, *blue;

	ibm561_write(sc, IBM561_ADDR_LOW,
	    (IBM561_CMAP_TABLE + start) & 0xff);
	ibm561_write(sc, IBM561_ADDR_HIGH,
	    ((IBM561_CMAP_TABLE + start) >> 8) & 0xff);

	red = sc->sc_cmap.cm_red;
	green = sc->sc_cmap.cm_green;
	blue = sc->sc_cmap.cm_blue;
	for (i = start; i < start + ncolors; i++) {
		ibm561_write(sc, IBM561_CMD_CMAP, *red++);
		ibm561_write(sc, IBM561_CMD_CMAP, *green++);
		ibm561_write(sc, IBM561_CMD_CMAP, *blue++);
	}
}
