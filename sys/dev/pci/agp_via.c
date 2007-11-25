/*	$OpenBSD: agp_via.c,v 1.5 2007/11/25 17:11:12 oga Exp $	*/
/*	$NetBSD: agp_via.c,v 1.2 2001/09/15 00:25:00 thorpej Exp $	*/

/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/sys/pci/agp_via.c,v 1.3 2001/07/05 21:28:47 jhb Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/agpio.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/vga_pcivar.h>
#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>

#include <machine/bus.h>

static u_int32_t agp_via_get_aperture(struct agp_softc *);
static int agp_via_set_aperture(struct agp_softc *, u_int32_t);
static int agp_via_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_via_unbind_page(struct agp_softc *, off_t);
static void agp_via_flush_tlb(struct agp_softc *);

struct agp_methods agp_via_methods = {
	agp_via_get_aperture,
	agp_via_set_aperture,
	agp_via_bind_page,
	agp_via_unbind_page,
	agp_via_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};

struct agp_via_softc {
	u_int32_t	initial_aperture; /* aperture size at startup */
	struct agp_gatt *gatt;
	int		*regs;
};

#define REG_GARTCTRL	0
#define REG_APSIZE	1
#define REG_ATTBASE	2

int via_v2_regs[] =
	{ AGP_VIA_GARTCTRL, AGP_VIA_APSIZE, AGP_VIA_ATTBASE };
int via_v3_regs[] =
	{ AGP3_VIA_GARTCTRL, AGP3_VIA_APSIZE, AGP3_VIA_ATTBASE };

int
agp_via_attach(struct agp_softc *sc, struct pci_attach_args *pa)
{
	struct agp_via_softc *asc;
	struct agp_gatt *gatt;
	pcireg_t agpsel, capval;

	asc = malloc(sizeof *asc, M_AGP, M_NOWAIT | M_ZERO);
	if (asc == NULL) {
		printf(": can't allocate chipset-specific softc\n");
		return (ENOMEM);
	}
	sc->sc_chipc = asc;
	sc->sc_methods = &agp_via_methods;
	pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, &sc->sc_capoff,
	    &capval);

	if (AGP_CAPID_GET_MAJOR(capval) >= 3) {
		agpsel = pci_conf_read(pa->pa_pc, pa->pa_tag, AGP_VIA_AGPSEL);
		if ((agpsel & (1 << 1)) == 0) {
			asc->regs = via_v3_regs;
			printf(" (v3)");
		} else {
			asc->regs = via_v2_regs;
			printf(" (v2 compat mode)");
		}
	} else {
		asc->regs = via_v2_regs;
		printf(" (v2)");
	}

	if (agp_map_aperture(pa ,sc, AGP_APBASE, PCI_MAPREG_TYPE_MEM) != 0) {
		printf(": can't map aperture\n");
		free(asc, M_AGP);
		return (ENXIO);
	}

	asc->initial_aperture = AGP_GET_APERTURE(sc);

	for (;;) {
		gatt = agp_alloc_gatt(sc);
		if (gatt)
			break;

		/*
		 * Probably contigmalloc failure. Try reducing the
		 * aperture so that the gatt size reduces.
		 */
		if (AGP_SET_APERTURE(sc, AGP_GET_APERTURE(sc) / 2)) {
			agp_generic_detach(sc);
			printf(": can't set aperture size\n");
			return (ENOMEM);
		}
	}
	asc->gatt = gatt;

	if (asc->regs == via_v2_regs) {
		/* Install the gatt. */
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_ATTBASE],
				 gatt->ag_physical | 3);
		/* Enable the aperture. */
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_GARTCTRL],
				 0x0000000f);
	} else {
		pcireg_t gartctrl;
		/* Install the gatt. */
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_ATTBASE],
				 gatt->ag_physical);
		/* Enable the aperture. */
		gartctrl = pci_conf_read(pa->pa_pc, pa->pa_tag,
				 asc->regs[REG_ATTBASE]);
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_GARTCTRL],
				 gartctrl | (3 << 7));
	}

	return (0);
}

#if 0
static int
agp_via_detach(struct agp_softc *sc)
{
	struct agp_via_softc *asc = sc->sc_chipc;
	int error;

	error = agp_generic_detach(sc);
	if (error)
		return (error);

	pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_GARTCTRL], 0);
	pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_ATTBASE], 0);
	AGP_SET_APERTURE(sc, asc->initial_aperture);
	agp_free_gatt(sc, asc->gatt);

	return (0);
}
#endif

static u_int32_t
agp_via_get_aperture(struct agp_softc *sc)
{
	struct agp_via_softc *asc = sc->sc_chipc;
	u_int32_t apsize;

	apsize = pci_conf_read(sc->sc_pc, sc->sc_pcitag, 
	    asc->regs[REG_APSIZE]) & 0x1f;

	/*
	 * The size is determined by the number of low bits of
	 * register APBASE which are forced to zero. The low 20 bits
	 * are always forced to zero and each zero bit in the apsize
	 * field just read forces the corresponding bit in the 27:20
	 * to be zero. We calculate the aperture size accordingly.
	 */
	return ((((apsize ^ 0xff) << 20) | ((1 << 20) - 1)) + 1);
}

static int
agp_via_set_aperture(struct agp_softc *sc, u_int32_t aperture)
{
	struct agp_via_softc *asc = sc->sc_chipc;
	u_int32_t apsize;
	pcireg_t reg;

	/*
	 * Reverse the magic from get_aperture.
	 */
	apsize = ((aperture - 1) >> 20) ^ 0xff;

	/*
	 * Double check for sanity.
	 */
	if ((((apsize ^ 0xff) << 20) | ((1 << 20) - 1)) + 1 != aperture)
		return (EINVAL);

	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, asc->regs[REG_APSIZE]);
	reg &= ~0xff;
	reg |= apsize;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, asc->regs[REG_APSIZE], reg);

	return (0);
}

static int
agp_via_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_via_softc *asc = sc->sc_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical;
	return (0);
}

static int
agp_via_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_via_softc *asc = sc->sc_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return (0);
}

static void
agp_via_flush_tlb(struct agp_softc *sc)
{
	struct agp_via_softc *asc = sc->sc_chipc;
	pcireg_t gartctrl;

	if (asc->regs == via_v2_regs) {
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, asc->regs[REG_GARTCTRL],
				0x8f);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, asc->regs[REG_GARTCTRL],
				0x0f);
	} else {
		gartctrl = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
					 asc->regs[REG_GARTCTRL]);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, asc->regs[REG_GARTCTRL],
			       gartctrl & ~(1 << 7));
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, asc->regs[REG_GARTCTRL],
			       gartctrl);
	}
}
