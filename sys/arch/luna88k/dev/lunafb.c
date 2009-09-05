/* $OpenBSD: lunafb.c,v 1.10 2009/09/05 14:09:35 miod Exp $ */
/* $NetBSD: lunafb.c,v 1.7.6.1 2002/08/07 01:48:34 lukem Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/errno.h>
#include <sys/buf.h>

#include <uvm/uvm_extern.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>

struct bt454 {
	u_int8_t bt_addr;		/* map address register */
	u_int8_t bt_cmap;		/* colormap data register */
};

struct bt458 {
	u_int8_t bt_addr;		/* map address register */
		unsigned :24;
	u_int8_t bt_cmap;		/* colormap data register */
		unsigned :24;
	u_int8_t bt_ctrl;		/* control register */
		unsigned :24;
	u_int8_t bt_omap;		/* overlay (cursor) map register */
		unsigned :24;
};

#define	OMFB_RFCNT	0xB1000000	/* video h-origin/v-origin */
#define	OMFB_PLANEMASK	0xB1040000	/* planemask register */
#define	OMFB_FB_WADDR	0xB1080008	/* common plane */
#define	OMFB_FB_RADDR	0xB10C0008	/* plane #0 */
#define	OMFB_ROPFUNC	0xB12C0000	/* ROP function code */
#define	OMFB_RAMDAC	0xC1100000	/* Bt454/Bt458 RAMDAC */
#define	OMFB_SIZE	(0xB1300000 - 0xB1080000 + NBPG)

struct om_hwdevconfig {
	int	dc_wid;			/* width of frame buffer */
	int	dc_ht;			/* height of frame buffer */
	int	dc_depth;		/* depth, bits per pixel */
	int	dc_rowbytes;		/* bytes in a FB scan line */
	int	dc_cmsize;		/* colormap size */
	vaddr_t	dc_videobase;		/* base of flat frame buffer */
	struct rasops_info dc_ri;	/* raster blitter variables */
};

struct hwcmap {
#define CMAP_SIZE 256
	u_int8_t r[CMAP_SIZE];
	u_int8_t g[CMAP_SIZE];
	u_int8_t b[CMAP_SIZE];
};

struct omfb_softc {
	struct device sc_dev;		/* base device */
	struct om_hwdevconfig *sc_dc;	/* device configuration */
	struct hwcmap sc_cmap;		/* software copy of colormap */
	int nscreens;
};

int  omgetcmap(struct omfb_softc *, struct wsdisplay_cmap *);
int  omsetcmap(struct omfb_softc *, struct wsdisplay_cmap *);

struct om_hwdevconfig omfb_console_dc;
void omfb_getdevconfig(paddr_t, struct om_hwdevconfig *);

/* in omrasops.c */
int	om_cursor(void *, int, int, int);
int	om_putchar(void *, int, int, u_int, long);
int	om_copycols(void *, int, int, int, int);
int	om_copyrows(void *, int, int, int num);
int	om_erasecols(void *, int, int, int, long);
int	om_eraserows(void *, int, int, long);

struct wsscreen_descr omfb_stdscreen = {
	"std"
};

const struct wsscreen_descr *_omfb_scrlist[] = {
	&omfb_stdscreen,
};

const struct wsscreen_list omfb_screenlist = {
	sizeof(_omfb_scrlist) / sizeof(struct wsscreen_descr *), _omfb_scrlist
};

int   omfbioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t omfbmmap(void *, off_t, int);
int   omfb_alloc_screen(void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *);
void  omfb_free_screen(void *, void *);
int   omfb_show_screen(void *, void *, int,
				void (*) (void *, int, int), void *);

const struct wsdisplay_accessops omfb_accessops = {
	omfbioctl,
	omfbmmap,
	omfb_alloc_screen,
	omfb_free_screen,
	omfb_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL	/* burner */
};

int  omfbmatch(struct device *, void *, void *);
void omfbattach(struct device *, struct device *, void *);

const struct cfattach fb_ca = {
	sizeof(struct omfb_softc), omfbmatch, omfbattach
};

struct cfdriver fb_cd = {
        NULL, "fb", DV_DULL
};

extern int hwplanebits;	/* hardware plane bits; retrieved at boot */

int omfb_console;
int omfb_cnattach(void);

int
omfbmatch(parent, cf, aux)
	struct device *parent;
	void *cf, *aux;
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, fb_cd.cd_name))
		return (0);
#if 0	/* XXX badaddr() bombs if no framebuffer is installed */
	if (badaddr((caddr_t)ma->ma_addr, 4))
		return (0);
