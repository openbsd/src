/*	$OpenBSD: cgthree.c,v 1.22 2002/08/06 03:48:45 jason Exp $	*/

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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
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
#include <dev/rasops/rasops.h>

#define	CGTHREE_CTRL_OFFSET	0x400000
#define	CGTHREE_CTRL_SIZE	(sizeof(u_int32_t) * 8)
#define	CGTHREE_VID_OFFSET	0x800000
#define	CGTHREE_VID_SIZE	(1024 * 1024)

union bt_cmap {
	u_int8_t cm_map[256][3];	/* 256 r/b/g entries */
	u_int32_t cm_chip[256 * 3 / 4];	/* the way the chip is loaded */
};

#define	BT_ADDR		0x00		/* map address register */
#define	BT_CMAP		0x04		/* colormap data register */
#define	BT_CTRL		0x08		/* control register */
#define	BT_OMAP		0x0c		/* overlay (cursor) map register */
#define	CG3_FBC_CTRL	0x10		/* control */
#define	CG3_FBC_STAT	0x11		/* status */
#define	CG3_FBC_START	0x12		/* cursor start */
#define	CG3_FBC_END	0x13		/* cursor end */
#define	CG3_FBC_VCTRL	0x14		/* 12 bytes of timing goo */

#define	BT_WRITE(sc, reg, val) \
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_ctrl_regs, (reg), (val))
#define	BT_READ(sc, reg) \
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_ctrl_regs, (reg))
#define	BT_BARRIER(sc,reg,flags) \
    bus_space_barrier((sc)->sc_bustag, (sc)->sc_ctrl_regs, (reg), \
	sizeof(u_int32_t), (flags))

#define	BT_D4M3(x)	((((x) >> 2) << 1) + ((x) >> 2)) /* (x / 4) * 3 */
#define	BT_D4M4(x)	((x) & ~3)			 /* (x / 4) * 4 */

#define	FBC_CTRL_IENAB		0x80	/* interrupt enable */
#define	FBC_CTRL_VENAB		0x40	/* video enable */
#define	FBC_CTRL_TIME		0x20	/* timing enable */
#define	FBC_CTRL_CURS		0x10	/* cursor compare enable */
#define	FBC_CTRL_XTAL		0x0c	/* xtal select (0,1,2,test): */
#define	FBC_CTRL_XTAL_0		0x00	/*  0 */
#define	FBC_CTRL_XTAL_1		0x04	/*  0 */
#define	FBC_CTRL_XTAL_2		0x08	/*  0 */
#define	FBC_CTRL_XTAL_TEST	0x0c	/*  0 */
#define	FBC_CTRL_DIV		0x03	/* divisor (1,2,3,4): */
#define	FBC_CTRL_DIV_1		0x00	/*  / 1 */
#define	FBC_CTRL_DIV_2		0x01	/*  / 2 */
#define	FBC_CTRL_DIV_3		0x02	/*  / 3 */
#define	FBC_CTRL_DIV_4		0x03	/*  / 4 */

#define	FBC_STAT_INTR		0x80	/* interrupt pending */
#define	FBC_STAT_RES		0x70	/* monitor sense: */
#define	FBC_STAT_RES_1024	0x10	/*  1024x768 */
#define	FBC_STAT_RES_1280	0x40	/*  1280x1024 */
#define	FBC_STAT_RES_1152	0x30	/*  1152x900 */
#define	FBC_STAT_RES_1152A	0x40	/*  1152x900x76, A */
#define	FBC_STAT_RES_1600	0x50	/*  1600x1200 */
#define	FBC_STAT_RES_1152B	0x60	/*  1152x900x86, B */
#define	FBC_STAT_ID		0x0f	/* id mask: */
#define	FBC_STAT_ID_COLOR	0x01	/*  color */
#define	FBC_STAT_ID_MONO	0x02	/*  monochrome */
#define	FBC_STAT_ID_MONOECL	0x03	/*  monochrome, ecl */

#define	FBC_READ(sc, reg) \
    bus_space_read_1((sc)->sc_bustag, (sc)->sc_ctrl_regs, (reg))
