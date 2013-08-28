/*	$OpenBSD: vgafb.c,v 1.55 2013/08/28 20:47:10 mpi Exp $	*/
/*	$NetBSD: vga.c,v 1.3 1996/12/02 22:24:54 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <dev/ofw/openfirm.h>
#include <macppc/macppc/ofw_machdep.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/vga_pcivar.h>

struct vga_config {
	bus_space_tag_t		vc_memt;
	bus_space_handle_t	vc_memh;

	struct rasops_info	ri;
	uint8_t			cmap[256 * 3];

	bus_addr_t	iobase, membase, mmiobase;
	bus_size_t	iosize, memsize, mmiosize;

	struct	wsscreen_descr	vc_wsd;
	struct	wsscreen_list	vc_wsl;
	struct	wsscreen_descr *vc_scrlist[1];

	int vc_backlight_on;
	u_int vc_mode;
};

int	vgafb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	vgafb_mmap(void *, off_t, int);
int	vgafb_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	vgafb_free_screen(void *, void *);
int	vgafb_show_screen(void *, void *, int, void (*cb)(void *, int, int),
	    void *);
void	vgafb_burn(void *v, u_int , u_int);
void	vgafb_restore_default_colors(struct vga_config *);
int	vgafb_is_console(int);
int	vgafb_mapregs(struct vga_config *, struct pci_attach_args *);

struct vga_config vgafbcn;

struct wsdisplay_accessops vgafb_accessops = {
	vgafb_ioctl,
	vgafb_mmap,
	vgafb_alloc_screen,
	vgafb_free_screen,
	vgafb_show_screen,
	NULL,		/* load_font */
	NULL,		/* scrollback */
	NULL,		/* getchar */
	vgafb_burn,	/* burner */
};

int	vgafb_getcmap(uint8_t *, struct wsdisplay_cmap *);
int	vgafb_putcmap(uint8_t *, struct wsdisplay_cmap *);

int	vgafb_match(struct device *, void *, void *);
void	vgafb_attach(struct device *, struct device *, void *);

const struct cfattach vgafb_ca = {
	sizeof(struct device), vgafb_match, vgafb_attach,
};

struct cfdriver vgafb_cd = {
	NULL, "vgafb", DV_DULL,
};

#ifdef APERTURE
extern int allowaperture;
#endif

int
vgafb_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	int node;

	if (DEVICE_IS_VGA_PCI(pa->pa_class) == 0) {
		/*
		 * XXX Graphic cards found in iMac G3 have a ``Misc''
		 * subclass, match them all.
		 */
		if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
		    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_MISC)
			return (0);
	}

	/*
	 * XXX Non-console devices do not get configured by the PROM,
	 * XXX so do not attach them yet.
	 */
	node = PCITAG_NODE(pa->pa_tag);
	if (!vgafb_is_console(node))
		return (0);

	return (1);
}

void
vgafb_attach(struct device *parent, struct device  *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct vga_config *vc = &vgafbcn;
	struct wsemuldisplaydev_attach_args waa;
	struct rasops_info *ri;
	long defattr;

	if (vgafb_mapregs(vc, pa))
		return;

	ri = &vc->ri;
	ri->ri_flg = RI_CENTER | RI_VCONS | RI_WRONLY;
	ri->ri_hw = vc;

	ofwconsswitch(ri);

	rasops_init(ri, 160, 160);

	strlcpy(vc->vc_wsd.name, "std", sizeof(vc->vc_wsd.name));
	vc->vc_wsd.capabilities = ri->ri_caps;
	vc->vc_wsd.nrows = ri->ri_rows;
	vc->vc_wsd.ncols = ri->ri_cols;
	vc->vc_wsd.textops = &ri->ri_ops;
	vc->vc_wsd.fontwidth = ri->ri_font->fontwidth;
	vc->vc_wsd.fontheight = ri->ri_font->fontheight;

	ri->ri_ops.alloc_attr(ri->ri_active, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&vc->vc_wsd, ri->ri_active, ri->ri_ccol, ri->ri_crow,
	    defattr);

	vc->vc_scrlist[0] = &vc->vc_wsd;
	vc->vc_wsl.nscreens = 1;
	vc->vc_wsl.screens = (const struct wsscreen_descr **)vc->vc_scrlist;

	waa.console = 1;
	waa.scrdata = &vc->vc_wsl;
	waa.accessops = &vgafb_accessops;
	waa.accesscookie = vc;
	waa.defaultscreens = 0;

	/* no need to keep the burner function if no hw support */
	if (cons_backlight_available == 0)
		vgafb_accessops.burn_screen = NULL;
	else {
		vc->vc_backlight_on = WSDISPLAYIO_VIDEO_OFF;
		vgafb_burn(vc, WSDISPLAYIO_VIDEO_ON, 0);	/* paranoia */
	}

	config_found(self, &waa, wsemuldisplaydevprint);
}

