/* $OpenBSD: vga_pci.c,v 1.67 2011/04/14 21:04:29 oga Exp $ */
/* $NetBSD: vga_pci.c,v 1.3 1998/06/08 06:55:58 thorpej Exp $ */

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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

#include "vga.h"
#if defined(__i386__) || defined(__amd64__)
#include "acpi.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/agpio.h>

#include <uvm/uvm.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/agpvar.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>


#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#ifdef X86EMU
#include <machine/vga_post.h>
#endif

#ifdef VESAFB
#include <dev/vesa/vesabiosvar.h>
#endif

#include "intagp.h"
#include "drm.h"

int	vga_pci_match(struct device *, void *, void *);
void	vga_pci_attach(struct device *, struct device *, void *);
int	vga_pci_activate(struct device *, int);
paddr_t	vga_pci_mmap(void* v, off_t off, int prot);
void	vga_pci_bar_init(struct vga_pci_softc *, struct pci_attach_args *);

#if NINTAGP > 0
int	intagpsubmatch(struct device *, void *, void *);
int	intagp_print(void *, const char *);
#endif 
#if NDRM > 0
int	drmsubmatch(struct device *, void *, void *);
int	vga_drm_print(void *, const char *);
#endif

#ifdef VESAFB
int	vesafb_putcmap(struct vga_pci_softc *, struct wsdisplay_cmap *);
int	vesafb_getcmap(struct vga_pci_softc *, struct wsdisplay_cmap *);
#endif

#if !defined(SMALL_KERNEL) && NACPI > 0
void	vga_save_state(struct vga_pci_softc *);
void	vga_restore_state(struct vga_pci_softc *);
#endif


/*
 * Function pointers for wsconsctl parameter handling.
 * XXX These should be per-softc, but right now we only attach
 * XXX a single vga@pci instance, so this will do.
 */
int	(*ws_get_param)(struct wsdisplay_param *);
int	(*ws_set_param)(struct wsdisplay_param *);
    

struct cfattach vga_pci_ca = {
	sizeof(struct vga_pci_softc), vga_pci_match, vga_pci_attach,
	NULL, vga_pci_activate
};

#if !defined(SMALL_KERNEL) && NACPI > 0
int vga_pci_do_post;
extern int do_real_mode_post;

struct vga_device_description {
	u_int16_t	rval[4];
	u_int16_t	rmask[4];
	char	vga_pci_post;
	char	real_mode_post;
};

static const struct vga_device_description vga_devs[] = {
	/*
	 * Header description:
	 *
	 * First entry is a list of the pci video information in the following
	 * order: VENDOR, PRODUCT, SUBVENDOR, SUBPRODUCT
	 *
	 * The next entry is a list of corresponding masks.
	 *
	 * Finally the last two values set what resume should do, repost with
	 * vga_pci (i.e. the x86emulator) or with a locore call to the video
	 * bios.
	 */
	{	/* All machines with Intel US15W (until more evidence) */
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_US15W_IGD,
	    	0x0000, 0x0000 },
	    {	0xffff, 0xffff, 0x0000, 0x0000 }, 1, 0
	},
	{	/* All machines with Intel US15L (until more evidence) */
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_US15L_IGD,
	    	0x0000, 0x0000 },
	    {	0xffff, 0xffff, 0x0000, 0x0000 }, 1, 0
	},

	{	/*  Anything with on-die intel graphics, for now */
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ARRANDALE_IGD,	
	    	0x0000, 0x0000 },
	    {	0xffff, 0xffff, 0x0000, 0x0000 }, 1, 0
	},

	{	/*  Anything with on-die intel graphics, for now */
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CLARKDALE_IGD,
	    	0x0000, 0x0000 },
	    {	0xffff, 0xffff, 0x0000, 0x0000 }, 1, 0
	},

	{	/* All ATI video until further notice */
	    {	PCI_VENDOR_ATI, 0x0000,
		0x0000, 0x0000 },
	    {	0xffff, 0x0000, 0x0000, 0x0000}, 1, 0
	},
};
#endif