#define	FBC_WRITE(sc, reg, val) \
    bus_space_write_1((sc)->sc_bustag, (sc)->sc_ctrl_regs, (reg), (val))

struct cgthree_softc {
	struct device sc_dev;
	struct sbusdev sc_sd;
	bus_space_tag_t sc_bustag;
	bus_addr_t sc_paddr;
	bus_space_handle_t sc_ctrl_regs;
	bus_space_handle_t sc_vid_regs;
	int sc_nscreens;
	int sc_width, sc_height, sc_depth, sc_linebytes;
	union bt_cmap sc_cmap;
	struct rasops_info sc_rasops;
	u_int sc_mode;
};

struct wsscreen_descr cgthree_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global. */
	0,
	0, 0,
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *cgthree_scrlist[] = {
	&cgthree_stdscreen,
	/* XXX other formats? */
};

struct wsscreen_list cgthree_screenlist = {
	sizeof(cgthree_scrlist) / sizeof(struct wsscreen_descr *), cgthree_scrlist
};

int cgthree_ioctl(void *, u_long, caddr_t, int, struct proc *);
int cgthree_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void cgthree_free_screen(void *, void *);
int cgthree_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t cgthree_mmap(void *, off_t, int);
int cgthree_is_console(int);
void cgthree_loadcmap(struct cgthree_softc *, u_int, u_int);
int cg3_bt_putcmap(union bt_cmap *, struct wsdisplay_cmap *);
int cg3_bt_getcmap(union bt_cmap *, struct wsdisplay_cmap *);
void cgthree_setcolor(struct cgthree_softc *, u_int,
    u_int8_t, u_int8_t, u_int8_t);
void cgthree_burner(void *, u_int, u_int);
void cgthree_reset(struct cgthree_softc *);
static int a2int(char *, int);

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

#define	CG3_TYPE_DEFAULT	0
#define	CG3_TYPE_76HZ		1
#define	CG3_TYPE_SMALL		2

struct cg3_videoctrl {
	u_int8_t	sense;
	u_int8_t	vctrl[12];
	u_int8_t	ctrl;
} cg3_videoctrl[] = {
	{	/* cpd-1790 */
		0x31,
		{ 0xbb, 0x2b, 0x04, 0x14, 0xae, 0x03,
		  0xa8, 0x24, 0x01, 0x05, 0xff, 0x01 },
		FBC_CTRL_XTAL_0 | FBC_CTRL_DIV_1
	},
	{	/* gdm-20e20 */
		0x41,
		{ 0xb7, 0x27, 0x03, 0x0f, 0xae, 0x03,
		  0xae, 0x2a, 0x01, 0x09, 0xff, 0x01 },
		FBC_CTRL_XTAL_1 | FBC_CTRL_DIV_1
	},
	{	/* defaults, should be last */
		0xff,
		{ 0xbb, 0x2b, 0x03, 0x0b, 0xb3, 0x03,
		  0xaf, 0x2b, 0x02, 0x0a, 0xff, 0x01 },
		0,
	},
};

int
cgthreematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0);
}

