/*	$OpenBSD: ifb.c,v 1.1 2008/03/23 20:07:21 miod Exp $	*/

/*
 * Copyright (c) 2007, 2008 Miodrag Vallat.
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
 * Least-effort driver for the Sun Expert3D cards (based on the
 * ``Wildcat'' chips.
 *
 * There is no public documentation for these chips available.
 * Since they are no longer supported by 3DLabs (which got bought by
 * Creative), and Sun does not want to publish even minimal information
 * or source code, the best we can do is experiment.
 *
 * Quoting Alan Coopersmith in
 * http://mail.opensolaris.org/pipermail/opensolaris-discuss/2005-December/0118885.html
 * ``Unfortunately, the lawyers have asked we not give details about why
 *   specific components are not being released.''
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/pciio.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/openfirm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/rasops/rasops.h>

#include <machine/fbvar.h>

/*
 * Parts of the following hardware knowledge come from David S. Miller's
 * XVR-500 Linux driver (drivers/video/sunxvr500.c).
 */

/*
 * The Expert3D and Expert3d-Lite cards are built around the Wildcat
 * 5110, 6210 and 7210 chips.
 *
 * The card exposes the following resources:
 * - a 32MB aperture window in which views to the different frame buffer
 *   areas can be mapped, in the first BAR.
 * - a 64KB PROM and registers area, in the second BAR, with the registers
 *   starting 32KB within this area.
 * - a 8MB memory mapping, which purpose is unknown, in the third BAR.
 *
 * In the state the PROM leaves us in, the 8MB frame buffer windows map
 * the video memory as interleaved stripes:
 * - frame buffer #0 will map horizontal pixels 00-1f, 60-9f, e0-11f, etc.
 * - frame buffer #1 will map horizontal pixels 20-5f, a0-df, 120-15f, etc.
 * However the non-visible parts of each stripe can still be addressed
 * (probably for fast screen switching).
 *
 * Unfortunately, since we do not know how to reconfigured the stripes
 * to provide at least a linear frame buffer, we have to write to both
 * windows and have them provide the complete image. This however leads
 * to parasite overlays appearing sometimes in the screen...
 */

#define	IFB_REG_OFFSET			0x8000

/*
 * 0040 component configuration
 * This register controls which parts of the board will be addressed by
 * writes to other configuration registers.
 * Apparently the low two bytes control the frame buffer windows for the
 * given head (starting at 1).
 * The high two bytes are texture related.
 */
#define	IFB_REG_COMPONENT_SELECT	0x0040

/*
 * 0058 magnifying configuration
 * This register apparently controls magnifying.
 * bits 5-6 select the window width divider (00: by 2, 01: by 4, 10: by 8,
 *   11: by 16)
 * bits 7-8 select the zoom factor (00: disabled, 01: x2, 10: x4, 11: x8)
 */
#define	IFB_REG_MAGNIFY			0x0058
#define	IFB_REG_MAGNIFY_DISABLE			0x00000000
#define	IFB_REG_MAGNIFY_X2			0x00000040
#define	IFB_REG_MAGNIFY_X4			0x00000080
#define	IFB_REG_MAGNIFY_X8			0x000000c0
#define	IFB_REG_MAGNIFY_WINDIV2			0x00000000
#define	IFB_REG_MAGNIFY_WINDIV4			0x00000010
#define	IFB_REG_MAGNIFY_WINDIV8			0x00000020
#define	IFB_REG_MAGNIFY_WINDIV16		0x00000030

/*
 * 0070 display resolution
 * Contains the size of the display, as ((height - 1) << 16) | (width - 1)
 */
#define	IFB_REG_RESOLUTION		0x0070
/*
 * 0074 configuration register
 * Contains 0x1a000088 | ((Log2 stride) << 16)
 */
#define	IFB_REG_CONFIG			0x0074
/*
 * 0078 32bit frame buffer window #0 (8 to 9 MB)
 * Contains the offset (relative to BAR0) of the 32 bit frame buffer window.
 */
#define	IFB_REG_FB32_0			0x0078
/*
 * 007c 32bit frame buffer window #1 (8 to 9 MB)
 * Contains the offset (relative to BAR0) of the 32 bit frame buffer window.
 */
