/*	$OpenBSD: cgsix.c,v 1.7 2002/02/05 20:00:04 jason Exp $	*/

/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/sbus/sbusvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rcons/raster.h>

union bt_cmap {
	u_int8_t cm_map[256][3];	/* 256 r/b/g entries */
	u_int32_t cm_chip[256 * 3 / 4];	/* the way the chip is loaded */
};

#define	BT_ADDR		0x00		/* map address register */
#define	BT_CMAP		0x04		/* colormap data register */
#define	BT_CTRL		0x08		/* control register */
#define	BT_OMAP		0x0c		/* overlay (cursor) map register */

#define	BT_WRITE(sc, reg, val) \
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_bt_regs, (reg), (val))
#define	BT_READ(sc, reg) \
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_bt_regs, (reg))

#define	BT_INIT(sc) do {					\
	BT_WRITE((sc), BT_ADDR, 0x06);	/* command reg */	\
	BT_WRITE((sc), BT_CTRL, 0x73);	/* overlay plane */	\
	BT_WRITE((sc), BT_ADDR, 0x04);  /* read mask */		\
	BT_WRITE((sc), BT_CTRL, 0xff);	/* color planes */	\
} while (0)

#define	BT_D4M3(x)	((((x) >> 2) << 1) + ((x) >> 2)) /* (x / 4) * 3 */
#define	BT_D4M4(x)	((x) & ~3)			 /* (x / 4) * 4 */

#define	CGSIX_ROM_OFFSET	0x000000
#define	CGSIX_BT_OFFSET		0x200000
#define	CGSIX_BT_SIZE		(sizeof(u_int32_t) * 4)
#define	CGSIX_DHC_OFFSET	0x240000
#define	CGSIX_ALT_OFFSET	0x280000
#define	CGSIX_FHC_OFFSET	0x300000
#define	CGSIX_FHC_SIZE		(sizeof(u_int32_t) * 1)
#define	CGSIX_THC_OFFSET	0x301000
#define	CGSIX_THC_SIZE		(sizeof(u_int32_t) * 640)
#define	CGSIX_FBC_OFFSET	0x700000
#define	CGSIX_TEC_OFFSET	0x701000
#define	CGSIX_VID_OFFSET	0x800000
#define	CGSIX_VID_SIZE		(1024 * 1024)

struct cgsix_softc {
	struct device sc_dev;
	struct sbusdev sc_sd;
	bus_space_tag_t sc_bustag;
	bus_addr_t sc_paddr;
	bus_space_handle_t sc_bt_regs;
	bus_space_handle_t sc_fhc_regs;
	bus_space_handle_t sc_thc_regs;
	bus_space_handle_t sc_vid_regs;
	int sc_nscreens;
	int sc_width, sc_height, sc_depth, sc_linebytes;
	struct rcons sc_rcons;
	struct raster sc_raster;
	union bt_cmap sc_cmap;
};

#define	CG6_USER_FBC	0x70000000
#define	CG6_USER_TEC	0x70001000
#define	CG6_USER_BTREGS	0x70002000
#define	CG6_USER_FHC	0x70004000
#define	CG6_USER_THC	0x70005000
#define	CG6_USER_ROM	0x70006000
#define	CG6_USER_RAM	0x70016000
#define	CG6_USER_DHC	0x80000000

struct mmo {
	u_long	mo_uaddr;		/* user (virtual address */
	u_long	mo_size;		/* size, or 0 for video ram size */
	u_long	mo_physoff;		/* offset from sc_physadr */
};

struct wsdisplay_emulops cgsix_emulops = {
	rcons_cursor,
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr cgsix_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global. */
	0,
	0, 0,
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *cgsix_scrlist[] = {
	&cgsix_stdscreen,
	/* XXX other formats? */
};

struct wsscreen_list cgsix_screenlist = {
	sizeof(cgsix_scrlist) / sizeof(struct wsscreen_descr *), cgsix_scrlist
};

int cgsix_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
int cgsix_alloc_screen __P((void *, const struct wsscreen_descr *, void **,
    int *, int *, long *));
void cgsix_free_screen __P((void *, void *));
int cgsix_show_screen __P((void *, void *, int,
    void (*cb) __P((void *, int, int)), void *));
paddr_t cgsix_mmap __P((void *, off_t, int));
int cgsix_is_console __P((int));
int cg6_bt_getcmap __P((union bt_cmap *, struct wsdisplay_cmap *));
int cg6_bt_putcmap __P((union bt_cmap *, struct wsdisplay_cmap *));
void cgsix_loadcmap __P((struct cgsix_softc *, u_int, u_int));
void cgsix_setcolor __P((struct cgsix_softc *, u_int,
    u_int8_t, u_int8_t, u_int8_t));

static int a2int __P((char *, int));

struct wsdisplay_accessops cgsix_accessops = {
	cgsix_ioctl,
	cgsix_mmap,
	cgsix_alloc_screen,
	cgsix_free_screen,
	cgsix_show_screen,
	0 /* load_font */
};

int	cgsixmatch	__P((struct device *, void *, void *));
void	cgsixattach	__P((struct device *, struct device *, void *));

struct cfattach cgsix_ca = {
	sizeof (struct cgsix_softc), cgsixmatch, cgsixattach
};

struct cfdriver cgsix_cd = {
	NULL, "cgsix", DV_DULL
};

int
cgsixmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0);
}

