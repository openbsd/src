/*	$OpenBSD: agp_intel.c,v 1.7 2007/11/26 15:35:15 deraadt Exp $	*/
/*	$NetBSD: agp_intel.c,v 1.3 2001/09/15 00:25:00 thorpej Exp $	*/

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
 *	$FreeBSD: src/sys/pci/agp_intel.c,v 1.4 2001/07/05 21:28:47 jhb Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/agpio.h>
#include <sys/device.h>
#include <sys/agpio.h>


#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>

#include <machine/bus.h>

struct agp_intel_softc {
	u_int32_t		initial_aperture; /* aperture size at startup */
	struct agp_gatt 	*gatt;
	struct pci_attach_args 	vga_pa; /* vga card */
	u_int			aperture_mask;
	int			chiptype; 
#define	CHIP_INTEL	0x0
#define	CHIP_I443	0x1
#define	CHIP_I840	0x2
#define	CHIP_I845	0x3
#define	CHIP_I850	0x4
#define	CHIP_I865	0x5
};


static u_int32_t agp_intel_get_aperture(struct agp_softc *);
static int agp_intel_set_aperture(struct agp_softc *, u_int32_t);
static int agp_intel_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_intel_unbind_page(struct agp_softc *, off_t);
static void agp_intel_flush_tlb(struct agp_softc *);

struct agp_methods agp_intel_methods = {
	agp_intel_get_aperture,
	agp_intel_set_aperture,
	agp_intel_bind_page,
	agp_intel_unbind_page,
	agp_intel_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};

static int
agp_intel_vgamatch(struct pci_attach_args *pa)
{
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82855PE_AGP:
	case PCI_PRODUCT_INTEL_82443LX_AGP:
	case PCI_PRODUCT_INTEL_82443BX_AGP:
	case PCI_PRODUCT_INTEL_82440BX_AGP:
	case PCI_PRODUCT_INTEL_82850_AGP:	/* i850/i860 */
	case PCI_PRODUCT_INTEL_82845_AGP:
	case PCI_PRODUCT_INTEL_82840_AGP:
	case PCI_PRODUCT_INTEL_82865_AGP:
	case PCI_PRODUCT_INTEL_82875P_AGP:
		return (1);
	}

	return (0);
}

int
agp_intel_attach(struct agp_softc *sc, struct pci_attach_args *pa)
{
	struct agp_intel_softc *isc;
	struct agp_gatt *gatt;
	pcireg_t reg;
	u_int32_t value;

	isc = malloc(sizeof *isc, M_AGP, M_NOWAIT | M_ZERO);
	if (isc == NULL) {
		printf("can't allocate chipset-specific softc\n");
		return (ENOMEM);
	}

	sc->sc_methods = &agp_intel_methods;
	sc->sc_chipc = isc;

	if (pci_find_device(&isc->vga_pa, agp_intel_vgamatch) == 0)
		isc->chiptype = CHIP_INTEL;

	pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, &sc->sc_capoff,
	    NULL);

	if (agp_map_aperture(pa, sc, AGP_APBASE, PCI_MAPREG_TYPE_MEM) != 0) {
		printf("can't map aperture\n");
		free(isc, M_AGP);
		sc->sc_chipc = NULL;
		return (ENXIO);
	}

	switch (PCI_PRODUCT(isc->vga_pa.pa_id)) {
	case PCI_PRODUCT_INTEL_82443LX_AGP:
	case PCI_PRODUCT_INTEL_82443BX_AGP:
	case PCI_PRODUCT_INTEL_82440BX_AGP:
		isc->chiptype = CHIP_I443;
		break;
	case PCI_PRODUCT_INTEL_82840_AGP:
		isc->chiptype = CHIP_I840;
		break;
	case PCI_PRODUCT_INTEL_82855PE_AGP:
	case PCI_PRODUCT_INTEL_82845_AGP:
		isc->chiptype = CHIP_I845;
		break;
	case PCI_PRODUCT_INTEL_82850_AGP:
		isc->chiptype = CHIP_I850;
		break;
	case PCI_PRODUCT_INTEL_82865_AGP:
	case PCI_PRODUCT_INTEL_82875P_AGP:
		isc->chiptype = CHIP_I865;
		break;
	}

	/* Determine maximum supported aperture size. */
	value = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_APSIZE);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag,
		AGP_INTEL_APSIZE, APSIZE_MASK);
	isc->aperture_mask = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
		AGP_INTEL_APSIZE) & APSIZE_MASK;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_APSIZE, value);
	isc->initial_aperture = AGP_GET_APERTURE(sc);

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
			printf("failed to set aperture\n");
			return (ENOMEM);
		}
	}
	isc->gatt = gatt;

	/* Install the gatt. */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_ATTBASE,
	    gatt->ag_physical);
	
	/* Enable the GLTB and setup the control register. */
	switch (isc->chiptype) {
	case CHIP_I443:
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL,
		    AGPCTRL_AGPRSE | AGPCTRL_GTLB);

	default:
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL,
		    pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL)
			| AGPCTRL_GTLB);
	}

	/* Enable things, clear errors etc. */
	switch (isc->chiptype) {
	case CHIP_I845:
	case CHIP_I865:
		{
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I840_MCHCFG);
		reg |= MCHCFG_AAGN;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_I840_MCHCFG, reg);
		break;
		}
	case CHIP_I840:
	case CHIP_I850:
		{
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCMD);
		reg |= AGPCMD_AGPEN;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCMD,
			reg);
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I840_MCHCFG);
		reg |= MCHCFG_AAGN;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_I840_MCHCFG,
			reg);
		break;
		}
	default:
		{
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_NBXCFG);
		reg &= ~NBXCFG_APAE;
		reg |=  NBXCFG_AAGN;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_NBXCFG, reg);
		}
	}

	/* Clear Error status */
	switch (isc->chiptype) {
	case CHIP_I840:
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			AGP_INTEL_I8XX_ERRSTS, 0xc000);
		break;
	case CHIP_I845:
	case CHIP_I850:
	case CHIP_I865:
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			AGP_INTEL_I8XX_ERRSTS, 0x00ff);
		break;

	default:
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			AGP_INTEL_ERRSTS, 0x70);
	}

	return (0);
}