void
vgafb_restore_default_colors(struct vga_config *vc)
{
	bcopy(rasops_cmap, vc->cmap, sizeof(vc->cmap));
	of_setcolors(vc->cmap, 0, 256);
}

int
vgafb_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct vga_config *vc = v;
	struct rasops_info *ri = &vc->ri;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int rc;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIVGA;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 256;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(uint *)data = ri->ri_stride;
		break;
	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		rc = vgafb_getcmap(vc->cmap, cm);
		if (rc != 0)
			return rc;
		break;
	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		rc = vgafb_putcmap(vc->cmap, cm);
		if (rc != 0)
			return (rc);
		if (ri->ri_depth == 8)
			of_setcolors(vc->cmap, cm->index, cm->count);
		break;
	case WSDISPLAYIO_SMODE:
		vc->vc_mode = *(u_int *)data;
		if (ri->ri_depth == 8)
			vgafb_restore_default_colors(vc);
		break;
	case WSDISPLAYIO_GETPARAM:
	{
		struct wsdisplay_param *dp = (struct wsdisplay_param *)data;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			if (cons_backlight_available != 0) {
				dp->min = MIN_BRIGHTNESS;
				dp->max = MAX_BRIGHTNESS;
				dp->curval = cons_brightness;
				return 0;
			}
			return -1;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			if (cons_backlight_available != 0) {
				dp->min = 0;
				dp->max = 1;
				dp->curval = vc->vc_backlight_on;
				return 0;
			} else
				return -1;
		}
	}
		return -1;

	case WSDISPLAYIO_SETPARAM:
	{
		struct wsdisplay_param *dp = (struct wsdisplay_param *)data;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			if (cons_backlight_available == 1) {
				of_setbrightness(dp->curval);
				return 0;
			} else
				return -1;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			if (cons_backlight_available != 0) {
				vgafb_burn(vc,
				    dp->curval ? WSDISPLAYIO_VIDEO_ON :
				      WSDISPLAYIO_VIDEO_OFF, 0);
				return 0;
			} else
				return -1;
		}
	}
		return -1;

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

	return (0);
}

paddr_t
vgafb_mmap(void *v, off_t off, int prot)
{
	struct vga_config *vc = v;

	if (off & PGOFSET)
		return (-1);

	switch (vc->vc_mode) {
	case WSDISPLAYIO_MODE_MAPPED:
#ifdef APERTURE
		if (allowaperture == 0)
			return (-1);
#endif

		if (vc->mmiosize == 0)
			return (-1);

		if (off >= vc->membase && off < (vc->membase + vc->memsize))
			return (off);

		if (off >= vc->mmiobase && off < (vc->mmiobase + vc->mmiosize))
			return (off);
		break;

	case WSDISPLAYIO_MODE_DUMBFB:
		if (off >= 0x00000 && off < vc->memsize)
			return (vc->membase + off);
		break;

	}

	return (-1);
}

int
vgafb_is_console(int node)
{
	extern int fbnode;

	return (fbnode == node);
}

int
vgafb_getcmap(uint8_t *cmap, struct wsdisplay_cmap *cm)
{
	uint index = cm->index, count = cm->count, i;
	uint8_t ramp[256], *dst, *src;
	int rc;

	if (index >= 256 || count > 256 - index)
		return EINVAL;

	index *= 3;

	src = cmap + index;
	dst = ramp;
	for (i = 0; i < count; i++)
		*dst++ = *src, src += 3;
	rc = copyout(ramp, cm->red, count);
	if (rc != 0)
		return rc;

	src = cmap + index + 1;
	dst = ramp;
	for (i = 0; i < count; i++)
		*dst++ = *src, src += 3;
	rc = copyout(ramp, cm->green, count);
	if (rc != 0)
		return rc;

	src = cmap + index + 2;
	dst = ramp;
	for (i = 0; i < count; i++)
		*dst++ = *src, src += 3;
	rc = copyout(ramp, cm->blue, count);
	if (rc != 0)
		return rc;

	return 0;
}

