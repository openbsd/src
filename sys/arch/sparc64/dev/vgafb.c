/*	$OpenBSD: vgafb.c,v 1.1 2001/12/14 14:59:04 jason Exp $	*/

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

#include <dev/pci/pcivar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rcons/raster.h>

struct vgafb_softc {
	struct device sc_dev;
	struct sbusdev sc_sd;
	int sc_nscreens;
	int sc_width, sc_height, sc_depth, sc_linebytes, sc_node;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_bh;
	struct rcons sc_rcons;
	struct raster sc_raster;
};

struct wsdisplay_emulops vgafb_emulops = {
	rcons_cursor,
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr vgafb_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global. */
	0,
	0, 0,
	WSSCREEN_REVERSE
};

const struct wsscreen_descr *vgafb_scrlist[] = {
	&vgafb_stdscreen,
	/* XXX other formats? */
};

struct wsscreen_list vgafb_screenlist = {
	sizeof(vgafb_scrlist) / sizeof(struct wsscreen_descr *), vgafb_scrlist
};

int vgafb_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
int vgafb_alloc_screen __P((void *, const struct wsscreen_descr *, void **,
    int *, int *, long *));
void vgafb_free_screen __P((void *, void *));
int vgafb_show_screen __P((void *, void *, int,
    void (*cb) __P((void *, int, int)), void *));
paddr_t vgafb_mmap __P((void *, off_t, int));
int vgafb_is_console __P((int));

static int a2int __P((char *, int));

struct wsdisplay_accessops vgafb_accessops = {
	vgafb_ioctl,
	vgafb_mmap,
	vgafb_alloc_screen,
	vgafb_free_screen,
	vgafb_show_screen,
	0 /* load_font */
};

int	vgafbmatch	__P((struct device *, void *, void *));
void	vgafbattach	__P((struct device *, struct device *, void *));

struct cfattach vgafb_ca = {
	sizeof (struct vgafb_softc), vgafbmatch, vgafbattach
};

struct cfdriver vgafb_cd = {
	NULL, "vgafb", DV_DULL
};

int
vgafbmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_PREHISTORIC &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PREHISTORIC_VGA)
		return (1);

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA)
		return (1);

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_MISC)
		return (1);

	return (0);
}

void    
vgafbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct vgafb_softc *sc = (struct vgafb_softc *)self;
	struct pci_attach_args *pa = aux;
	struct wsemuldisplaydev_attach_args waa;
	bus_size_t iosize;
	long defattr;
	int console;
	bus_addr_t base;

	sc->sc_node = PCITAG_NODE(pa->pa_tag);

	sc->sc_depth = getpropint(sc->sc_node, "depth", -1);
	if (sc->sc_depth == -1)
		sc->sc_depth = 8;

	sc->sc_linebytes = getpropint(sc->sc_node, "linebytes", -1);
	if (sc->sc_linebytes == -1)
		sc->sc_linebytes = 1152;

	sc->sc_height = getpropint(sc->sc_node, "height", -1);
	if (sc->sc_height == -1)
		sc->sc_height = 900;

	sc->sc_width = getpropint(sc->sc_node, "width", -1);
	if (sc->sc_width == -1)
		sc->sc_width = 1152;

	console = vgafb_is_console(sc->sc_node);

	if (pci_mem_find(pa->pa_pc, pa->pa_tag, 0x10, &base, &iosize, NULL)) {
		printf(": can't find mem space\n");
		goto fail;
	}
	if (bus_space_map2(pa->pa_memt, SBUS_BUS_SPACE, base, iosize, 0, NULL,
	    &sc->sc_bh)) {
		printf(": can't map mem space\n");
		goto fail;
	}
	sc->sc_bt = pa->pa_memt;

	sc->sc_rcons.rc_sp = &sc->sc_raster;
	sc->sc_raster.width = sc->sc_width;
	sc->sc_raster.height = sc->sc_height;
	sc->sc_raster.depth = sc->sc_depth;
	sc->sc_raster.linelongs = sc->sc_linebytes / 4;
	sc->sc_raster.pixels = (void *)sc->sc_bh;

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

	vgafb_stdscreen.nrows = sc->sc_rcons.rc_maxrow;
	vgafb_stdscreen.ncols = sc->sc_rcons.rc_maxcol;
	vgafb_stdscreen.textops = &vgafb_emulops;
	rcons_alloc_attr(&sc->sc_rcons, 0, 0, 0, &defattr);

	printf("\n");

	if (console)
		wsdisplay_cnattach(&vgafb_stdscreen, &sc->sc_rcons,
		    *sc->sc_rcons.rc_ccolp, *sc->sc_rcons.rc_crowp, defattr);

	waa.console = console;
	waa.scrdata = &vgafb_screenlist;
	waa.accessops = &vgafb_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);

	return;
fail:
}

int
vgafb_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct vgafb_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;

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

#if 0
	case WSDISPLAYIO_GETCMAP:
		return vgafb_getcmap(vc, (struct wsdisplay_cmap *)data);
	case WSDISPLAYIO_PUTCMAP:
		return vgafb_putcmap(vc, (struct wsdisplay_cmap *)data);
#endif

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
vgafb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct vgafb_softc *sc = v;

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
vgafb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct vgafb_softc *sc = v;

	sc->sc_nscreens--;
}

int
vgafb_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb) __P((void *, int, int));
	void *cbarg;
{
	return (0);
}

paddr_t
vgafb_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
#if 0
	struct vgafb_softc *sc = v;
#endif

	if (offset & PGOFSET)
		return (-1);
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
vgafb_is_console(node)
	int node;
{
	extern int fbnode;

	return (fbnode == node);
}
