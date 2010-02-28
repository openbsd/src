/*	$OpenBSD: smfb.c,v 1.7 2010/02/28 21:35:41 miod Exp $	*/

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
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

/*
 * Minimal SiliconMotion SM502 and SM712 frame buffer driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <loongson/dev/voyagerreg.h>
#include <loongson/dev/voyagervar.h>
#include <loongson/dev/smfbreg.h>

#define	DPR_READ(fb, reg)	(fb)->dpr[(reg) / 4]
#define	DPR_WRITE(fb, reg, val)	(fb)->dpr[(reg) / 4] = (val)

#define	REG_READ(fb, reg)	(fb)->regs[(reg) / 4]
#define	REG_WRITE(fb, reg, val)	(fb)->regs[(reg) / 4] = (val)

struct smfb_softc;

/* minimal frame buffer information, suitable for early console */
struct smfb {
	struct smfb_softc	*sc;
	struct rasops_info	ri;
	int			is5xx;

	volatile uint32_t	*regs;
	volatile uint32_t	*dpr;
	volatile uint8_t	*mmio;
	struct wsscreen_descr	wsd;
};

struct smfb_softc {
	struct device		 sc_dev;
	struct smfb		*sc_fb;
	struct smfb		 sc_fb_store;

	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;

	bus_space_tag_t		 sc_regt;
	bus_space_handle_t	 sc_regh;

	struct wsscreen_list	 sc_wsl;
	struct wsscreen_descr	*sc_scrlist[1];
	int			 sc_nscr;
};

int	smfb_pci_match(struct device *, void *, void *);
void	smfb_pci_attach(struct device *, struct device *, void *);
int	smfb_voyager_match(struct device *, void *, void *);
void	smfb_voyager_attach(struct device *, struct device *, void *);

const struct cfattach smfb_pci_ca = {
	sizeof(struct smfb_softc), smfb_pci_match, smfb_pci_attach
};

const struct cfattach smfb_voyager_ca = {
	sizeof(struct smfb_softc), smfb_voyager_match, smfb_voyager_attach
};

struct cfdriver smfb_cd = {
	NULL, "smfb", DV_DULL
};

int	smfb_alloc_screen(void *, const struct wsscreen_descr *, void **, int *,
	    int *, long *);
void	smfb_free_screen(void *, void *);
int	smfb_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	smfb_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
paddr_t	smfb_mmap(void *, off_t, int);

struct wsdisplay_accessops smfb_accessops = {
	smfb_ioctl,
	smfb_mmap,
	smfb_alloc_screen,
	smfb_free_screen,
	smfb_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL	/* burner */
};

int	smfb_setup(struct smfb *, vaddr_t, vaddr_t);

void	smfb_copyrect(struct smfb *, int, int, int, int, int, int);
void	smfb_fillrect(struct smfb *, int, int, int, int, int);
int	smfb_copyrows(void *, int, int, int);
int	smfb_copycols(void *, int, int, int, int);
int	smfb_do_cursor(struct rasops_info *);
int	smfb_erasecols(void *, int, int, int, long);
int	smfb_eraserows(void *, int, int, long);
int	smfb_wait(struct smfb *);

void	smfb_attach_common(struct smfb_softc *, int);

static struct smfb smfbcn;

const struct pci_matchid smfb_devices[] = {
	{ PCI_VENDOR_SMI, PCI_PRODUCT_SMI_SM712 }
};

int
smfb_pci_match(struct device *parent, void *vcf, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	return pci_matchbyid(pa, smfb_devices, nitems(smfb_devices));
}

int
smfb_voyager_match(struct device *parent, void *vcf, void *aux)
{
	struct voyager_attach_args *vaa = (struct voyager_attach_args *)aux;
	struct cfdata *cf = (struct cfdata *)vcf;

	return strcmp(vaa->vaa_name, cf->cf_driver->cd_name) == 0;
}

void
smfb_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct smfb_softc *sc = (struct smfb_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM,
	    BUS_SPACE_MAP_LINEAR, &sc->sc_memt, &sc->sc_memh,
	    NULL, NULL, 0) != 0) {
		printf(": can't map frame buffer\n");
		return;
	}

	smfb_attach_common(sc, 0);
}

