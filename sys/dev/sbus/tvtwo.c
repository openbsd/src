/*	$OpenBSD: tvtwo.c,v 1.3 2005/01/05 23:04:25 miod Exp $	*/
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
 * Driver for the Parallax XVideo and PowerVideo graphics boards.
 *
 * Some details about these board are available at:
 * http://www.jlw.com/~woolsey/parallax/support/developers/xvideotech.html
 */

/*
 * The Parallax XVideo series frame buffers are 8/24-bit accelerated
 * frame buffers, with hardware MPEG capabilities using a CCube chipset.
 */

/*
 * Currently, this driver can only handle the 24-bit plane of the frame
 * buffer, in an unaccelerated mode.
 *
 * TODO:
 * - nvram handling
 * - use the accelerator
 * - interface to the c^3
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
#include <machine/bus.h>
#include <machine/pmap.h>
#include <machine/cpu.h>
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <dev/sbus/sbusvar.h>

/*
 * The memory layout of the board is as follows:
 *
 *	 PROM0		000000 - 00ffff
 *	 overlay plane	010000 - 037fff
 *	 registers	040000 - 0404d0
 *	 CCube		050000 - 05ffff
 *	 8-bit plane	080000 - 17ffff
 *	 24-bit plane	200000 - 6fffff
 *	 PROM1		7f0000 - 7fffff
 *
 * All of this is mapped using only one register (except for older models
 * which are not currently supported).
 * At PROM initialization, the board will be in 24-bit mode, so no specific
 * initialization is necessary.
 */

#define	PX_PROM0_OFFSET		0x000000
#define	PX_OVERAY_OFFSET	0x010000
#define	PX_REG_OFFSET		0x040000
#define	PX_CCUBE_OFFSET		0x050000
#define	PX_PLANE8_OFFSET	0x080000
#define	PX_PLANE24_OFFSET	0x200000
#define	PX_PROM1_OFFSET		0x7f0000

/*
 * Partial registers layout
 */

#define	PX_REG_DISPKLUDGE	0x00b8	/* write only */
#define	DISPKLUDGE_DEFAULT	0xc41f
#define	DISPKLUDGE_BLANK	(1 << 12)

#define	PX_REG_BT463		0x0480

#define	PX_REG_SIZE		0x04d0


/* per-display variables */
struct tvtwo_softc {
	struct	sunfb	sc_sunfb;	/* common base device */
	struct	sbusdev sc_sd;		/* sbus device */

	bus_space_tag_t	sc_bustag;
	bus_addr_t	sc_paddr;

	volatile u_int8_t *sc_regs;

	int	sc_nscreens;
};

int tvtwo_ioctl(void *, u_long, caddr_t, int, struct proc *);
int tvtwo_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void tvtwo_free_screen(void *, void *);
int tvtwo_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t tvtwo_mmap(void *, off_t, int);
void tvtwo_burner(void *, u_int, u_int);

static __inline__ void tvtwo_ramdac_wraddr(struct tvtwo_softc *sc,
    u_int32_t addr);
void tvtwo_initcmap(struct tvtwo_softc *);

struct wsdisplay_accessops tvtwo_accessops = {
	tvtwo_ioctl,
	tvtwo_mmap,
	tvtwo_alloc_screen,
	tvtwo_free_screen,
	tvtwo_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	tvtwo_burner,
};

int tvtwomatch(struct device *, void *, void *);
void tvtwoattach(struct device *, struct device *, void *);

struct cfattach tvtwo_ca = {
	sizeof(struct tvtwo_softc), tvtwomatch, tvtwoattach
};

struct cfdriver tvtwo_cd = {
	NULL, "tvtwo", DV_DULL
};

/*
 * Default frame buffer resolution, depending upon the "freqcode"
 */
const int defwidth[] = { 1152, 1152, 1152, 1024, 640, 1024 };
const int defheight[] = { 900, 900, 900, 768, 480, 1024 };

/*
 * Match an XVideo or PowerVideo card.
 */
int
tvtwomatch(struct device *parent, void *vcf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp(sa->sa_name, "PGI,tvtwo") != 0)
		return (0);

	return (1);
}

/*
 * Attach a display.
 */