#define	IFB_REG_FB32_1			0x007c
/*
 * 0080 8bit frame buffer window #0 (2 to 2.2 MB)
 * Contains the offset (relative to BAR0) of the 8 bit frame buffer window.
 */
#define	IFB_REG_FB8_0			0x0080
/*
 * 0084 8bit frame buffer window #1 (2 to 2.2 MB)
 * Contains the offset (relative to BAR0) of the 8 bit frame buffer window.
 */
#define	IFB_REG_FB8_1			0x0084
/*
 * 0088 unknown window (as large as a 32 bit frame buffer)
 */
#define	IFB_REG_FB_UNK0			0x0088
/*
 * 008c unknown window (as large as a 8 bit frame buffer)
 */
#define	IFB_REG_FB_UNK1			0x008c
/*
 * 0090 unknown window (as large as a 8 bit frame buffer)
 */
#define	IFB_REG_FB_UNK2			0x0090

/*
 * 00bc RAMDAC palette index register
 */
#define	IFB_REG_CMAP_INDEX		0x00bc
/*
 * 00c0 RAMDAC palette data register
 */
#define	IFB_REG_CMAP_DATA		0x00c0

/*
 * 00e4 DPMS state register
 * States ``off'' and ``suspend'' need chip reprogramming before video can
 * be enabled again.
 */
#define	IFB_REG_DPMS_STATE		0x00e4
#define	IFB_REG_DPMS_OFF			0x00000000
#define	IFB_REG_DPMS_SUSPEND			0x00000001
#define	IFB_REG_DPMS_STANDBY			0x00000002
#define	IFB_REG_DPMS_ON				0x00000003

struct ifb_softc {
	struct sunfb sc_sunfb;
	int sc_nscreens;

	bus_space_tag_t sc_mem_t;
	pcitag_t sc_pcitag;

	bus_space_handle_t sc_mem_h;
	bus_addr_t sc_membase;
	bus_size_t sc_memlen;
	vaddr_t	sc_memvaddr, sc_fb8bank0_vaddr, sc_fb8bank1_vaddr;
	bus_space_handle_t sc_reg_h;

	struct wsdisplay_emulops sc_old_ops;
	void (*sc_old_cursor)(struct rasops_info *);

	int sc_console;
	u_int8_t sc_cmap_red[256];
	u_int8_t sc_cmap_green[256];
	u_int8_t sc_cmap_blue[256];
};

int	ifb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	ifb_mmap(void *, off_t, int);
int	ifb_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	ifb_free_screen(void *, void *);
int	ifb_show_screen(void *, void *, int,
	    void (*cb)(void *, int, int), void *);
void	ifb_burner(void *, u_int, u_int);

struct wsdisplay_accessops ifb_accessops = {
	ifb_ioctl,
	ifb_mmap,
	ifb_alloc_screen,
	ifb_free_screen,
	ifb_show_screen,
	NULL,
	NULL,
	NULL,
	ifb_burner
};

int	ifbmatch(struct device *, void *, void *);
void	ifbattach(struct device *, struct device *, void *);

struct cfattach ifb_ca = {
	sizeof (struct ifb_softc), ifbmatch, ifbattach
};

struct cfdriver ifb_cd = {
	NULL, "ifb", DV_DULL
};

int	ifb_mapregs(struct ifb_softc *, struct pci_attach_args *);
int	ifb_is_console(int);
int	ifb_getcmap(struct ifb_softc *, struct wsdisplay_cmap *);
int	ifb_putcmap(struct ifb_softc *, struct wsdisplay_cmap *);
void	ifb_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);

void	ifb_putchar(void *, int, int, u_int, long);
void	ifb_copycols(void *, int, int, int, int);
void	ifb_erasecols(void *, int, int, int, long);
void	ifb_copyrows(void *, int, int, int);
void	ifb_eraserows(void *, int, int, long);
void	ifb_do_cursor(struct rasops_info *);