void
smfb_voyager_attach(struct device *parent, struct device *self, void *aux)
{
	struct smfb_softc *sc = (struct smfb_softc *)self;
	struct voyager_attach_args *vaa = (struct voyager_attach_args *)aux;

	sc->sc_memt = vaa->vaa_fbt;
	sc->sc_memh = vaa->vaa_fbh;
	sc->sc_regt = vaa->vaa_mmiot;
	sc->sc_regh = vaa->vaa_mmioh;

	smfb_attach_common(sc, 1);
}

void
smfb_attach_common(struct smfb_softc *sc, int is5xx)
{
	struct wsemuldisplaydev_attach_args waa;
	vaddr_t fbbase, regbase;
	int console;

	console = smfbcn.ri.ri_hw != NULL;

	if (console) {
		sc->sc_fb = &smfbcn;
		sc->sc_fb->sc = sc;
	} else {
		sc->sc_fb = &sc->sc_fb_store;
		sc->sc_fb->is5xx = is5xx;
		fbbase = (vaddr_t)bus_space_vaddr(sc->sc_memt, sc->sc_memh);
		if (is5xx) {
			regbase = (vaddr_t)bus_space_vaddr(sc->sc_regt,
			    sc->sc_regh);
		} else {
			regbase = 0;
		}
		if (smfb_setup(sc->sc_fb, fbbase, regbase) != 0) {
			printf(": can't setup frame buffer\n");
			return;
		}
	}

	/* XXX print resolution */
	printf("\n");

	sc->sc_scrlist[0] = &sc->sc_fb->wsd;
	sc->sc_wsl.nscreens = 1;
	sc->sc_wsl.screens = (const struct wsscreen_descr **)sc->sc_scrlist;

	waa.console = console;
	waa.scrdata = &sc->sc_wsl;
	waa.accessops = &smfb_accessops;
	waa.accesscookie = sc;
	waa.defaultscreens = 0;

	config_found((struct device *)sc, &waa, wsemuldisplaydevprint);
}

/*
 * wsdisplay accesops
 */

int
smfb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct smfb_softc *sc = (struct smfb_softc *)v;
	struct rasops_info *ri = &sc->sc_fb->ri;

	if (sc->sc_nscr > 0)
		return ENOMEM;

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, attrp);
	sc->sc_nscr++;

	return 0;
}

void
smfb_free_screen(void *v, void *cookie)
{
	struct smfb_softc *sc = (struct smfb_softc *)v;

	sc->sc_nscr--;
}

int
smfb_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct smfb_softc *sc = (struct smfb_softc *)v;
	struct rasops_info *ri = &sc->sc_fb->ri;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(uint *)data = WSDISPLAY_TYPE_SMFB;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(uint *)data = ri->ri_stride;
		break;
	default:
		return -1;
	}

	return 0;
}

int
smfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return 0;
}

paddr_t
smfb_mmap(void *v, off_t offset, int prot)
{
	struct smfb_softc *sc = (struct smfb_softc *)v;
	struct rasops_info *ri = &sc->sc_fb->ri;
	paddr_t pa;

	if ((offset & PAGE_MASK) != 0)
		return -1;

	if (offset < 0 || offset >= ri->ri_stride * ri->ri_height)
		return -1;

	pa = XKPHYS_TO_PHYS((paddr_t)ri->ri_bits) + offset;
	return atop(pa);
}

/*
 * Frame buffer initialization.
 */