void    
cgthreeattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cgthree_softc *sc = (struct cgthree_softc *)self;
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
	 * Map just CTRL and video RAM.
	 */
	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGTHREE_CTRL_OFFSET,
	    CGTHREE_CTRL_SIZE, BUS_SPACE_MAP_LINEAR,
	    0, &sc->sc_ctrl_regs) != 0) {
		printf(": cannot map ctrl registers\n");
		goto fail_ctrl;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGTHREE_VID_OFFSET,
	    CGTHREE_VID_SIZE, BUS_SPACE_MAP_LINEAR,
	    0, &sc->sc_vid_regs) != 0) {
		printf(": cannot map vid registers\n");
		goto fail_vid;
	}

	console = cgthree_is_console(sa->sa_node);

	sc->sc_depth = getpropint(sa->sa_node, "depth", 8);
	sc->sc_linebytes = getpropint(sa->sa_node, "linebytes", 1152);
	sc->sc_height = getpropint(sa->sa_node, "height", 900);
	sc->sc_width = getpropint(sa->sa_node, "width", 1152);

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	cgthree_reset(sc);

	BT_WRITE(sc, BT_ADDR, 0);
	for (i = 0; i < 256 * 3 / 4; i++)
		sc->sc_cmap.cm_chip[i] = BT_READ(sc, BT_CMAP);

	cgthree_burner(sc, 1, 0);

	sc->sc_rasops.ri_depth = sc->sc_depth;
	sc->sc_rasops.ri_stride = sc->sc_linebytes;
	sc->sc_rasops.ri_flg = RI_CENTER |
	    (console ? 0 : RI_CLEAR);
	sc->sc_rasops.ri_bits = (void *)bus_space_vaddr(sc->sc_bustag,
	    sc->sc_vid_regs);
	sc->sc_rasops.ri_width = sc->sc_width;
	sc->sc_rasops.ri_height = sc->sc_height;
	sc->sc_rasops.ri_hw = sc;

	rasops_init(&sc->sc_rasops,
	    a2int(getpropstring(optionsnode, "screen-#rows"), 34),
	    a2int(getpropstring(optionsnode, "screen-#columns"), 80));

	cgthree_stdscreen.nrows = sc->sc_rasops.ri_rows;
	cgthree_stdscreen.ncols = sc->sc_rasops.ri_cols;
	cgthree_stdscreen.textops = &sc->sc_rasops.ri_ops;
	sc->sc_rasops.ri_ops.alloc_attr(&sc->sc_rasops,
	    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, &defattr);

	printf("\n");

	if (console) {
		int *ccolp, *crowp;

		cgthree_setcolor(sc, WSCOL_BLACK, 0, 0, 0);
		cgthree_setcolor(sc, 255, 0, 0, 0);
		cgthree_setcolor(sc, WSCOL_RED, 255, 0, 0);
		cgthree_setcolor(sc, WSCOL_GREEN, 0, 255, 0);
		cgthree_setcolor(sc, WSCOL_BROWN, 154, 85, 46);
		cgthree_setcolor(sc, WSCOL_BLUE, 0, 0, 255);
		cgthree_setcolor(sc, WSCOL_MAGENTA, 255, 255, 0);
		cgthree_setcolor(sc, WSCOL_CYAN, 0, 255, 255);
		cgthree_setcolor(sc, WSCOL_WHITE, 255, 255, 255);

		if (romgetcursoraddr(&crowp, &ccolp))
			ccolp = crowp = NULL;
		if (ccolp != NULL)
			sc->sc_rasops.ri_ccol = *ccolp;
		if (crowp != NULL)
			sc->sc_rasops.ri_crow = *crowp;

		wsdisplay_cnattach(&cgthree_stdscreen, &sc->sc_rasops,
		    sc->sc_rasops.ri_ccol, sc->sc_rasops.ri_crow, defattr);
	}

	waa.console = console;
	waa.scrdata = &cgthree_screenlist;
	waa.accessops = &cgthree_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);

	return;

fail_vid:
	bus_space_unmap(sa->sa_bustag, sc->sc_ctrl_regs, CGTHREE_CTRL_SIZE);