int
vga_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (DEVICE_IS_VGA_PCI(pa->pa_class) == 0)
		return (0);

	/* check whether it is disabled by firmware */
	if ((pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    != (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
		return (0);

	/* If it's the console, we have a winner! */
	if (vga_is_console(pa->pa_iot, WSDISPLAY_TYPE_PCIVGA))
		return (1);

	/*
	 * If we might match, make sure that the card actually looks OK.
	 */
	if (!vga_common_probe(pa->pa_iot, pa->pa_memt))
		return (0);

	return (1);
}

void
vga_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	pcireg_t reg;
	struct vga_pci_softc *sc = (struct vga_pci_softc *)self;

#if !defined(SMALL_KERNEL) && NACPI > 0
	int prod, vend, subid, subprod, subvend, i;
#endif

	/*
	 * Enable bus master; X might need this for accelerated graphics.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

#ifdef VESAFB
	if (vesabios_softc != NULL && vesabios_softc->sc_nmodes > 0) {
		sc->sc_textmode = vesafb_get_mode(sc);
		printf(", vesafb\n");
		sc->sc_vc = vga_extended_attach(self, pa->pa_iot, pa->pa_memt,
		    WSDISPLAY_TYPE_PCIVGA, vga_pci_mmap);
		return;
	}
#endif
	printf("\n");
	sc->sc_vc = vga_common_attach(self, pa->pa_iot, pa->pa_memt,
	    WSDISPLAY_TYPE_PCIVGA);

	vga_pci_bar_init(sc, pa);

#if !defined(SMALL_KERNEL) && NACPI > 0

#ifdef X86EMU
	if ((sc->sc_posth = vga_post_init(pa->pa_bus, pa->pa_device,
	    pa->pa_function)) == NULL)
		printf("couldn't set up vga POST handler\n");
#endif

	vend = PCI_VENDOR(pa->pa_id);
	prod = PCI_PRODUCT(pa->pa_id);
	subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	subvend = PCI_VENDOR(subid);
	subprod = PCI_PRODUCT(subid);

	for (i = 0; i < nitems(vga_devs); i++)
		if ((vend & vga_devs[i].rmask[0]) == vga_devs[i].rval[0] &&
		    (prod & vga_devs[i].rmask[1]) == vga_devs[i].rval[1] &&
		    (subvend & vga_devs[i].rmask[2]) == vga_devs[i].rval[2] &&
		    (subprod & vga_devs[i].rmask[3]) == vga_devs[i].rval[3]) {
			vga_pci_do_post = vga_devs[i].vga_pci_post;
			if (sc->sc_dev.dv_unit == 0)	/* main screen only */
				do_real_mode_post = vga_devs[i].real_mode_post;
			break;
		}
#endif

#if NINTAGP > 0
	/*
	 * attach intagp here instead of pchb so it can share mappings
	 * with the DRM.
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL) {
		config_found_sm(self, aux, intagp_print, intagpsubmatch);

	}
#endif

#if NDRM > 0
	config_found_sm(self, aux, NULL, drmsubmatch);
#endif
}

int
vga_pci_activate(struct device *self, int act)
{
	int rv = 0;

#if !defined(SMALL_KERNEL) && NACPI > 0
	struct vga_pci_softc *sc = (struct vga_pci_softc *)self;
#endif

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_activate_children(self, act);
		break;
	case DVACT_SUSPEND:
		rv = config_activate_children(self, act);
#if !defined(SMALL_KERNEL) && NACPI > 0
		/*
		 * Save the common vga state. This should theoretically only
		 * be necessary if we intend to POST, but it is preferrable
		 * to do it unconditionnaly, as many systems do not restore
		 * this state correctly upon resume.
		 */
		vga_save_state(sc);
#endif
		break;
	case DVACT_RESUME:
#if !defined(SMALL_KERNEL) && NACPI > 0
#if defined (X86EMU)
		if (vga_pci_do_post) {
#ifdef obnoxious
			printf("%s: reposting video using BIOS.  Is this necessary?\n",
			    sc->sc_dev.dv_xname);
#endif
			vga_post_call(sc->sc_posth);
		}
#endif
		vga_restore_state(sc);
#endif
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}