void
tvtwoattach(struct device *parent, struct device *self, void *args)
{
	struct tvtwo_softc *sc = (struct tvtwo_softc *)self;
	struct sbus_attach_args *sa = args;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int node, width, height, freqcode;
	int isconsole;
	char *freqstring;

	bt = sa->sa_bustag;
	node = sa->sa_node;

	printf(": %s", getpropstring(node, "model"));
	printf(", revision %s\n", getpropstring(node, "revision"));

	/* We do not know how to handle older boards. */
	if (sa->sa_nreg != 1) {
		printf("%s: old-style boards with %d registers are not supported\n",
		    self->dv_xname, sa->sa_nreg);
		return;
	}

	isconsole = node == fbnode;

	/* Map registers. */
	sc->sc_bustag = bt;
	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + PX_REG_OFFSET,
	    PX_REG_SIZE, BUS_SPACE_MAP_LINEAR, 0, &bh) != 0) {
		printf("%s: couldn't map registers\n", self->dv_xname);
		return;
	}
	sc->sc_regs = bus_space_vaddr(bt, bh);

	/* Compute framebuffer size. */
	freqstring = getpropstring(node, "freqcode");
	freqcode = (int)*freqstring;
	if (freqcode == 'g')
		freqcode = '6';
	if (freqcode < '1' || freqcode > '6')
		freqcode = 0;
	else
		freqcode -= '1';

	width = getpropint(node, "hres", defwidth[freqcode]);
	height = getpropint(node, "vres", defheight[freqcode]);
	fb_setsize(&sc->sc_sunfb, 32, width, height,
	    node, 0);

	/* Map the frame buffer memory area we're interested in. */
	sc->sc_paddr = sbus_bus_addr(bt, sa->sa_slot, sa->sa_offset);
	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset + PX_PLANE24_OFFSET,
	    round_page(sc->sc_sunfb.sf_fbsize), BUS_SPACE_MAP_LINEAR, 0,
	    &bh) != 0) {
		printf("%s: couldn't map video memory\n", self->dv_xname);
		return;
	}
	sc->sc_sunfb.sf_ro.ri_bits = bus_space_vaddr(bt, bh);

	/* Initialize a direct color map. */
	tvtwo_initcmap(sc);

	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, isconsole ? 0 : RI_CLEAR);

	printf("%s: %dx%d\n", self->dv_xname,
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, -1, NULL);
	}

	sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);

	fbwscons_attach(&sc->sc_sunfb, &tvtwo_accessops, isconsole);
}

int
tvtwo_ioctl(void *dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct tvtwo_softc *sc = dev;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
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
		break;

	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return (-1);
	}

	return (0);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
tvtwo_mmap(void *v, off_t offset, int prot)
{
	struct tvtwo_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (bus_space_mmap(sc->sc_bustag, sc->sc_paddr,
		    PX_PLANE24_OFFSET + offset, prot, BUS_SPACE_MAP_LINEAR));
	}

	return (-1);
}

int
tvtwo_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct tvtwo_softc *sc = v;

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
tvtwo_free_screen(void *v, void *cookie)
{
	struct tvtwo_softc *sc = v;

	sc->sc_nscreens--;
}

int
tvtwo_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

/*
 * Simple Bt463 programming routines.
 */

static __inline__ void
tvtwo_ramdac_wraddr(struct tvtwo_softc *sc, u_int32_t addr)
{
	volatile u_int32_t *dac = (u_int32_t *)(sc->sc_regs + PX_REG_BT463);

	dac[0] = (addr & 0xff);		/* lo addr */
	dac[1] = ((addr >> 8) & 0xff);	/* hi addr */
}

void
tvtwo_initcmap(struct tvtwo_softc *sc)
{
	volatile u_int32_t *dac = (u_int32_t *)(sc->sc_regs + PX_REG_BT463);
	u_int32_t c;

	tvtwo_ramdac_wraddr(sc, 0);
	for (c = 0; c < 256; c++) {
		dac[3] = c;	/* R */
		dac[3] = c;	/* G */
		dac[3] = c;	/* B */
	}
}

void
tvtwo_burner(void *v, u_int on, u_int flags)
{
	struct tvtwo_softc *sc = v;
	volatile u_int32_t *dispkludge =
	    (u_int32_t *)(sc->sc_regs + PX_REG_DISPKLUDGE);

	if (on)
		*dispkludge = DISPKLUDGE_DEFAULT & ~DISPKLUDGE_BLANK;
	else
		*dispkludge = DISPKLUDGE_DEFAULT | DISPKLUDGE_BLANK;
}