int
smfb_setup(struct smfb *fb, vaddr_t fbbase, vaddr_t regbase)
{
	struct rasops_info *ri;
	int accel = 0;

	ri = &fb->ri;
	ri->ri_width = 1024;
	ri->ri_height = 600;
	ri->ri_depth = 16;
	ri->ri_stride = (ri->ri_width * ri->ri_depth) / 8;
	ri->ri_flg = RI_CENTER | RI_CLEAR | RI_FULLCLEAR;
	ri->ri_bits = (void *)fbbase;
	ri->ri_hw = fb;

#ifdef __MIPSEL__
	/* swap B and R */
	ri->ri_rnum = 5;
	ri->ri_rpos = 11;
	ri->ri_gnum = 6;
	ri->ri_gpos = 5;
	ri->ri_bnum = 5;
	ri->ri_bpos = 0;
#endif

	rasops_init(ri, 160, 160);

	strlcpy(fb->wsd.name, "std", sizeof(fb->wsd.name));
	fb->wsd.ncols = ri->ri_cols;
	fb->wsd.nrows = ri->ri_rows;
	fb->wsd.textops = &ri->ri_ops;
	fb->wsd.fontwidth = ri->ri_font->fontwidth;
	fb->wsd.fontheight = ri->ri_font->fontheight;
	fb->wsd.capabilities = ri->ri_caps;

	if (fb->is5xx) {
		fb->dpr = (volatile uint32_t *)(regbase + SM5XX_DPR_BASE);
		fb->mmio = NULL;
		fb->regs = (volatile uint32_t *)(regbase + SM5XX_MMIO_BASE);
		accel = 1;
	} else {
		fb->dpr = (volatile uint32_t *)(fbbase + SM7XX_DPR_BASE);
		fb->mmio = (volatile uint8_t *)(fbbase + SM7XX_MMIO_BASE);
		fb->regs = NULL;
		accel = 1;
	}

	/*
	 * Setup 2D acceleration whenever possible
	 */

	if (accel) {
		if (smfb_wait(fb) != 0)
			accel = 0;
	}
	if (accel) {
		DPR_WRITE(fb, DPR_CROP_TOPLEFT_COORDS, DPR_COORDS(0, 0));
		/* use of width both times is intentional */
		DPR_WRITE(fb, DPR_PITCH,
		    DPR_COORDS(ri->ri_width, ri->ri_width));
		DPR_WRITE(fb, DPR_SRC_WINDOW,
		    DPR_COORDS(ri->ri_width, ri->ri_width));
		DPR_WRITE(fb, DPR_BYTE_BIT_MASK, 0xffffffff);
		DPR_WRITE(fb, DPR_COLOR_COMPARE_MASK, 0);
		DPR_WRITE(fb, DPR_COLOR_COMPARE, 0);
		DPR_WRITE(fb, DPR_SRC_BASE, 0);
		DPR_WRITE(fb, DPR_DST_BASE, 0);
		DPR_READ(fb, DPR_DST_BASE);

		ri->ri_ops.copycols = smfb_copycols;
		ri->ri_ops.copyrows = smfb_copyrows;
		ri->ri_ops.erasecols = smfb_erasecols;
		ri->ri_ops.eraserows = smfb_eraserows;
	}

	return 0;
}

void
smfb_copyrect(struct smfb *fb, int sx, int sy, int dx, int dy, int w, int h)
{
	uint32_t dir;

	/* Compute rop direction */
	if (sy < dy || (sy == dy && sx <= dx)) {
		sx += w - 1;
		dx += w - 1;
		sy += h - 1;
		dy += h - 1;
		dir = DE_CTRL_RTOL;
	} else
		dir = 0;

	DPR_WRITE(fb, DPR_SRC_COORDS, DPR_COORDS(sx, sy));
	DPR_WRITE(fb, DPR_DST_COORDS, DPR_COORDS(dx, dy));
	DPR_WRITE(fb, DPR_SPAN_COORDS, DPR_COORDS(w, h));
	DPR_WRITE(fb, DPR_DE_CTRL, DE_CTRL_START | DE_CTRL_ROP_ENABLE | dir |
	    (DE_CTRL_COMMAND_BITBLT << DE_CTRL_COMMAND_SHIFT) |
	    (DE_CTRL_ROP_SRC << DE_CTRL_ROP_SHIFT));
	DPR_READ(fb, DPR_DE_CTRL);

	smfb_wait(fb);
}

void
smfb_fillrect(struct smfb *fb, int x, int y, int w, int h, int bg)
{
	struct rasops_info *ri;

	ri = &fb->ri;

	DPR_WRITE(fb, DPR_FG_COLOR, ri->ri_devcmap[bg]);
	DPR_WRITE(fb, DPR_DST_COORDS, DPR_COORDS(x, y));
	DPR_WRITE(fb, DPR_SPAN_COORDS, DPR_COORDS(w, h));
	DPR_WRITE(fb, DPR_DE_CTRL, DE_CTRL_START | DE_CTRL_ROP_ENABLE |
	    (DE_CTRL_COMMAND_SOLIDFILL << DE_CTRL_COMMAND_SHIFT) |
	    (DE_CTRL_ROP_SRC << DE_CTRL_ROP_SHIFT));
	DPR_READ(fb, DPR_DE_CTRL);

	smfb_wait(fb);
}

int
smfb_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct smfb *fb = ri->ri_hw;
	struct wsdisplay_font *f = ri->ri_font;

	num *= f->fontheight;
	src *= f->fontheight;
	dst *= f->fontheight;

	smfb_copyrect(fb, ri->ri_xorigin, ri->ri_yorigin + src,
	    ri->ri_xorigin, ri->ri_yorigin + dst, ri->ri_emuwidth, num);

	return 0;
}