#if NINTAGP > 0
int
intagpsubmatch(struct device *parent, void *match, void *aux)
{
	extern struct cfdriver intagp_cd;
	struct cfdata *cf = match;

	/* only allow intagp to attach */
	if (cf->cf_driver == &intagp_cd)
		return ((*cf->cf_attach->ca_match)(parent, match, aux));
	return (0);
}

int
intagp_print(void *vaa, const char *pnp)
{
	if (pnp)
		printf("intagp at %s", pnp);
	return (UNCONF);
}
#endif

#if NDRM > 0
int
drmsubmatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct cfdriver *cd;
	size_t len = 0;
	char *sm;

	cd = cf->cf_driver;

	/* is this a *drm device? */
	len = strlen(cd->cd_name);
	sm = cd->cd_name + len - 3;
	if (strncmp(sm, "drm", 3) == 0)
		return ((*cf->cf_attach->ca_match)(parent, match, aux));

	return (0);
}
#endif

paddr_t
vga_pci_mmap(void *v, off_t off, int prot)
{
#ifdef VESAFB
	struct vga_config *vc = (struct vga_config *)v;
	struct vga_pci_softc *sc = (struct vga_pci_softc *)vc->vc_softc;

	if (sc->sc_mode == WSDISPLAYIO_MODE_DUMBFB) {
		if (off < 0 || off > vesabios_softc->sc_size)
			return (-1);
		return (sc->sc_base + off);
	}
#endif
	return -1;
}

int
vga_pci_cnattach(bus_space_tag_t iot, bus_space_tag_t memt,
    pci_chipset_tag_t pc, int bus, int device, int function)
{
	return (vga_cnattach(iot, memt, WSDISPLAY_TYPE_PCIVGA, 0));
}

int
vga_pci_ioctl(void *v, u_long cmd, caddr_t addr, int flag, struct proc *pb)
{
	int error = 0;
#ifdef VESAFB
	struct vga_config *vc = (struct vga_config *)v;
	struct vga_pci_softc *sc = (struct vga_pci_softc *)vc->vc_softc;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_gfx_mode *gfxmode;
	int mode;
#endif

	switch (cmd) {
#ifdef VESAFB
	case WSDISPLAYIO_SMODE:
		mode = *(u_int *)addr;
		switch (mode) {
		case WSDISPLAYIO_MODE_EMUL:
			/* back to text mode */
			vesafb_set_mode(sc, sc->sc_textmode);
			sc->sc_mode = mode;
			break;
		case WSDISPLAYIO_MODE_DUMBFB:
			if (sc->sc_gfxmode == -1)
				return (-1);
			vesafb_set_mode(sc, sc->sc_gfxmode);
			sc->sc_mode = mode;
			break;
		default:
			error = -1;
		}
		break;
	case WSDISPLAYIO_GINFO:
		if (sc->sc_gfxmode == -1)
			return (-1);
		wdf = (void *)addr;
		wdf->height = sc->sc_height;
		wdf->width = sc->sc_width;
		wdf->depth = sc->sc_depth;
		wdf->cmsize = 256;
		break;

	case WSDISPLAYIO_LINEBYTES:
		if (sc->sc_gfxmode == -1)
			return (-1);
		*(u_int *)addr = sc->sc_linebytes;
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
		break;
	case WSDISPLAYIO_GETCMAP:
		if (sc->sc_depth == 8)
			error = vesafb_getcmap(sc,
			    (struct wsdisplay_cmap *)addr);
		break;

	case WSDISPLAYIO_PUTCMAP:
		if (sc->sc_depth == 8)
			error = vesafb_putcmap(sc,
			    (struct wsdisplay_cmap *)addr);
		break;

	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		*(int *)addr = vesafb_get_supported_depth(sc);
		break;
		
	case WSDISPLAYIO_SETGFXMODE:
		gfxmode = (struct wsdisplay_gfx_mode *)addr;
		sc->sc_gfxmode = vesafb_find_mode(sc, gfxmode->width,
		    gfxmode->height, gfxmode->depth);
		if (sc->sc_gfxmode == -1) 
			error = -1;
		break;

#endif
	case WSDISPLAYIO_GETPARAM:
		if (ws_get_param != NULL)
			return (*ws_get_param)((struct wsdisplay_param *)addr);
		else
			error = ENOTTY;
		break;
	case WSDISPLAYIO_SETPARAM:
		if (ws_set_param != NULL)
			return (*ws_set_param)((struct wsdisplay_param *)addr);
		else
			error = ENOTTY;
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}

#ifdef notyet
void
vga_pci_close(void *v)
{
}
#endif

/*
 * Prepare dev->bars to be used for information. we do this at startup
 * so we can do the whole array at once, dealing with 64-bit BARs correctly.
 */
void
vga_pci_bar_init(struct vga_pci_softc *dev, struct pci_attach_args *pa)
{
	pcireg_t type;
	int addr = PCI_MAPREG_START, i = 0;
	memcpy(&dev->pa, pa, sizeof(dev->pa));

	while (i < VGA_PCI_MAX_BARS) {
		dev->bars[i] = malloc(sizeof((*dev->bars[i])), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (dev->bars[i] == NULL) {
			return;
		}

		dev->bars[i]->addr = addr;

		type = dev->bars[i]->maptype = pci_mapreg_type(pa->pa_pc,
		    pa->pa_tag, addr);
		if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, addr,
		    dev->bars[i]->maptype, &dev->bars[i]->base,
		    &dev->bars[i]->maxsize, &dev->bars[i]->flags) != 0) {
			free(dev->bars[i], M_DEVBUF);
			dev->bars[i] = NULL;
		}

		if (type == PCI_MAPREG_MEM_TYPE_64BIT) {
			addr += 8;
			i += 2;
		} else {
			addr += 4;
			i++;
		}
	}
}