#else
	if (hwplanebits == 0)
		return (0);
#endif
	return (1);
}

void
omfbattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct omfb_softc *sc = (struct omfb_softc *)self;
	struct wsemuldisplaydev_attach_args waa;

	if (omfb_console) {
		sc->sc_dc = &omfb_console_dc;
		sc->nscreens = 1;
	}
	else {
		sc->sc_dc = (struct om_hwdevconfig *)
		    malloc(sizeof(struct om_hwdevconfig), M_DEVBUF, M_WAITOK);
		omfb_getdevconfig(OMFB_FB_WADDR, sc->sc_dc);
	}
	printf(": %d x %d, %dbpp\n", sc->sc_dc->dc_wid, sc->sc_dc->dc_ht,
	    sc->sc_dc->dc_depth);

#if 0	/* WHITE on BLACK */
	cm = &sc->sc_cmap;
	memset(cm, 255, sizeof(struct hwcmap));
	cm->r[0] = cm->g[0] = cm->b[0] = 0;
#endif
	waa.console = omfb_console;
	waa.scrdata = &omfb_screenlist;
	waa.accessops = &omfb_accessops;
	waa.accesscookie = sc;
	waa.defaultscreens = 0;

	config_found(self, &waa, wsemuldisplaydevprint);
}

/* EXPORT */ int
omfb_cnattach()
{
	struct om_hwdevconfig *dc = &omfb_console_dc;
	struct rasops_info *ri = &dc->dc_ri;
	long defattr;

	omfb_getdevconfig(OMFB_FB_WADDR, dc);
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&omfb_stdscreen, ri, 0, 0, defattr);
	omfb_console = 1;
	return (0);
}

int
omfbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct omfb_softc *sc = v;
	struct om_hwdevconfig *dc = sc->sc_dc;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_LUNA;
		break;

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = dc->dc_ht;
		wsd_fbip->width = dc->dc_wid;
		wsd_fbip->depth = dc->dc_depth;
		wsd_fbip->cmsize = dc->dc_cmsize;
#undef fbt
		break;

	case WSDISPLAYIO_GETCMAP:
		return omgetcmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return omsetcmap(sc, (struct wsdisplay_cmap *)data);

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
omfbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct omfb_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);
	if (offset >= OMFB_SIZE || offset < 0)
		return (-1);

	return atop(trunc_page(sc->sc_dc->dc_videobase) + offset);
}

int
omgetcmap(sc, p)
	struct omfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;
        unsigned int cmsize;
	int error;

	cmsize = sc->sc_dc->dc_cmsize;
	if (index >= cmsize || count > cmsize - index)
		return (EINVAL);

	error = copyout(&sc->sc_cmap.r[index], p->red, count);
	if (error != 0)
		return (error);
	error = copyout(&sc->sc_cmap.g[index], p->green, count);
	if (error != 0)
		return (error);
	error = copyout(&sc->sc_cmap.b[index], p->blue, count);
	if (error != 0)
		return (error);

	return (0);
}

int
omsetcmap(sc, p)
	struct omfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;
        unsigned int cmsize, i;
	int error;

	cmsize = sc->sc_dc->dc_cmsize;
	if (index >= cmsize || count > cmsize - index)
		return (EINVAL);

	error = copyin(p->red, &sc->sc_cmap.r[index], count);
	if (error != 0)
		return (error);
	error = copyin(p->green, &sc->sc_cmap.g[index], count);
	if (error != 0)
		return (error);
	error = copyin(p->blue, &sc->sc_cmap.b[index], count);
	if (error != 0)
		return (error);

	if (hwplanebits == 4) {
		struct bt454 *odac = (struct bt454 *)OMFB_RAMDAC;
		odac->bt_addr = index;
		for (i = index; i < count; i++) {
			odac->bt_cmap = sc->sc_cmap.r[i];
			odac->bt_cmap = sc->sc_cmap.g[i];
			odac->bt_cmap = sc->sc_cmap.b[i];
		}
	}
	else if (hwplanebits == 8) {
		struct bt458 *ndac = (struct bt458 *)OMFB_RAMDAC;
		ndac->bt_addr = index;
		for (i = index; i < count; i++) {
			ndac->bt_cmap = sc->sc_cmap.r[i];
			ndac->bt_cmap = sc->sc_cmap.g[i];
			ndac->bt_cmap = sc->sc_cmap.b[i];
		}
	}
	return (0);
}

