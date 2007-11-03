/* $OpenBSD: vga_pci.c,v 1.27 2007/11/03 10:09:03 martin Exp $ */
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

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>


#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#ifdef VESAFB
#include <dev/vesa/vesabiosvar.h>
#endif

int	vga_pci_match(struct device *, void *, void *);
void	vga_pci_attach(struct device *, struct device *, void *);
paddr_t	vga_pci_mmap(void* v, off_t off, int prot);

#ifdef VESAFB
int vesafb_putcmap(struct vga_pci_softc *, struct wsdisplay_cmap *);
int vesafb_getcmap(struct vga_pci_softc *, struct wsdisplay_cmap *);
#endif

struct cfattach vga_pci_ca = {
	sizeof(struct vga_pci_softc), vga_pci_match, vga_pci_attach,
};

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
#ifdef VESAFB
	struct vga_pci_softc *sc = (struct vga_pci_softc *)self;
#endif

	/*
	 * Enable bus master; X might need this for accelerated graphics.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

#ifdef PCIAGP
	agp_attach(parent, self, aux);
#endif
#ifdef VESAFB
	if (vesabios_softc != NULL && vesabios_softc->sc_nmodes > 0) {
		sc->sc_textmode = vesafb_get_mode(sc);
		printf(", vesafb\n");
		vga_extended_attach(self, pa->pa_iot, pa->pa_memt,
		    WSDISPLAY_TYPE_PCIVGA, vga_pci_mmap);
		return;
	}
#endif
	printf("\n");
	vga_common_attach(self, pa->pa_iot, pa->pa_memt,
	    WSDISPLAY_TYPE_PCIVGA);
}

paddr_t
vga_pci_mmap(void *v, off_t off, int prot)
{
#ifdef VESAFB
	struct vga_config *vc = (struct vga_config *)v;
	struct vga_pci_softc *sc = (struct vga_pci_softc *)vc->vc_softc;

	if (sc->sc_mode == WSDISPLAYIO_MODE_DUMBFB) {
		if (off < 0 || off > vesabios_softc->sc_size)
			return (-1);
		return atop(sc->sc_base + off);
	}
#endif
#ifdef PCIAGP
	return agp_mmap(v, off, prot);
#else
	return -1;
#endif
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
#ifdef PCIAGP
	case AGPIOC_INFO:
	case AGPIOC_ACQUIRE:
	case AGPIOC_RELEASE:
	case AGPIOC_SETUP:
	case AGPIOC_ALLOCATE:
	case AGPIOC_DEALLOCATE:
	case AGPIOC_BIND:
	case AGPIOC_UNBIND:
		error = agp_ioctl(v, cmd, addr, flag, pb);
		break;
#endif
	default:
		error = ENOTTY;
	}

	return (error);
}

#ifdef notyet
void
vga_pci_close(void *v)
{
#ifdef PCIAGP
	agp_close(v);
#endif
}
#endif