const struct pci_matchid ifb_devices[] = {
    { PCI_VENDOR_INTERGRAPH, PCI_PRODUCT_INTERGRAPH_EXPERT3D },
    { PCI_VENDOR_3DLABS,     PCI_PRODUCT_3DLABS_WILDCAT_6210 },
    { PCI_VENDOR_3DLABS,     PCI_PRODUCT_3DLABS_WILDCAT_5110 },/* Sun XVR-500 */
    { PCI_VENDOR_3DLABS,     PCI_PRODUCT_3DLABS_WILDCAT_7210 },
};

int
ifbmatch(struct device *parent, void *cf, void *aux)
{
	struct pci_attach_args *paa = aux;
	int node;
	char *name;

	if (pci_matchbyid(paa, ifb_devices,
	    sizeof(ifb_devices) / sizeof(ifb_devices[0])) != 0)
		return 1;

	node = PCITAG_NODE(paa->pa_tag);
	name = getpropstring(node, "name");
	if (strcmp(name, "SUNW,Expert3D") == 0 ||
	    strcmp(name, "SUNW,Expert3D-Lite") == 0)
		return 1;

	return 0;
}

void    
ifbattach(struct device *parent, struct device *self, void *aux)
{
	struct ifb_softc *sc = (struct ifb_softc *)self;
	struct pci_attach_args *paa = aux;
	struct rasops_info *ri;
	int node;

	sc->sc_mem_t = paa->pa_memt;
	sc->sc_pcitag = paa->pa_tag;

	printf("\n");

	if (ifb_mapregs(sc, paa))
		return;

	sc->sc_memvaddr = (vaddr_t)bus_space_vaddr(sc->sc_mem_t, sc->sc_mem_h);
	sc->sc_fb8bank0_vaddr = sc->sc_memvaddr +
	    bus_space_read_4(sc->sc_mem_t, sc->sc_reg_h,
	      IFB_REG_OFFSET + IFB_REG_FB8_0) - sc->sc_membase;
	sc->sc_fb8bank1_vaddr = sc->sc_memvaddr +
	    bus_space_read_4(sc->sc_mem_t, sc->sc_reg_h,
	      IFB_REG_OFFSET + IFB_REG_FB8_1) - sc->sc_membase;

	node = PCITAG_NODE(paa->pa_tag);
	sc->sc_console = ifb_is_console(node);

	fb_setsize(&sc->sc_sunfb, 8, 1152, 900, node, 0);

	printf("%s: %dx%d\n",
	    self->dv_xname, sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

#if 0
	/*
	 * Make sure the frame buffer is configured to sane values.
	 * So much more is needed there... documentation permitting.
	 */
	bus_space_write_4(sc->sc_mem_t, sc->sc_reg_h,
	    IFB_REG_OFFSET + IFB_REG_COMPONENT_SELECT, 0x00000101);
	delay(1000);
	bus_space_write_4(sc->sc_mem_t, sc->sc_reg_h,
	    IFB_REG_OFFSET + IFB_REG_MAGNIFY, IFB_REG_MAGNIFY_DISABLE);
#endif

	ri = &sc->sc_sunfb.sf_ro;
	ri->ri_bits = NULL;
	ri->ri_hw = sc;

	/* do NOT pass RI_CLEAR yet */
	fbwscons_init(&sc->sc_sunfb, RI_BSWAP);
	ri->ri_flg &= ~RI_FULLCLEAR;	/* due to the way we handle updates */

	if (!sc->sc_console) {
		bzero((void *)sc->sc_fb8bank0_vaddr, sc->sc_sunfb.sf_fbsize);
		bzero((void *)sc->sc_fb8bank1_vaddr, sc->sc_sunfb.sf_fbsize);
	}

	/* pick centering delta */
	sc->sc_fb8bank0_vaddr += ri->ri_bits - ri->ri_origbits;
	sc->sc_fb8bank1_vaddr += ri->ri_bits - ri->ri_origbits;

	sc->sc_old_ops = ri->ri_ops;	/* structure copy */
	sc->sc_old_cursor = ri->ri_do_cursor;
	ri->ri_ops.copyrows = ifb_copyrows;
	ri->ri_ops.copycols = ifb_copycols;
	ri->ri_ops.eraserows = ifb_eraserows;
	ri->ri_ops.erasecols = ifb_erasecols;
	ri->ri_ops.putchar = ifb_putchar;
	ri->ri_do_cursor = ifb_do_cursor;

	fbwscons_setcolormap(&sc->sc_sunfb, ifb_setcolor);
	if (sc->sc_console)
		fbwscons_console_init(&sc->sc_sunfb, -1);
	fbwscons_attach(&sc->sc_sunfb, &ifb_accessops, sc->sc_console);
}

int
ifb_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct ifb_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	struct pcisel *sel;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
		break;

	case WSDISPLAYIO_SMODE:
		if (*(u_int *)data == WSDISPLAYIO_MODE_EMUL)
			fbwscons_setcolormap(&sc->sc_sunfb, ifb_setcolor);
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width  = sc->sc_sunfb.sf_width;
		wdf->depth  = sc->sc_sunfb.sf_depth;
		wdf->cmsize = 256;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;
		
	case WSDISPLAYIO_GETCMAP:
		return ifb_getcmap(sc, (struct wsdisplay_cmap *)data);
	case WSDISPLAYIO_PUTCMAP:
		return ifb_putcmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_GPCIID:
		sel = (struct pcisel *)data;
		sel->pc_bus = PCITAG_BUS(sc->sc_pcitag);
		sel->pc_dev = PCITAG_DEV(sc->sc_pcitag);
		sel->pc_func = PCITAG_FUN(sc->sc_pcitag);
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
		return -1; /* not supported yet */
        }

	return 0;
}