void    
cgsixattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cgsix_softc *sc = (struct cgsix_softc *)self;
	struct sbus_attach_args *sa = aux;
	struct wsemuldisplaydev_attach_args waa;
	int console, i;
	long defattr;

	sc->sc_bustag = sa->sa_bustag;
	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot, sa->sa_offset);

	if (sa->sa_nreg != 1) {
		printf(": expected %d registers, got %d\n", 1, sa->sa_nreg);
		goto fail;
	}

	/*
	 * Map just BT, FHC, THC, and video RAM.
	 */
	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_BT_OFFSET,
	    CGSIX_BT_SIZE, BUS_SPACE_MAP_LINEAR, 0, &sc->sc_bt_regs) != 0) {
		printf(": cannot map bt registers\n", self->dv_xname);
		goto fail_bt;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_FHC_OFFSET,
	    CGSIX_FHC_SIZE, BUS_SPACE_MAP_LINEAR, 0, &sc->sc_fhc_regs) != 0) {
		printf(": cannot map fhc registers\n", self->dv_xname);
		goto fail_fhc;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_THC_OFFSET,
	    CGSIX_THC_SIZE, BUS_SPACE_MAP_LINEAR, 0, &sc->sc_thc_regs) != 0) {
		printf(": cannot map thc registers\n", self->dv_xname);
		goto fail_thc;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_VID_OFFSET,
	    CGSIX_VID_SIZE, BUS_SPACE_MAP_LINEAR, 0, &sc->sc_vid_regs) != 0) {
		printf(": cannot map vid registers\n", self->dv_xname);
		goto fail_vid;
	}

	console = cgsix_is_console(sa->sa_node);

	BT_WRITE(sc, BT_ADDR, 0);
	for (i = 0; i < 256; i++) {
		sc->sc_cmap.cm_map[i][0] = BT_READ(sc, BT_CMAP) >> 24;
		sc->sc_cmap.cm_map[i][1] = BT_READ(sc, BT_CMAP) >> 24;
		sc->sc_cmap.cm_map[i][2] = BT_READ(sc, BT_CMAP) >> 24;
	}

	sc->sc_depth = getpropint(sa->sa_node, "depth", -1);
	if (sc->sc_depth == -1)
		sc->sc_depth = 8;

	sc->sc_linebytes = getpropint(sa->sa_node, "linebytes", -1);
	if (sc->sc_linebytes == -1)
		sc->sc_linebytes = 1152;

	sc->sc_height = getpropint(sa->sa_node, "height", -1);
	if (sc->sc_height == -1)
		sc->sc_height = 900;

	sc->sc_width = getpropint(sa->sa_node, "width", -1);
	if (sc->sc_width == -1)
		sc->sc_width = 1152;

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	sc->sc_rcons.rc_sp = &sc->sc_raster;
	sc->sc_raster.width = sc->sc_width;
	sc->sc_raster.height = sc->sc_height;
	sc->sc_raster.depth = sc->sc_depth;
	sc->sc_raster.linelongs = sc->sc_linebytes / 4;
	sc->sc_raster.pixels = (void *)sc->sc_vid_regs;

	if (console == 0 ||
	    romgetcursoraddr(&sc->sc_rcons.rc_crowp, &sc->sc_rcons.rc_ccolp)) {
		sc->sc_rcons.rc_crow = sc->sc_rcons.rc_ccol = -1;
		sc->sc_rcons.rc_crowp = &sc->sc_rcons.rc_crow;
		sc->sc_rcons.rc_ccolp = &sc->sc_rcons.rc_ccol;
	}

	sc->sc_rcons.rc_maxcol =
	    a2int(getpropstring(optionsnode, "screen-#columns"), 80);
	sc->sc_rcons.rc_maxrow =
	    a2int(getpropstring(optionsnode, "screen-#rows"), 34);

	rcons_init(&sc->sc_rcons,
	    sc->sc_rcons.rc_maxrow, sc->sc_rcons.rc_maxcol);

	cgsix_stdscreen.nrows = sc->sc_rcons.rc_maxrow;
	cgsix_stdscreen.ncols = sc->sc_rcons.rc_maxcol;
	cgsix_stdscreen.textops = &cgsix_emulops;
	rcons_alloc_attr(&sc->sc_rcons, 0, 0, 0, &defattr);

	printf("\n");

	if (console) {
		cgsix_setcolor(sc, WSCOL_BLACK, 0, 0, 0);
		cgsix_setcolor(sc, 255, 255, 255, 255);
		cgsix_setcolor(sc, WSCOL_RED, 255, 0, 0);
		cgsix_setcolor(sc, WSCOL_GREEN, 0, 255, 0);
		cgsix_setcolor(sc, WSCOL_BROWN, 154, 85, 46);
		cgsix_setcolor(sc, WSCOL_BLUE, 0, 0, 255);
		cgsix_setcolor(sc, WSCOL_MAGENTA, 255, 255, 0);
		cgsix_setcolor(sc, WSCOL_CYAN, 0, 255, 255);
		cgsix_setcolor(sc, WSCOL_WHITE, 255, 255, 255);

		wsdisplay_cnattach(&cgsix_stdscreen, &sc->sc_rcons,
		    *sc->sc_rcons.rc_ccolp, *sc->sc_rcons.rc_crowp, defattr);
	}

	waa.console = console;
	waa.scrdata = &cgsix_screenlist;
	waa.accessops = &cgsix_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);

	return;

