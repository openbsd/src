/*	$OpenBSD: agp_amd.c,v 1.4 2007/10/06 23:50:54 krw Exp $	*/
/*	$NetBSD: agp_amd.c,v 1.6 2001/10/06 02:48:50 thorpej Exp $	*/


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
 *	$FreeBSD: src/sys/pci/agp_amd.c,v 1.6 2001/07/05 21:28:46 jhb Exp $
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

#include <dev/pci/pcidevs.h>

#define READ2(off)	bus_space_read_2(asc->iot, asc->ioh, off)
#define READ4(off)	bus_space_read_4(asc->iot, asc->ioh, off)
#define WRITE2(off,v)	bus_space_write_2(asc->iot, asc->ioh, off, v)
#define WRITE4(off,v)	bus_space_write_4(asc->iot, asc->ioh, off, v)

struct agp_amd_gatt {
	bus_dmamap_t	ag_dmamap;
	bus_dma_segment_t ag_dmaseg;
	int		ag_nseg;
	u_int32_t	ag_entries;
	u_int32_t      *ag_vdir;	/* virtual address of page dir */
	bus_addr_t	ag_pdir;	/* bus address of page dir */
	u_int32_t      *ag_virtual;	/* virtual address of gatt */
	bus_addr_t	ag_physical;	/* bus address of gatt */
	size_t		ag_size;
};

struct agp_amd_softc {
	u_int32_t	initial_aperture; /* aperture size at startup */
	struct agp_amd_gatt *gatt;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
};

static u_int32_t agp_amd_get_aperture(struct vga_pci_softc *);
static int agp_amd_set_aperture(struct vga_pci_softc *, u_int32_t);
static int agp_amd_bind_page(struct vga_pci_softc *, off_t, bus_addr_t);
static int agp_amd_unbind_page(struct vga_pci_softc *, off_t);
static void agp_amd_flush_tlb(struct vga_pci_softc *);


struct agp_methods agp_amd_methods = {
	agp_amd_get_aperture,
	agp_amd_set_aperture,
	agp_amd_bind_page,
	agp_amd_unbind_page,
	agp_amd_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};


static struct agp_amd_gatt *
agp_amd_alloc_gatt(struct vga_pci_softc *sc)
{
	u_int32_t apsize = AGP_GET_APERTURE(sc);
	u_int32_t entries = apsize >> AGP_PAGE_SHIFT;
	struct agp_amd_gatt *gatt;
	int i, npages;
	caddr_t vdir;

	gatt = malloc(sizeof(struct agp_amd_gatt), M_DEVBUF, M_NOWAIT);
	if (!gatt)
		return (0);

	if (agp_alloc_dmamem(sc->sc_dmat,
	    AGP_PAGE_SIZE + entries * sizeof(u_int32_t), 0,
	    &gatt->ag_dmamap, &vdir, &gatt->ag_pdir,
	    &gatt->ag_dmaseg, 1, &gatt->ag_nseg) != 0) {
		printf("failed to allocate GATT\n");
		free(gatt, M_DEVBUF);
		return (NULL);
	}

	gatt->ag_vdir = (u_int32_t *)vdir;
	gatt->ag_entries = entries;
	gatt->ag_virtual = (u_int32_t *)(vdir + AGP_PAGE_SIZE);
	gatt->ag_physical = gatt->ag_pdir + AGP_PAGE_SIZE;
	gatt->ag_size = AGP_PAGE_SIZE + entries * sizeof(u_int32_t);

	memset(gatt->ag_vdir, 0, AGP_PAGE_SIZE);
	memset(gatt->ag_virtual, 0, entries * sizeof(u_int32_t));

	/*
	 * Map the pages of the GATT into the page directory.
	 */
	npages = ((entries * sizeof(u_int32_t) + AGP_PAGE_SIZE - 1)
		  >> AGP_PAGE_SHIFT);

	for (i = 0; i < npages; i++)
		gatt->ag_vdir[i] = (gatt->ag_physical + i * AGP_PAGE_SIZE) | 1;

	/*
	 * Make sure the chipset can see everything.
	 */
	agp_flush_cache();

	return (gatt);
}

#if 0
static void
agp_amd_free_gatt(struct vga_pci_softc *sc, struct agp_amd_gatt *gatt)
{
	agp_free_dmamem(sc->sc_dmat, gatt->ag_size,
	    gatt->ag_dmamap, (caddr_t)gatt->ag_virtual, &gatt->ag_dmaseg,
	    gatt->ag_nseg);
	free(gatt, M_DEVBUF);
}
#endif