/*
 * Get the vga_pci_bar struct for the address in question. returns NULL if
 * invalid BAR is passed.
 */
struct vga_pci_bar*
vga_pci_bar_info(struct vga_pci_softc *dev, int no)
{
	if (dev == NULL || no >= VGA_PCI_MAX_BARS)
		return (NULL);
	return (dev->bars[no]);
}

/*
 * map the BAR in question, returning the vga_pci_bar struct in case any more
 * processing needs to be done. Returns NULL on failure. Can be called multiple
 * times.
 */
struct vga_pci_bar*
vga_pci_bar_map(struct vga_pci_softc *dev, int addr, bus_size_t size,
    int busflags)
{
	struct vga_pci_bar *bar = NULL;
	int i;

	if (dev == NULL) 
		return (NULL);

	for (i = 0; i < VGA_PCI_MAX_BARS; i++) {
		if (dev->bars[i] && dev->bars[i]->addr == addr) {
			bar = dev->bars[i];
			break;
		}
	}
	if (bar == NULL) {
		printf("vga_pci_bar_map: given invalid address 0x%x\n", addr);
		return (NULL);
	}

	if (bar->mapped == 0) {
		if (pci_mapreg_map(&dev->pa, bar->addr, bar->maptype,
		    bar->flags | busflags, &bar->bst, &bar->bsh, NULL,
		    &bar->size, size)) {
			printf("vga_pci_bar_map: can't map bar 0x%x\n", addr);
			return (NULL);
		}
	}

	bar->mapped++;
	return (bar);
}

/*
 * "unmap" the BAR referred to by argument. If more than one place has mapped it
 * we just decrement the reference counter so nothing untoward happens.
 */
void
vga_pci_bar_unmap(struct vga_pci_bar *bar)
{
	if (bar != NULL && bar->mapped != 0) {
		if (--bar->mapped == 0)
			bus_space_unmap(bar->bst, bar->bsh, bar->size);
	}
}