fail_ctrl:
fail:
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
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
		break;
	case WSDISPLAYIO_SMODE:
		sc->sc_mode = *(u_int *)data;
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
		error = cg3_bt_getcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		break;

	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cg3_bt_putcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		cgthree_loadcmap(sc, cm->index, cm->count);
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

	*cookiep = &sc->sc_rasops;
	*curyp = 0;
	*curxp = 0;
	sc->sc_rasops.ri_ops.alloc_attr(&sc->sc_rasops,
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

#define	START		(128 * 1024 + 128 * 1024)
#define	NOOVERLAY	(0x04000000)

paddr_t
cgthree_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct cgthree_softc *sc = v;

	if (offset & PGOFSET || offset < 0)
		return (-1);

	switch (sc->sc_mode) {
	case WSDISPLAYIO_MODE_MAPPED:
		if (offset >= NOOVERLAY)
			offset -= NOOVERLAY;
		else if (offset >= START)
			offset -= START;
		else
			offset = 0;
		if (offset >= sc->sc_linebytes * sc->sc_height)
			return (-1);
		return (bus_space_mmap(sc->sc_bustag, sc->sc_paddr,
		    CGTHREE_VID_OFFSET + offset, prot, BUS_SPACE_MAP_LINEAR));
	case WSDISPLAYIO_MODE_DUMBFB:
		if (offset < (sc->sc_linebytes * sc->sc_height))
			return (bus_space_mmap(sc->sc_bustag, sc->sc_paddr,
			    CGTHREE_VID_OFFSET + offset, prot,
			    BUS_SPACE_MAP_LINEAR));
		break;
	}
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
cgthree_is_console(node)
	int node;
{
	extern int fbnode;

	return (fbnode == node);
}

void
cgthree_setcolor(sc, index, r, g, b)
	struct cgthree_softc *sc;
	u_int index;
	u_int8_t r, g, b;
{
	union bt_cmap *bcm = &sc->sc_cmap;

	bcm->cm_map[index][0] = r;
	bcm->cm_map[index][1] = g;
	bcm->cm_map[index][2] = b;
	cgthree_loadcmap(sc, index, 1);
}

void
cgthree_loadcmap(sc, start, ncolors)
	struct cgthree_softc *sc;
	u_int start, ncolors;
{
	u_int cstart;
	int count;

	cstart = BT_D4M3(start);
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	BT_WRITE(sc, BT_ADDR, BT_D4M4(start));
	while (--count >= 0) {
		BT_WRITE(sc, BT_CMAP, sc->sc_cmap.cm_chip[cstart]);
		cstart++;
	}
}

int
cg3_bt_getcmap(bcm, rcm)
	union bt_cmap *bcm;
	struct wsdisplay_cmap *rcm;
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || count > 256 - index)
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
cg3_bt_putcmap(bcm, rcm)
	union bt_cmap *bcm;
	struct wsdisplay_cmap *rcm;
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || count > 256 - index)
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
cgthree_reset(sc)
	struct cgthree_softc *sc;
{
	int i, j;
	u_int8_t sts, ctrl;

	sts = FBC_READ(sc, CG3_FBC_STAT);
	ctrl = FBC_READ(sc, CG3_FBC_CTRL);

	if (ctrl & FBC_CTRL_TIME) {
		/* already initialized */
		return;
	}

	for (i = 0; i <  sizeof(cg3_videoctrl)/sizeof(cg3_videoctrl[0]); i++) {
		if (cg3_videoctrl[i].sense == 0xff ||
		    (cg3_videoctrl[i].sense ==
		     (sts & (FBC_STAT_RES | FBC_STAT_ID)))) {
			for (j = 0; j < 12; j++)
				FBC_WRITE(sc, CG3_FBC_VCTRL + j,
				    cg3_videoctrl[i].vctrl[j]);
			ctrl &= ~(FBC_CTRL_XTAL | FBC_CTRL_DIV);
			ctrl |= cg3_videoctrl[i].ctrl |
			    FBC_CTRL_TIME;
			FBC_WRITE(sc, CG3_FBC_CTRL, ctrl);
			break;
		}
	}

	BT_WRITE(sc, BT_ADDR, 0x04);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0xff);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);

	BT_WRITE(sc, BT_ADDR, 0x05);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0x00);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);

	BT_WRITE(sc, BT_ADDR, 0x06);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0x70);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);

	BT_WRITE(sc, BT_ADDR, 0x07);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0x00);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);
}

void
cgthree_burner(vsc, on, flags)
	void *vsc;
	u_int on, flags;
{
	struct cgthree_softc *sc = vsc;
	int s;
	u_int8_t fbc;

	s = splhigh();
	fbc = FBC_READ(sc, CG3_FBC_CTRL);
	if (on)
		fbc |= FBC_CTRL_VENAB | FBC_CTRL_TIME;
	else {
		fbc &= ~FBC_CTRL_VENAB;
		if (flags & WSDISPLAY_BURN_VBLANK)
			fbc &= ~FBC_CTRL_TIME;
	}
	FBC_WRITE(sc, CG3_FBC_CTRL, fbc);
	splx(s);
}