int
ifb_getcmap(struct ifb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error;

	if (index >= 256 || count > 256 - index)
		return EINVAL;

	error = copyout(&sc->sc_cmap_red[index], cm->red, count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_green[index], cm->green, count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_blue[index], cm->blue, count);
	if (error)
		return error;
	return 0;
}

int
ifb_putcmap(struct ifb_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	u_int i;
	int error;
	u_char *r, *g, *b;

	if (index >= 256 || count > 256 - index)
		return EINVAL;

	if ((error = copyin(cm->red, &sc->sc_cmap_red[index], count)) != 0)
		return error;
	if ((error = copyin(cm->green, &sc->sc_cmap_green[index], count)) != 0)
		return error;
	if ((error = copyin(cm->blue, &sc->sc_cmap_blue[index], count)) != 0)
		return error;

	r = &sc->sc_cmap_red[index];
	g = &sc->sc_cmap_green[index];
	b = &sc->sc_cmap_blue[index];

	for (i = 0; i < count; i++) {
		bus_space_write_4(sc->sc_mem_t, sc->sc_reg_h,
		    IFB_REG_OFFSET + IFB_REG_CMAP_INDEX, index);
		bus_space_write_4(sc->sc_mem_t, sc->sc_reg_h,
		    IFB_REG_OFFSET + IFB_REG_CMAP_DATA,
		    (((u_int)*b) << 22) | (((u_int)*g) << 12) | (((u_int)*r) << 2));
		r++, g++, b++, index++;
	}
	return 0;
}

void
ifb_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct ifb_softc *sc = v;

	sc->sc_cmap_red[index] = r;
	sc->sc_cmap_green[index] = g;
	sc->sc_cmap_blue[index] = b;

	bus_space_write_4(sc->sc_mem_t, sc->sc_reg_h,
	    IFB_REG_OFFSET + IFB_REG_CMAP_INDEX, index);
	bus_space_write_4(sc->sc_mem_t, sc->sc_reg_h,
	    IFB_REG_OFFSET + IFB_REG_CMAP_DATA,
	    (((u_int)b) << 22) | (((u_int)g) << 12) | (((u_int)r) << 2));
}

int
ifb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct ifb_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return ENOMEM;

	*cookiep = &sc->sc_sunfb.sf_ro;
	*curyp = 0;
	*curxp = 0;
	sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
	    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, attrp);
	sc->sc_nscreens++;
	return 0;
}

void
ifb_free_screen(void *v, void *cookie)
{
	struct ifb_softc *sc = v;

	sc->sc_nscreens--;
}

int
ifb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return 0;
}

paddr_t
ifb_mmap(void *v, off_t off, int prot)
{
	return -1;
}