#if 0
static int
agp_intel_detach(struct agp_softc *sc)
{
	int error;
	pcireg_t reg;
	struct agp_intel_softc *isc = sc->sc_chipc;

	error = agp_generic_detach(sc);
	if (error)
		return (error);

	/* XXX i845/i855PM/i840/i850E */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_NBXCFG);
	reg &= ~(1 << 9);
	printf("%s: set NBXCFG to %x\n", __FUNCTION__, reg);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_NBXCFG, reg);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_ATTBASE, 0);
	AGP_SET_APERTURE(sc, isc->initial_aperture);
	agp_free_gatt(sc, isc->gatt);

	return (0);
}
#endif

static u_int32_t
agp_intel_get_aperture(struct agp_softc *sc)
{
	struct agp_intel_softc *isc = sc->sc_chipc;
	u_int32_t apsize;

	apsize = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
	    AGP_INTEL_APSIZE) & isc->aperture_mask;

	/*
	 * The size is determined by the number of low bits of
	 * register APBASE which are forced to zero. The low 22 bits
	 * are always forced to zero and each zero bit in the apsize
	 * field just read forces the corresponding bit in the 27:22
	 * to be zero. We calculate the aperture size accordingly.
	 */
	return ((((apsize ^ isc->aperture_mask) << 22) | ((1 << 22) - 1)) + 1);
}

static int
agp_intel_set_aperture(struct agp_softc *sc, u_int32_t aperture)
{
	struct agp_intel_softc *isc = sc->sc_chipc;
	u_int32_t apsize;

	/*
	 * Reverse the magic from get_aperture.
	 */
	apsize = ((aperture - 1) >> 22) ^ isc->aperture_mask;

	/*
	 * Double check for sanity.
	 */
	if ((((apsize ^ isc->aperture_mask) << 22) |
			((1 << 22) - 1)) + 1 != aperture)
		return (EINVAL);

	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_APSIZE, apsize);

	return (0);
}

static int
agp_intel_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_intel_softc *isc = sc->sc_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	isc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical | 0x17;
	return (0);
}

static int
agp_intel_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_intel_softc *isc = sc->sc_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	isc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return (0);
}

static void
agp_intel_flush_tlb(struct agp_softc *sc)
{
	struct agp_intel_softc *isc = sc->sc_chipc;
	pcireg_t reg;

	switch (isc->chiptype) {
	case CHIP_I865:
	case CHIP_I850:
	case CHIP_I845:
	case CHIP_I840:
	case CHIP_I443:
		{
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL);
		reg &= ~AGPCTRL_GTLB;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL,
			reg);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL,
			reg | AGPCTRL_GTLB);
		break;
		}
	default: /* XXX */
		{
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL,
			0x2200);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_INTEL_AGPCTRL,
			0x2280);
		}
	}
}