int
vgafb_putcmap(uint8_t *cmap, struct wsdisplay_cmap *cm)
{
	uint index = cm->index, count = cm->count, i;
	uint8_t ramp[256], *dst, *src;
	int rc;

	if (index >= 256 || count > 256 - index)
		return EINVAL;

	index *= 3;

	rc = copyin(cm->red, ramp, count);
	if (rc != 0)
		return rc;
	dst = cmap + index;
	src = ramp;
	for (i = 0; i < count; i++)
		*dst = *src++, dst += 3;

	rc = copyin(cm->green, ramp, count);
	if (rc != 0)
		return rc;
	dst = cmap + index + 1;
	src = ramp;
	for (i = 0; i < count; i++)
		*dst = *src++, dst += 3;

	rc = copyin(cm->blue, ramp, count);
	if (rc != 0)
		return rc;
	dst = cmap + index + 2;
	src = ramp;
	for (i = 0; i < count; i++)
		*dst = *src++, dst += 3;

	return 0;
}

void
vgafb_burn(void *v, u_int on, u_int flags)
{
	struct vga_config *vc = v;

	if (vc->vc_backlight_on != on) {
		of_setbacklight(on == WSDISPLAYIO_VIDEO_ON);
		vc->vc_backlight_on = on;
	}
}

int
vgafb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct vga_config *vc = v;
	struct rasops_info *ri = &vc->ri;

	return rasops_alloc_screen(ri, cookiep, curxp, curyp, attrp);
}

void
vgafb_free_screen(void *v, void *cookie)
{
	struct vga_config *vc = v;
	struct rasops_info *ri = &vc->ri;

	return rasops_free_screen(ri, cookie);
}

int
vgafb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct vga_config *vc = v;
	struct rasops_info *ri = &vc->ri;

	if (cookie == ri->ri_active)
		return (0);

	return rasops_show_screen(ri, cookie, waitok, cb, cbarg);
}

int
vgafb_mapregs(struct vga_config *vc, struct pci_attach_args *pa)
{
	bus_addr_t ba;
	bus_size_t bs;
	int hasio = 0, hasmem = 0, hasmmio = 0;
	uint32_t i, cf;
	int rv;

	for (i = PCI_MAPREG_START; i <= PCI_MAPREG_PPB_END; i += 4) {
		cf = pci_conf_read(pa->pa_pc, pa->pa_tag, i);
		if (PCI_MAPREG_TYPE(cf) == PCI_MAPREG_TYPE_IO) {
			if (hasio)
				continue;
			rv = pci_io_find(pa->pa_pc, pa->pa_tag, i,
			    &vc->iobase, &vc->iosize);
			if (rv != 0) {
#if notyet
				if (rv != ENOENT)
					printf("%s: failed to find io at 0x%x\n",
					    DEVNAME(sc), i);
#endif
				continue;
			}
			hasio = 1;
		} else {
			/* Memory mapping... frame memory or mmio? */
			rv = pci_mem_find(pa->pa_pc, pa->pa_tag, i,
			    &ba, &bs, NULL);
			if (rv != 0) {
#if notyet
				if (rv != ENOENT)
					printf("%s: failed to find mem at 0x%x\n",
					    DEVNAME(sc), i);
#endif
				continue;
			}

			if (bs == 0 /* || ba == 0 */) {
				/* ignore this entry */
			} else if (hasmem == 0) {
				/*
				 * first memory slot found goes into memory,
				 * this is for the case of no mmio
				 */
				vc->membase = ba;
				vc->memsize = bs;
				hasmem = 1;
			} else {
				/*
				 * Oh, we have a second `memory'
				 * region, is this region the vga memory
				 * or mmio, we guess that memory is
				 * the larger of the two.
				 */
				if (vc->memsize >= bs) {
					/* this is the mmio */
					vc->mmiobase = ba;
					vc->mmiosize = bs;
					hasmmio = 1;
				} else {
					/* this is the memory */
					vc->mmiobase = vc->membase;
					vc->mmiosize = vc->memsize;
					vc->membase = ba;
					vc->memsize = bs;
				}
			}
		}
	}

	/* failure to initialize io ports should not prevent attachment */
	if (hasmem == 0) {
		printf(": could not find memory space\n");
		return (1);
	}

	if (hasmmio)
		printf (", mmio");
	printf("\n");

	return (0);
}