void
ifb_burner(void *v, u_int on, u_int flags)
{
	struct ifb_softc *sc = (struct ifb_softc *)v;
	int s;
	uint32_t dpms;

	s = splhigh();
	if (on)
		dpms = IFB_REG_DPMS_ON;
	else {
#ifdef notyet
		if (flags & WSDISPLAY_BURN_VBLANK)
			dpms = IFB_REG_DPMS_SUSPEND;
		else
#endif
			dpms = IFB_REG_DPMS_STANDBY;
	}
	bus_space_write_4(sc->sc_mem_t, sc->sc_reg_h,
	    IFB_REG_OFFSET + IFB_REG_DPMS_STATE, dpms);
	splx(s);
}

int
ifb_is_console(int node)
{
	extern int fbnode;

	return fbnode == node;
}

int
ifb_mapregs(struct ifb_softc *sc, struct pci_attach_args *pa)
{
	u_int32_t cf;
	int rc;

	cf = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	if (PCI_MAPREG_TYPE(cf) == PCI_MAPREG_TYPE_IO)
		rc = EINVAL;
	else {
		rc = pci_mapreg_map(pa, PCI_MAPREG_START, cf,
		    BUS_SPACE_MAP_LINEAR, NULL, &sc->sc_mem_h,
		    &sc->sc_membase, &sc->sc_memlen, 0);
	}
	if (rc != 0) {
		printf("%s: can't map video memory\n",
		    sc->sc_sunfb.sf_dev.dv_xname);
		return rc;
	}

	cf = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START + 4);
	if (PCI_MAPREG_TYPE(cf) == PCI_MAPREG_TYPE_IO)
		rc = EINVAL;
	else {
		rc = pci_mapreg_map(pa, PCI_MAPREG_START + 4, cf,
		    0, NULL, &sc->sc_reg_h, NULL, NULL, 0x9000);
	}
	if (rc != 0) {
		printf("%s: can't map register space\n",
		    sc->sc_sunfb.sf_dev.dv_xname);
		return rc;
	}

	return 0;
}

void
ifb_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct ifb_softc *sc = ri->ri_hw;

	ri->ri_bits = (void *)sc->sc_fb8bank0_vaddr;
	sc->sc_old_ops.putchar(cookie, row, col, uc, attr);
	ri->ri_bits = (void *)sc->sc_fb8bank1_vaddr;
	sc->sc_old_ops.putchar(cookie, row, col, uc, attr);
}

void
ifb_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct ifb_softc *sc = ri->ri_hw;

	ri->ri_bits = (void *)sc->sc_fb8bank0_vaddr;
	sc->sc_old_ops.copycols(cookie, row, src, dst, num);
	ri->ri_bits = (void *)sc->sc_fb8bank1_vaddr;
	sc->sc_old_ops.copycols(cookie, row, src, dst, num);
}

void
ifb_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct ifb_softc *sc = ri->ri_hw;

	ri->ri_bits = (void *)sc->sc_fb8bank0_vaddr;
	sc->sc_old_ops.erasecols(cookie, row, col, num, attr);
	ri->ri_bits = (void *)sc->sc_fb8bank1_vaddr;
	sc->sc_old_ops.erasecols(cookie, row, col, num, attr);
}

void
ifb_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct ifb_softc *sc = ri->ri_hw;

	ri->ri_bits = (void *)sc->sc_fb8bank0_vaddr;
	sc->sc_old_ops.copyrows(cookie, src, dst, num);
	ri->ri_bits = (void *)sc->sc_fb8bank1_vaddr;
	sc->sc_old_ops.copyrows(cookie, src, dst, num);
}

void
ifb_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct ifb_softc *sc = ri->ri_hw;

	ri->ri_bits = (void *)sc->sc_fb8bank0_vaddr;
	sc->sc_old_ops.eraserows(cookie, row, num, attr);
	ri->ri_bits = (void *)sc->sc_fb8bank1_vaddr;
	sc->sc_old_ops.eraserows(cookie, row, num, attr);
}

void
ifb_do_cursor(struct rasops_info *ri)
{
	struct ifb_softc *sc = ri->ri_hw;

	ri->ri_bits = (void *)sc->sc_fb8bank0_vaddr;
	sc->sc_old_cursor(ri);
	ri->ri_bits = (void *)sc->sc_fb8bank1_vaddr;
	sc->sc_old_cursor(ri);
}
