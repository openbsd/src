/*	$OpenBSD: simplefb.c,v 1.1 2017/01/03 19:57:01 kettenis Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

struct simplefb_format {
	const char *format;
	int depth;
	int rpos, rnum;
	int gpos, gnum;
	int bpos, bnum;
};

/*
 * Supported pixel formats.  Layout ommitted when it matches the
 * rasops defaults.
 */
struct simplefb_format simplefb_formats[] = {
	{ "r5g6b5", 16 },
	{ "x1r5g5b5", 15 },
	{ "a1r5g5b5", 15 },
	{ "r8g8b8", 24 },
	{ "x8r8g8b8", 32 },
	{ "a8r8g8b8", 32 },
	{ "a8b8g8r8", 32, 0, 8, 8, 8, 16, 8 }
};

struct simplefb_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct rasops_info	sc_ri;
	struct wsscreen_descr	sc_wsd;
	struct wsscreen_list	sc_wsl;
	struct wsscreen_descr	*sc_scrlist[1];

	struct simplefb_format	*sc_format;
	paddr_t			sc_paddr;
	psize_t			sc_psize;
};

int	simplefb_match(struct device *, void *, void *);
void	simplefb_attach(struct device *, struct device *, void *);

struct cfattach simplefb_ca = {
	sizeof(struct simplefb_softc), simplefb_match, simplefb_attach
};

struct cfdriver simplefb_cd = {
	NULL, "simplefb", DV_DULL
};

int	simplefb_wsioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	simplefb_wsmmap(void *, off_t, int);
int	simplefb_alloc_screen(void *, const struct wsscreen_descr *,
	    void **, int *, int *, long *);

struct wsdisplay_accessops simplefb_accessops = {
	.ioctl = simplefb_wsioctl,
	.mmap = simplefb_wsmmap,
	.alloc_screen = simplefb_alloc_screen,
	.free_screen = rasops_free_screen,
	.show_screen = rasops_show_screen,
	.getchar = rasops_getchar,
	.load_font = rasops_load_font,
	.list_font = rasops_list_font,
};

int
simplefb_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "simple-framebuffer");
}

void
simplefb_attach(struct device *parent, struct device *self, void *aux)
{
	struct simplefb_softc *sc = (struct simplefb_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct rasops_info *ri = &sc->sc_ri;
	struct wsemuldisplaydev_attach_args waa;
	char format[16];
	int i;

	if (faa->fa_nreg < 1)
		return;

	format[0] = 0;
	OF_getprop(faa->fa_node, "format", format, sizeof(format));
	format[sizeof(format) - 1] = 0;

	for (i = 0; i < nitems(simplefb_formats); i++) {
		if (strcmp(format, simplefb_formats[i].format) == 0) {
			sc->sc_format = &simplefb_formats[i];
			break;
		}
	}
	if (sc->sc_format == NULL) {
		printf(": unsupported format \"%s\"\n", format);
		return;
	}

	sc->sc_iot = faa->fa_iot;
	sc->sc_paddr = faa->fa_reg[0].addr;
	sc->sc_psize = faa->fa_reg[0].size;
	if (bus_space_map(sc->sc_iot, sc->sc_paddr, sc->sc_psize,
	    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	ri->ri_width = OF_getpropint(faa->fa_node, "width", 0);
	ri->ri_height = OF_getpropint(faa->fa_node, "height", 0);
	ri->ri_stride = OF_getpropint(faa->fa_node, "stride", 0);
	ri->ri_depth = sc->sc_format->depth;
	ri->ri_rpos = sc->sc_format->rpos;
	ri->ri_rnum = sc->sc_format->rnum;
	ri->ri_gpos = sc->sc_format->gpos;
	ri->ri_gnum = sc->sc_format->gnum;
	ri->ri_bpos = sc->sc_format->bpos;
	ri->ri_bnum = sc->sc_format->bnum;
	ri->ri_flg = RI_CENTER | RI_CLEAR | RI_FULLCLEAR;
	ri->ri_flg |= RI_VCONS | RI_WRONLY;
	ri->ri_bits = bus_space_vaddr(sc->sc_iot, sc->sc_ioh);
	ri->ri_hw = sc;

	printf(": %dx%d\n", ri->ri_width, ri->ri_height);

	rasops_init(ri, 160, 160);

	strlcpy(sc->sc_wsd.name, "std", sizeof(sc->sc_wsd.name));
	sc->sc_wsd.capabilities = ri->ri_caps;
	sc->sc_wsd.nrows = ri->ri_rows;
	sc->sc_wsd.ncols = ri->ri_cols;
	sc->sc_wsd.textops = &ri->ri_ops;
	sc->sc_wsd.fontwidth = ri->ri_font->fontwidth;
	sc->sc_wsd.fontheight = ri->ri_font->fontheight;

	sc->sc_scrlist[0] = &sc->sc_wsd;
	sc->sc_wsl.nscreens = 1;
	sc->sc_wsl.screens = (const struct wsscreen_descr **)sc->sc_scrlist;

	memset(&waa, 0, sizeof(waa));
	waa.scrdata = &sc->sc_wsl;
	waa.accessops = &simplefb_accessops;
	waa.accesscookie = ri;

	config_found_sm(self, &waa, wsemuldisplaydevprint,
	    wsemuldisplaydevsubmatch);
}

int
simplefb_wsioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct rasops_info *ri = v;
	struct wsdisplay_fbinfo	*wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = WSDISPLAY_TYPE_EFIFB;
		return 0;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;	/* color map is unavailable */
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = ri->ri_stride;
		break;
	case WSDISPLAYIO_SMODE:
		break;
	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		switch (ri->ri_depth) {
		case 32:
			*(u_int *)data = WSDISPLAYIO_DEPTH_24_32;
			break;
		case 24:
			*(u_int *)data = WSDISPLAYIO_DEPTH_24_24;
			break;
		case 16:
			*(u_int *)data = WSDISPLAYIO_DEPTH_16;
			break;
		case 15:
			*(u_int *)data = WSDISPLAYIO_DEPTH_15;
			break;
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}

	return 0;
}

paddr_t
simplefb_wsmmap(void *v, off_t off, int prot)
{
	struct rasops_info *ri = v;
	struct simplefb_softc *sc = ri->ri_hw;

	if (off < 0 || off >= sc->sc_psize)
		return -1;

	return sc->sc_paddr + off;
}

int
simplefb_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	return rasops_alloc_screen(v, cookiep, curxp, curyp, attrp);
}
