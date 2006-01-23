/*	$OpenBSD: grf_iv.c,v 1.39 2006/01/23 19:06:04 miod Exp $	*/
/*	$NetBSD: grf_iv.c,v 1.17 1997/02/20 00:23:27 scottr Exp $	*/

/*
 * Copyright (C) 1998 Scott Reynolds
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1995 Allen Briggs.  All rights reserved.
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
 *	This product includes software developed by Allen Briggs.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Graphics display driver for the Macintosh internal video for machines
 * that don't map it into a fake nubus card.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/viareg.h>

#include <uvm/uvm_extern.h>

#include <mac68k/dev/nubus.h>
#include <mac68k/dev/obiovar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <mac68k/dev/macfbvar.h>

extern u_int32_t	mac68k_vidphys;
extern u_int32_t	mac68k_vidlen;
extern long		videoaddr;
extern long		videorowbytes;
extern long		videobitdepth;
extern u_long		videosize;

int	macfb_obio_match(struct device *, void *, void *);
void	macfb_obio_attach(struct device *, struct device *, void *);

struct cfattach macfb_obio_ca = {
	sizeof(struct macfb_softc), macfb_obio_match, macfb_obio_attach
};

#define DAFB_BASE		0xf9000000
#define DAFB_CONTROL_BASE	0xf9800000
#define	DAFB_CMAP_BASE		0xf9800200
#define CIVIC_BASE		0x50100000
#define CIVIC_CONTROL_BASE	0x50036000
#define VALKYRIE_BASE		0xf9000000
#define VALKYRIE_CONTROL_BASE	0x50f2a000

void	dafb_setcolor(void *, u_int, u_int);

int
macfb_obio_match(struct device *parent, void *vcf, void *aux)
{
	struct obio_attach_args *oa = (struct obio_attach_args *)aux;
	bus_space_handle_t bsh;
	static int found;
	u_int base;

	if (found != 0)
		return (0);

	found = 1;

        switch (current_mac_model->class) {
	case MACH_CLASSQ2:
		if (current_mac_model->machineid != MACH_MACLC575) {
			base = VALKYRIE_CONTROL_BASE;

			if (bus_space_map(oa->oa_tag, base, 0x40, 0, &bsh))
				return (0);

			/* Disable interrupts */
			bus_space_write_1(oa->oa_tag, bsh, 0x18, 0x1);

			bus_space_unmap(oa->oa_tag, bsh, 0x40);
			break;
		}
		/*
		 * Note:  the only system in this class that does not have
		 * the Valkyrie chip -- at least, that we know of -- is
		 * the Performa/LC 57x series.  This system has a version
		 * of the DAFB controller, instead.
		 *
		 * If this assumption proves false, we'll have to be more
		 * intelligent here.
		 */
		/*FALLTHROUGH*/
	case MACH_CLASSQ:
		/*
		 * Assume DAFB for all of these, unless we can't
		 * access the memory.
		 */
		base = DAFB_CONTROL_BASE;

		if (bus_space_map(oa->oa_tag, base, 0x20, 0, &bsh))
			return (0);

		if (mac68k_bus_space_probe(oa->oa_tag, bsh, 0x1c, 4) == 0) {
			bus_space_unmap(oa->oa_tag, bsh, 0x20);
			return (0);
		}

		bus_space_unmap(oa->oa_tag, bsh, 0x20);

		if (bus_space_map(oa->oa_tag, base + 0x100, 0x20, 0, &bsh))
			return (0);

		if (mac68k_bus_space_probe(oa->oa_tag, bsh, 0x04, 4) == 0) {
			bus_space_unmap(oa->oa_tag, bsh, 0x20);
			return (0);
		}

		/* Disable interrupts */
		bus_space_write_4(oa->oa_tag, bsh, 0x04, 0);

		/* Clear any interrupts */
		bus_space_write_4(oa->oa_tag, bsh, 0x0C, 0);
		bus_space_write_4(oa->oa_tag, bsh, 0x10, 0);
		bus_space_write_4(oa->oa_tag, bsh, 0x14, 0);

		bus_space_unmap(oa->oa_tag, bsh, 0x20);
		break;
	case MACH_CLASSAV:
		base = CIVIC_CONTROL_BASE;

		if (bus_space_map(oa->oa_tag, base, 0x1000, 0, &bsh))
			return (0);

		/* Disable interrupts */
		bus_space_write_1(oa->oa_tag, bsh, 0x120, 0);

		bus_space_unmap(oa->oa_tag, bsh, 0x1000);
		break;
	case MACH_CLASSIIci:
	case MACH_CLASSIIsi:
		if (mac68k_vidlen == 0 ||
		    (via2_reg(rMonitor) & RBVMonitorMask) == RBVMonIDNone)
			found = 0;
		break;
	default:
		if (mac68k_vidlen == 0)
			found = 0;
		break;
	}

	return (found);
}