fail_vid:
	bus_space_unmap(sa->sa_bustag, sc->sc_thc_regs, CGSIX_THC_SIZE);
fail_thc:
	bus_space_unmap(sa->sa_bustag, sc->sc_fhc_regs, CGSIX_FHC_SIZE);
fail_fhc:
	bus_space_unmap(sa->sa_bustag, sc->sc_bt_regs, CGSIX_BT_SIZE);
fail_bt:
fail:
}

int
cgsix_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgsix_softc *sc = v;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = sc->sc_height;
		wdf->width  = sc->sc_width;
		wdf->depth  = sc->sc_depth;
		wdf->cmsize = 256;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cg6_bt_getcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		break;

	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cg6_bt_putcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		cgsix_loadcmap(sc, cm->index, cm->count);
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return -1; /* not supported yet */
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

	*cookiep = &sc->sc_rcons;
	*curyp = *sc->sc_rcons.rc_crowp;
	*curxp = *sc->sc_rcons.rc_ccolp;
	rcons_alloc_attr(&sc->sc_rcons, 0, 0, 0, attrp);
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
	void (*cb) __P((void *, int, int));
	void *cbarg;
{
	return (0);
}

paddr_t
cgsix_mmap(v, off, prot)
	void *v;
	off_t off;
	int prot;
{
	struct cgsix_softc *sc = v;

	if (off & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (off >= 0 && off < (sc->sc_linebytes * sc->sc_height))
		return (bus_space_mmap(sc->sc_bustag, sc->sc_paddr,
		    off + CGSIX_VID_OFFSET, prot, BUS_SPACE_MAP_LINEAR));

	return (-1);
}

static int
a2int(char *cp, int deflt)
{
	int i = 0;

	if (*cp == '\0')
		return (deflt);
	while (*cp != '\0')
		i = i * 10 + *cp++ - '0';
	return (i);
}

int
cgsix_is_console(node)
	int node;
{
	extern int fbnode;

	return (fbnode == node);
}

int
cg6_bt_getcmap(bcm, rcm)
	union bt_cmap *bcm;
	struct wsdisplay_cmap *rcm;
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || index + count > 256)
		return (EINVAL);
	for (i = 0; i < count; i++) {
		if ((error = copyout(&bcm->cm_map[index + i][0],
		    &rcm->red[i], 1)) != 0)
			return (error);
		if ((error = copyout(&bcm->cm_map[index + i][1],
		    &rcm->green[i], 1)) != 0)
			return (error);
		if ((error = copyout(&bcm->cm_map[index + i][2],
		    &rcm->blue[i], 1)) != 0)
			return (error);
	}
	return (0);
}

int
cg6_bt_putcmap(bcm, rcm)
	union bt_cmap *bcm;
	struct wsdisplay_cmap *rcm;
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || rcm->count > 256 ||
	    (rcm->index + rcm->count) > 256)
		return (EINVAL);
	for (i = 0; i < count; i++) {
		if ((error = copyin(&rcm->red[i],
		    &bcm->cm_map[index + i][0], 1)) != 0)
			return (error);
		if ((error = copyin(&rcm->green[i],
		    &bcm->cm_map[index + i][1], 1)) != 0)
			return (error);
		if ((error = copyin(&rcm->blue[i],
		    &bcm->cm_map[index + i][2], 1)) != 0)
			return (error);
	}
	return (0);
}

void
cgsix_loadcmap(sc, start, ncolors)
	struct cgsix_softc *sc;
	u_int start, ncolors;
{
	u_int cstart;
	u_int32_t v;
	int count;

	cstart = BT_D4M3(start);
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	BT_WRITE(sc, BT_ADDR, BT_D4M4(start) << 24);
	while (--count >= 0) {
		v = sc->sc_cmap.cm_chip[cstart];
		BT_WRITE(sc, BT_CMAP, v << 0);
		BT_WRITE(sc, BT_CMAP, v << 8);
		BT_WRITE(sc, BT_CMAP, v << 16);
		BT_WRITE(sc, BT_CMAP, v << 24);
		cstart++;
	}
}

void
cgsix_setcolor(sc, index, r, g, b)
	struct cgsix_softc *sc;
	u_int index;
	u_int8_t r, g, b;
{
	union bt_cmap *bcm = &sc->sc_cmap;

	bcm->cm_map[index][0] = r;
	bcm->cm_map[index][1] = g;
	bcm->cm_map[index][2] = b;
	cgsix_loadcmap(sc, index, 1);
}