void
omfb_getdevconfig(paddr, dc)
	paddr_t paddr;
	struct om_hwdevconfig *dc;
{
	int bpp, i;
	struct rasops_info *ri;
	union {
		struct { short h, v; } p;
		u_int32_t u;
	} rfcnt;

	switch (hwplanebits) {
	case 8:
		bpp = 8;	/* XXX check monochrome bit in DIPSW */
		break;
	default:
	case 4:
		bpp = 4;	/* XXX check monochrome bit in DIPSW */
		break;
	case 1:
		bpp = 1;
		break;
	}
	dc->dc_wid = 1280;
	dc->dc_ht = 1024;
	dc->dc_depth = bpp;
	dc->dc_rowbytes = 2048 / 8;
	dc->dc_cmsize = (bpp == 1) ? 0 : 1 << bpp;
	dc->dc_videobase = paddr;

#if 0 /* WHITE on BLACK XXX experiment resulted in WHITE on SKYBLUE... */
	if (hwplanebits == 4) {
		/* XXX Need Bt454 initialization */
		struct bt454 *odac = (struct bt454 *)OMFB_RAMDAC;
		odac->bt_addr = 0;
		odac->bt_cmap = 0;
		odac->bt_cmap = 0;
		odac->bt_cmap = 0;
		for (i = 1; i < 16; i++) {
			odac->bt_cmap = 255;
			odac->bt_cmap = 255;
			odac->bt_cmap = 255;
		}
	}
	else if (hwplanebits == 8) {
		struct bt458 *ndac = (struct bt458 *)OMFB_RAMDAC;

		ndac->bt_addr = 0x04;
		ndac->bt_ctrl = 0xff; /* all planes will be read */
		ndac->bt_ctrl = 0x00; /* all planes have non-blink */
		ndac->bt_ctrl = 0x43; /* pallete enabled, ovly plane */
		ndac->bt_ctrl = 0x00; /* no test mode */
		ndac->bt_addr = 0;
		ndac->bt_cmap = 0;
		ndac->bt_cmap = 0;
		ndac->bt_cmap = 0;
		for (i = 1; i < 256; i++) {
			ndac->bt_cmap = 255;
			ndac->bt_cmap = 255;
			ndac->bt_cmap = 255;
		}
	}
#endif

	/* adjust h/v origin on screen */
	rfcnt.p.h = 7;
	rfcnt.p.v = -27;
	*(u_int32_t *)OMFB_RFCNT = rfcnt.u; /* single write of 0x007ffe6 */

	/* clear the screen */
	*(u_int32_t *)OMFB_PLANEMASK = 0xff;
	((u_int32_t *)OMFB_ROPFUNC)[5] = ~0;	/* ROP copy */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes / sizeof(u_int32_t); i++)
		*((u_int32_t *)dc->dc_videobase + i) = 0;
	*(u_int32_t *)OMFB_PLANEMASK = 0x01;

	/* initialize the raster */
	ri = &dc->dc_ri;
	ri->ri_width = dc->dc_wid;
	ri->ri_height = dc->dc_ht;
	ri->ri_depth = 1;	/* since planes are independently addressed */
	ri->ri_stride = dc->dc_rowbytes;
	ri->ri_bits = (void *)dc->dc_videobase;
	ri->ri_flg = RI_CENTER;
	ri->ri_hw = dc;

	rasops_init(ri, 35, 80);

	omfb_stdscreen.ncols = ri->ri_cols;
	omfb_stdscreen.nrows = ri->ri_rows;
	ri->ri_ops.cursor = om_cursor;
	ri->ri_ops.putchar = om_putchar;
	ri->ri_ops.copycols = om_copycols;
	ri->ri_ops.erasecols = om_erasecols;
	ri->ri_ops.copyrows = om_copyrows;
	ri->ri_ops.eraserows = om_eraserows;
	omfb_stdscreen.textops = &ri->ri_ops;
	omfb_stdscreen.fontwidth = ri->ri_font->fontwidth;
	omfb_stdscreen.fontheight = ri->ri_font->fontheight;
	omfb_stdscreen.capabilities = ri->ri_caps & ~WSSCREEN_UNDERLINE;
}

int
omfb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct omfb_softc *sc = v;
	struct rasops_info *ri = &sc->sc_dc->dc_ri;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;
	*curxp = 0;
	*curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, attrp);
	sc->nscreens++;
	return (0);
}

void
omfb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct omfb_softc *sc = v;

	sc->nscreens--;
}

int
omfb_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return 0;
}