#if !defined(SMALL_KERNEL) && NACPI > 0
void
vga_save_state(struct vga_pci_softc *sc)
{
	struct vga_config *vc = sc->sc_vc;
	struct vga_handle *vh;
	struct vgascreen *scr;
	size_t i;
	char *buf;

	if (vc == NULL)
		return;

	vh = &vc->hdl;

	/*
	 * Save sequencer registers
	 */
	vga_ts_write(vh, syncreset, 1);	/* stop sequencer */
	buf = (char *)&sc->sc_save_ts;
	*buf++ = 0;
	for (i = 1; i < sizeof(sc->sc_save_ts); i++)
		*buf++ = _vga_ts_read(vh, i);
	vga_ts_write(vh, syncreset, 3);	/* start sequencer */
	/* pretend screen is not blanked */
	sc->sc_save_ts.mode &= ~0x20;
	sc->sc_save_ts.mode |= 0x80;

	/*
	 * Save CRTC registers
	 */
	buf = (char *)&sc->sc_save_crtc;
	for (i = 0; i < sizeof(sc->sc_save_crtc); i++)
		*buf++ = _pcdisplay_6845_read(&vh->vh_ph, i);

	/*
	 * Save ATC registers
	 */
	buf = (char *)&sc->sc_save_atc;
	for (i = 0; i < sizeof(sc->sc_save_atc); i++)
		*buf++ = _vga_attr_read(vh, i);

	/*
	 * Save GDC registers
	 */
	buf = (char *)&sc->sc_save_gdc;
	for (i = 0; i < sizeof(sc->sc_save_gdc); i++)
		*buf++ = _vga_gdc_read(vh, i);

	vga_save_palette(vc);

	/* XXX should also save font data */

	/*
	 * Save current screen contents if we have backing store for it,
	 * and intend to POST on resume.
	 * XXX Since we don't allocate backing store unless the second VT is
	 * XXX created, we could theoretically have no backing store available
	 * XXX at this point.
	 */
	if (vga_pci_do_post) {
		scr = vc->active;
		if (scr != NULL && scr->pcs.active && scr->pcs.mem != NULL)
			bus_space_read_region_2(vh->vh_memt, vh->vh_memh,
			    scr->pcs.dispoffset, scr->pcs.mem,
			    scr->pcs.type->ncols * scr->pcs.type->nrows);
	}
}

void
vga_restore_state(struct vga_pci_softc *sc)
{
	struct vga_config *vc = sc->sc_vc;
	struct vga_handle *vh;
	struct vgascreen *scr;
	size_t i;
	char *buf;

	if (vc == NULL)
		return;

	vh = &vc->hdl;

	/*
	 * Restore sequencer registers
	 */
	vga_ts_write(vh, syncreset, 1);	/* stop sequencer */
	buf = (char *)&sc->sc_save_ts + 1;
	for (i = 1; i < sizeof(sc->sc_save_ts); i++)
		_vga_ts_write(vh, i, *buf++);
	vga_ts_write(vh, syncreset, 3);	/* start sequencer */

	/*
	 * Restore CRTC registers
	 */
	/* unprotect registers 00-07 */
	vga_6845_write(vh, vsynce,
	    vga_6845_read(vh, vsynce) & ~0x80);
	buf = (char *)&sc->sc_save_crtc;
	for (i = 0; i < sizeof(sc->sc_save_crtc); i++)
		_pcdisplay_6845_write(&vh->vh_ph, i, *buf++);

	/*
	 * Restore ATC registers
	 */
	buf = (char *)&sc->sc_save_atc;
	for (i = 0; i < sizeof(sc->sc_save_atc); i++)
		_vga_attr_write(vh, i, *buf++);

	/*
	 * Restore GDC registers
	 */
	buf = (char *)&sc->sc_save_gdc;
	for (i = 0; i < sizeof(sc->sc_save_gdc); i++)
		_vga_gdc_write(vh, i, *buf++);

	vga_restore_palette(vc);

	/*
	 * Restore current screen contents if we have backing store for it,
	 * and have POSTed on resume.
	 * XXX Since we don't allocate backing store unless the second VT is
	 * XXX created, we could theoretically have no backing store available
	 * XXX at this point.
	 */
	if (vga_pci_do_post) {
		scr = vc->active;
		if (scr != NULL && scr->pcs.active && scr->pcs.mem != NULL)
			bus_space_write_region_2(vh->vh_memt, vh->vh_memh,
			    scr->pcs.dispoffset, scr->pcs.mem,
			    scr->pcs.type->ncols * scr->pcs.type->nrows);
	}
}
#endif