int
smfb_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct smfb *fb = ri->ri_hw;
	struct wsdisplay_font *f = ri->ri_font;

	num *= f->fontwidth;
	src *= f->fontwidth;
	dst *= f->fontwidth;
	row *= f->fontheight;

	smfb_copyrect(fb, ri->ri_xorigin + src, ri->ri_yorigin + row,
	    ri->ri_xorigin + dst, ri->ri_yorigin + row, num, f->fontheight);

	return 0;
}

int
smfb_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct smfb *fb = ri->ri_hw;
	struct wsdisplay_font *f = ri->ri_font;
	int bg, fg;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	row *= f->fontheight;
	col *= f->fontwidth;
	num *= f->fontwidth;

	smfb_fillrect(fb, ri->ri_xorigin + col, ri->ri_yorigin + row,
	    num, f->fontheight, ri->ri_devcmap[bg]);

	return 0;
}

int
smfb_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct smfb *fb = ri->ri_hw;
	struct wsdisplay_font *f = ri->ri_font;
	int bg, fg;
	int x, y, w;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	if ((num == ri->ri_rows) && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
		num = ri->ri_height;
		x = y = 0;
		w = ri->ri_width;
	} else {
		num *= f->fontheight;
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + row * f->fontheight;
		w = ri->ri_emuwidth;
	}
	smfb_fillrect(fb, x, y, w, num, ri->ri_devcmap[bg]);

	return 0;
}

int
smfb_wait(struct smfb *fb)
{
	uint32_t reg;
	int i;

	i = 10000;
	while (i-- != 0) {
		if (fb->is5xx) {
			reg = REG_READ(fb, VOYAGER_SYSTEM_CONTROL);
			if ((reg & (VSC_FIFO_EMPTY | VSC_2DENGINE_BUSY)) ==
			    VSC_FIFO_EMPTY)
				return 0;
		} else {
			fb->mmio[0x3c4] = 0x16;
			(void)fb->mmio[0x3c4];	/* posted write */
			reg = fb->mmio[0x3c5];
			if ((reg & 0x18) == 0x10)
				return 0;
		}
		delay(1);
	}

	return EBUSY;
}

/*
 * Early console code
 */

int smfb_cnattach(bus_space_tag_t, bus_space_tag_t, pcitag_t, pcireg_t);

int
smfb_cnattach(bus_space_tag_t memt, bus_space_tag_t iot, pcitag_t tag,
    pcireg_t id)
{
	long defattr;
	struct rasops_info *ri;
	bus_space_handle_t fbh, regh;
	vaddr_t fbbase, regbase;
	pcireg_t bar;
	int rc, is5xx;

	/* filter out unrecognized devices */
	switch(id) {
	default:
		return ENODEV;
	case PCI_ID_CODE(PCI_VENDOR_SMI, PCI_PRODUCT_SMI_SM712):
		is5xx = 0;
		break;
	case PCI_ID_CODE(PCI_VENDOR_SMI, PCI_PRODUCT_SMI_SM501):
		is5xx = 1;
		break;
	}

	smfbcn.is5xx = is5xx;

	bar = pci_conf_read_early(tag, PCI_MAPREG_START);
	if (PCI_MAPREG_TYPE(bar) != PCI_MAPREG_TYPE_MEM)
		return EINVAL;
	rc = bus_space_map(memt, PCI_MAPREG_MEM_ADDR(bar), 1 /* XXX */,
	    BUS_SPACE_MAP_LINEAR, &fbh);
	if (rc != 0)
		return rc;
	fbbase = (vaddr_t)bus_space_vaddr(memt, fbh);

	if (smfbcn.is5xx) {
		bar = pci_conf_read_early(tag, PCI_MAPREG_START + 0x04);
		if (PCI_MAPREG_TYPE(bar) != PCI_MAPREG_TYPE_MEM)
			return EINVAL;
		rc = bus_space_map(memt, PCI_MAPREG_MEM_ADDR(bar), 1 /* XXX */,
		    BUS_SPACE_MAP_LINEAR, &regh);
		if (rc != 0)
			return rc;
		regbase = (vaddr_t)bus_space_vaddr(memt, regh);
	} else {
		regbase = 0;
	}

	rc = smfb_setup(&smfbcn, fbbase, regbase);
	if (rc != 0)
		return rc;

	ri = &smfbcn.ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&smfbcn.wsd, ri, 0, 0, defattr);

	return 0;
}