int
agp_amd_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa, struct pci_attach_args *pchb_pa)
{
	struct agp_amd_softc *asc;
	struct agp_amd_gatt *gatt;
	pcireg_t reg;
	int error;

	asc = malloc(sizeof *asc, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (asc == NULL) {
		printf(": can't allocate softc\n");
		/* agp_generic_detach(sc) */
		return (ENOMEM);
	}

	error = pci_mapreg_map(pchb_pa, AGP_AMD751_REGISTERS,
	     PCI_MAPREG_TYPE_MEM,0, &asc->iot, &asc->ioh, NULL, NULL, 0);
	if (error != 0) {
		printf(": can't map AGP registers\n");
		agp_generic_detach(sc);
		return (error);
	}

	if (agp_map_aperture(sc, AGP_APBASE, PCI_MAPREG_TYPE_MEM) != 0) {
		printf(": can't map aperture\n");
		agp_generic_detach(sc);
		free(asc, M_DEVBUF);
		return (ENXIO);
	}
	sc->sc_methods = &agp_amd_methods;
	sc->sc_chipc = asc;
	asc->initial_aperture = AGP_GET_APERTURE(sc);

	for (;;) {
		gatt = agp_amd_alloc_gatt(sc);
		if (gatt)
			break;

		/*
		 * Probably contigmalloc failure. Try reducing the
		 * aperture so that the gatt size reduces.
		 */
		if (AGP_SET_APERTURE(sc, AGP_GET_APERTURE(sc) / 2)) {
			printf(": can't set aperture\n");
			return (ENOMEM);
		}
	}
	asc->gatt = gatt;

	/* Install the gatt. */
	WRITE4(AGP_AMD751_ATTBASE, gatt->ag_physical);

	/* Enable synchronisation between host and agp. */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_AMD751_MODECTRL);
	reg &= ~0x00ff00ff;
	reg |= (AGP_AMD751_MODECTRL_SYNEN) | (AGP_AMD751_MODECTRL2_GPDCE << 16);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_AMD751_MODECTRL, reg);
	/* Enable the TLB and flush */
	WRITE2(AGP_AMD751_STATUS,
	    READ2(AGP_AMD751_STATUS) | AGP_AMD751_STATUS_GCE);
	AGP_FLUSH_TLB(sc);

	return (0);
}

#if 0
static int
agp_amd_detach(struct vga_pci_softc *sc)
{
	pcireg_t reg;
	struct agp_amd_softc *asc = sc->sc_chipc;

	/* Disable the TLB.. */
	WRITE2(AGP_AMD751_STATUS,
	    READ2(AGP_AMD751_STATUS) & ~AGP_AMD751_STATUS_GCE);

	/* Disable host-agp sync */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_AMD751_MODECTRL);
	reg &= 0xffffff00;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_AMD751_MODECTRL, reg);

	/* Clear the GATT base */
	WRITE4(AGP_AMD751_ATTBASE, 0);

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(sc, asc->initial_aperture);

	agp_amd_free_gatt(sc, asc->gatt);

	/* XXXfvdl no pci_mapreg_unmap */

	return (0);
}
#endif

static u_int32_t
agp_amd_get_aperture(struct vga_pci_softc *sc)
{
	int vas;

	vas = (pci_conf_read(sc->sc_pc, sc->sc_pcitag,
	    AGP_AMD751_APCTRL) & 0x06);
	vas >>= 1;
	/*
	 * The aperture size is equal to 32M<<vas.
	 */
	return ((32 * 1024 * 1024) << vas);
}

static int
agp_amd_set_aperture(struct vga_pci_softc *sc, u_int32_t aperture)
{
	int vas;
	pcireg_t reg;

	/*
	 * Check for a power of two and make sure its within the
	 * programmable range.
	 */
	if (aperture & (aperture - 1)
	    || aperture < 32*1024*1024
	    || aperture > 2U*1024*1024*1024)
		return (EINVAL);

	vas = ffs(aperture / 32*1024*1024) - 1;

	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_AMD751_APCTRL);
	reg = (reg & ~0x06) | (vas << 1);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_AMD751_APCTRL, reg);

	return (0);
}

static int
agp_amd_bind_page(struct vga_pci_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_amd_softc *asc = sc->sc_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical | 1;
	return (0);
}

static int
agp_amd_unbind_page(struct vga_pci_softc *sc, off_t offset)
{
	struct agp_amd_softc *asc = sc->sc_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return (0);
}

static void
agp_amd_flush_tlb(struct vga_pci_softc *sc)
{
	struct agp_amd_softc *asc = sc->sc_chipc;

	/* Set the cache invalidate bit and wait for the chipset to clear */
	WRITE4(AGP_AMD751_TLBCTRL, 1);
	do {
		DELAY(1);
	} while (READ4(AGP_AMD751_TLBCTRL));
}