void
macfb_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct obio_attach_args *oa = (struct obio_attach_args *) aux;
	struct macfb_softc *sc = (struct macfb_softc *)self;
	u_long length;
	u_int32_t vbase1, vbase2;
	struct macfb_devconfig *dc;

	sc->card_id = 0;

	dc = malloc(sizeof(*dc), M_DEVBUF, M_WAITOK);
	bzero(dc, sizeof(*dc));

        switch (current_mac_model->class) {
	case MACH_CLASSQ2:
		if (current_mac_model->machineid != MACH_MACLC575) {
			sc->sc_basepa = VALKYRIE_BASE;
			length = 0x00100000;		/* 1MB */

			if (sc->sc_basepa <= mac68k_vidphys &&
			    mac68k_vidphys < (sc->sc_basepa + length))
				sc->sc_fbofs = mac68k_vidphys - sc->sc_basepa;
			else
				sc->sc_fbofs = 0;

#ifdef DEBUG
			printf(" @ %lx", sc->sc_basepa + sc->sc_fbofs);
#endif
			printf(": Valkyrie\n");
			break;
		}
		/* See note in grfiv_match() */
		/*FALLTHROUGH*/
        case MACH_CLASSQ:
		sc->sc_tag = oa->oa_tag;
		if (bus_space_map(sc->sc_tag, DAFB_CONTROL_BASE, 0x20, 0,
		    &sc->sc_regh)) {
			printf(": failed to map DAFB register space\n");
			free(dc, M_DEVBUF);
			return;
		}

		sc->sc_basepa = DAFB_BASE;
		length = 0x00100000;		/* 1MB */

		/* Compute the current frame buffer offset */
		vbase1 = bus_space_read_4(sc->sc_tag, sc->sc_regh, 0x0) & 0xfff;

		/*
		 * XXX The following exists because the DAFB v7 in these
		 * systems doesn't return reasonable values to use for fbofs.
		 * Ken'ichi Ishizaka gets credit for this hack.  (sar 19990426)
		 * (Does this get us the correct result for _all_ DAFB-
		 * equipped systems and monitor combinations?  It seems
		 * possible, if not likely...)
		 */
		switch (current_mac_model->machineid) {
		case MACH_MACLC475:
		case MACH_MACLC475_33:
		case MACH_MACLC575:
		case MACH_MACQ605:
		case MACH_MACQ605_33:
			vbase1 &= 0x3f;
			break;
		}
		vbase2 = bus_space_read_4(sc->sc_tag, sc->sc_regh, 0x4) & 0xf;
		sc->sc_fbofs = (vbase1 << 9) | (vbase2 << 5);

#ifdef DEBUG
		printf(" @ %lx", sc->sc_basepa + sc->sc_fbofs);
#endif
		printf(": DAFB, monitor sense %x\n",
		    (bus_space_read_4(sc->sc_tag, sc->sc_regh, 0x1c) & 0x7));

		bus_space_unmap(sc->sc_tag, sc->sc_regh, 0x20);

		if (bus_space_map(sc->sc_tag, DAFB_CMAP_BASE, 0x20, 0,
		    &sc->sc_regh) == 0) {
			dc->dc_cmapregs = (vaddr_t)bus_space_vaddr(sc->sc_tag,
			    sc->sc_regh);
			dc->dc_setcolor = dafb_setcolor;
		}

		break;
	case MACH_CLASSAV:
		sc->sc_basepa = CIVIC_BASE;
		length = 0x00200000;		/* 2MB */

		if (sc->sc_basepa <= mac68k_vidphys &&
		    mac68k_vidphys < (sc->sc_basepa + length))
			sc->sc_fbofs = mac68k_vidphys - sc->sc_basepa;
		else
			sc->sc_fbofs = 0;

#ifdef DEBUG
		printf(" @ %lx", sc->sc_basepa + sc->sc_fbofs);
#endif
		printf(": Civic\n");
		break;
	case MACH_CLASSIIci:
	case MACH_CLASSIIsi:
		sc->sc_basepa = trunc_page(mac68k_vidphys);
		sc->sc_fbofs = m68k_page_offset(mac68k_vidphys);
		length = mac68k_vidlen + sc->sc_fbofs;

#ifdef DEBUG
		printf(" @ %lx: RBV", sc->sc_basepa + sc->sc_fbofs);
		switch (via2_reg(rMonitor) & RBVMonitorMask) {
		case RBVMonIDBWP:
			printf(", 15\" monochrome portrait");
			break;
		case RBVMonIDRGB12:
			printf(", 12\" color");
			break;
		case RBVMonIDRGB15:
			printf(", 15\" color");
			break;
		case RBVMonIDStd:
			printf(", Macintosh II");
			break;
		default:
			printf(", unrecognized");
			break;
		}
		printf(" display\n");
#else
		printf(": RBV\n");
#endif

		break;
	default:
		sc->sc_basepa = trunc_page(mac68k_vidphys);
		sc->sc_fbofs = m68k_page_offset(mac68k_vidphys);
		length = mac68k_vidlen + sc->sc_fbofs;

#ifdef DEBUG
		printf(" @ %lx:", sc->sc_basepa + sc->sc_fbofs);
#endif
		printf(": On-board video\n");
		break;
	}

	if (bus_space_map(sc->sc_tag, sc->sc_basepa, length, 0,
	    &sc->sc_handle)) {
		printf("%s: failed to map video RAM\n", sc->sc_dev.dv_xname);
		free(dc, M_DEVBUF);
		return;
	}

	if (sc->sc_basepa <= mac68k_vidphys &&
	    mac68k_vidphys < (sc->sc_basepa + length))
		videoaddr =
		    (vaddr_t)bus_space_vaddr(sc->sc_tag, sc->sc_handle) +
		    sc->sc_fbofs;

	dc->dc_vaddr = (vaddr_t)bus_space_vaddr(sc->sc_tag, sc->sc_handle);
	dc->dc_paddr = sc->sc_basepa;
	dc->dc_offset = sc->sc_fbofs;
	dc->dc_wid = videosize & 0xffff;
	dc->dc_ht = (videosize >> 16) & 0xffff;
	dc->dc_depth = videobitdepth;
	dc->dc_rowbytes = videorowbytes;
	dc->dc_size = dc->dc_ht * dc->dc_rowbytes;
	dc->nscreens = 0;

	/* Perform common video attachment. */
	macfb_attach_common(sc, dc);
}

/*
 * Basic indexed modes palette handling.
 */

void
dafb_setcolor(void *v, u_int start, u_int end)
{
	struct macfb_devconfig *dc = v;
	u_int i;
	u_int8_t *c;

	c = dc->dc_cmap;

	/*
	 * DAFB can not start a colormap update at a color index different
	 * than zero, so we need to reprogram all slots below the requested
	 * range.
	 */
	*(volatile u_int32_t *)(dc->dc_cmapregs) = 0;
	for (i = 0; i < end; i++) {
		*(volatile u_int8_t *)(dc->dc_cmapregs + 0x13) = *c++;
		*(volatile u_int8_t *)(dc->dc_cmapregs + 0x13) = *c++;
		*(volatile u_int8_t *)(dc->dc_cmapregs + 0x13) = *c++;
	}
}
