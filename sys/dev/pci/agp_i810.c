/*	$OpenBSD: agp_i810.c,v 1.4 2003/02/13 20:07:33 mickey Exp $	*/
/*	$NetBSD: agp_i810.c,v 1.8 2001/09/20 20:00:16 fvdl Exp $	*/

/*-
 * Copyright (c) 2000 Doug Rabson
 * Copyright (c) 2000 Ruslan Ermilov
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
 *	$FreeBSD: src/sys/pci/agp_i810.c,v 1.4 2001/07/05 21:28:47 jhb Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/agpio.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>

#include <dev/pci/vga_pcivar.h>

#include <machine/bus.h>

#define READ1(off)	bus_space_read_1(isc->bst, isc->bsh, off)
#define WRITE4(off,v)	bus_space_write_4(isc->bst, isc->bsh, off, v)

struct agp_i810_softc {
	u_int32_t initial_aperture;	   /* aperture size at startup */
	struct agp_gatt *gatt;
	u_int32_t dcache_size;
	bus_space_tag_t bst;		    /* bus_space tag */
	bus_space_handle_t bsh;		    /* bus_space handle */
};

u_int32_t agp_i810_get_aperture(struct vga_pci_softc *);
int	agp_i810_set_aperture(struct vga_pci_softc *, u_int32_t);
int	agp_i810_bind_page(struct vga_pci_softc *, off_t, bus_addr_t);
int	agp_i810_unbind_page(struct vga_pci_softc *, off_t);
void	agp_i810_flush_tlb(struct vga_pci_softc *);
int	agp_i810_enable(struct vga_pci_softc *, u_int32_t mode);
struct agp_memory *
	agp_i810_alloc_memory(struct vga_pci_softc *, int, vsize_t);
int	agp_i810_free_memory(struct vga_pci_softc *, struct agp_memory *);
int	agp_i810_bind_memory(struct vga_pci_softc *, struct agp_memory *,
	    off_t);
int	agp_i810_unbind_memory(struct vga_pci_softc *, struct agp_memory *);

struct agp_methods agp_i810_methods = {
	agp_i810_get_aperture,
	agp_i810_set_aperture,
	agp_i810_bind_page,
	agp_i810_unbind_page,
	agp_i810_flush_tlb,
	agp_i810_enable,
	agp_i810_alloc_memory,
	agp_i810_free_memory,
	agp_i810_bind_memory,
	agp_i810_unbind_memory,
};


int
agp_i810_attach(struct vga_pci_softc* sc, struct pci_attach_args *pa,
		struct pci_attach_args *pchb_pa)
{
	struct agp_i810_softc *isc;
	struct agp_gatt *gatt;
	int error;

	isc = malloc(sizeof *isc, M_DEVBUF, M_NOWAIT);
	if (isc == NULL) {
		printf(": can't allocate chipset-specific softc\n");
		return (ENOMEM);
	}
	memset(isc, 0, sizeof *isc);
	sc->sc_chipc = isc;
	sc->sc_methods = &agp_i810_methods;
	sc->sc_dmat = pa->pa_dmat;	/* XXX fvdl */

	if ((error = agp_map_aperture(sc))) {
		printf(": can't map aperture\n");
		free(isc, M_DEVBUF);
		return (error);
	}

	error = pci_mapreg_map(pa, AGP_I810_MMADR,
	    PCI_MAPREG_TYPE_MEM, 0, &isc->bst, &isc->bsh, NULL, NULL, 0);
	if (error != 0) {
		printf(": can't map mmadr registers\n");
		return (error);
	}

	isc->initial_aperture = AGP_GET_APERTURE(sc);

	if (READ1(AGP_I810_DRT) & AGP_I810_DRT_POPULATED)
		isc->dcache_size = 4 * 1024 * 1024;
	else
		isc->dcache_size = 0;

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
			return (ENOMEM);
		}
	}
	isc->gatt = gatt;

	/* Install the GATT. */
	WRITE4(AGP_I810_PGTBL_CTL, gatt->ag_physical | 1);

	/*
	 * Make sure the chipset can see everything.
	 */
	agp_flush_cache();

	return (0);
}

u_int32_t
agp_i810_get_aperture(struct vga_pci_softc *sc)
{
	u_int16_t miscc;

	miscc = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I810_SMRAM) >> 16;
	if ((miscc & AGP_I810_MISCC_WINSIZE) == AGP_I810_MISCC_WINSIZE_32)
		return (32 * 1024 * 1024);
	else
		return (64 * 1024 * 1024);
}

int
agp_i810_set_aperture(struct vga_pci_softc *sc, u_int32_t aperture)
{
	pcireg_t reg, miscc;

	/*
	 * Double check for sanity.
	 */
	if (aperture != 32 * 1024 * 1024 && aperture != 64 * 1024 * 1024) {
		printf("AGP: bad aperture size %d\n", aperture);
		return (EINVAL);
	}

	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I810_SMRAM);
	miscc = reg >> 16;
	miscc &= ~AGP_I810_MISCC_WINSIZE;
	if (aperture == 32 * 1024 * 1024)
		miscc |= AGP_I810_MISCC_WINSIZE_32;
	else
		miscc |= AGP_I810_MISCC_WINSIZE_64;

	reg &= 0x0000ffff;
	reg |= (miscc << 16);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_I810_SMRAM, miscc);

	return (0);
}

int
agp_i810_bind_page(struct vga_pci_softc *sc, off_t offset, bus_addr_t pa)
{
	struct agp_i810_softc *isc = sc->sc_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	WRITE4(AGP_I810_GTT + (u_int32_t)(offset >> AGP_PAGE_SHIFT) * 4,
	    pa | 1);

	return (0);
}

int
agp_i810_unbind_page(struct vga_pci_softc *sc, off_t offset)
{
	struct agp_i810_softc *isc = sc->sc_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	WRITE4(AGP_I810_GTT + (u_int32_t)(offset >> AGP_PAGE_SHIFT) * 4, 0);
	return (0);
}

/*
 * Writing via memory mapped registers already flushes all TLBs.
 */
void
agp_i810_flush_tlb(struct vga_pci_softc *sc)
{
}

int
agp_i810_enable(struct vga_pci_softc *sc, u_int32_t mode)
{
	return (0);
}

struct agp_memory *
agp_i810_alloc_memory(struct vga_pci_softc *sc, int type, vsize_t size)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	struct agp_memory *mem;
	int error;

	if (type == 1) {
		/*
		 * Mapping local DRAM into GATT.
		 */
		if (size != isc->dcache_size)
			return (NULL);
	} else if (type == 2) {
		/*
		 * Bogus mapping of a single page for the hardware cursor.
		 */
		if (size != AGP_PAGE_SIZE)
			return (NULL);
	}

	mem = malloc(sizeof *mem, M_DEVBUF, M_WAITOK);
	bzero(mem, sizeof *mem);
	mem->am_id = sc->sc_nextid++;
	mem->am_size = size;
	mem->am_type = type;
	
	if (type == 2) {
		mem->am_dmaseg = malloc(sizeof *mem->am_dmaseg, M_DEVBUF,
		    M_WAITOK);

		if ((error = agp_alloc_dmamem(sc->sc_dmat, size, 0,
		    &mem->am_dmamap, &mem->am_virtual, &mem->am_physical,
		    mem->am_dmaseg, 1, &mem->am_nseg)) != 0) {
			free(mem->am_dmaseg, M_DEVBUF);
			free(mem, M_DEVBUF);
			printf("agp: agp_alloc_dmamem(%d)\n", error);
			return (NULL);
		}
	} else if (type != 1) {
		if ((error = bus_dmamap_create(sc->sc_dmat, size,
		    size / PAGE_SIZE + 1, size, 0, BUS_DMA_NOWAIT,
		    &mem->am_dmamap)) != 0) {
			free(mem, M_DEVBUF);
			printf("agp: bus_dmamap_create(%d)\n", error);
			return (NULL);
		}
	}

	TAILQ_INSERT_TAIL(&sc->sc_memory, mem, am_link);
	sc->sc_allocated += size;

	return (mem);
}

int
agp_i810_free_memory(struct vga_pci_softc *sc, struct agp_memory *mem)
{
	if (mem->am_is_bound)
		return (EBUSY);
	
	if (mem->am_type == 2) {
		agp_free_dmamem(sc->sc_dmat, mem->am_size, mem->am_dmamap,
		    mem->am_virtual, mem->am_dmaseg, mem->am_nseg);
		free(mem->am_dmaseg, M_DEVBUF);
	}

	sc->sc_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->sc_memory, mem, am_link);
	free(mem, M_DEVBUF);
	return (0);
}

int
agp_i810_bind_memory(struct vga_pci_softc *sc, struct agp_memory *mem,
		     off_t offset)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	u_int32_t regval, i;

	/*
	 * XXX evil hack: the PGTBL_CTL appearently gets overwritten by the
	 * X server for mysterious reasons which leads to crashes if we write
	 * to the GTT through the MMIO window.
	 * Until the issue is solved, simply restore it.
	 */
	regval = bus_space_read_4(isc->bst, isc->bsh, AGP_I810_PGTBL_CTL);
	if (regval != (isc->gatt->ag_physical | 1)) {
#if 0
		printf("agp_i810_bind_memory: PGTBL_CTL is 0x%x - fixing\n",
		    regval);
#endif
		bus_space_write_4(isc->bst, isc->bsh, AGP_I810_PGTBL_CTL,
		    isc->gatt->ag_physical | 1);
	}

	if (mem->am_type == 2) {
		WRITE4(AGP_I810_GTT + (u_int32_t)(offset >> AGP_PAGE_SHIFT) * 4,
		    mem->am_physical | 1);
		mem->am_offset = offset;
		mem->am_is_bound = 1;
		return (0);
	}

	if (mem->am_type != 1)
		return agp_generic_bind_memory(sc, mem, offset);

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		WRITE4(AGP_I810_GTT +
		    (u_int32_t)(offset >> AGP_PAGE_SHIFT) * 4, i | 3);

	return (0);
}

int
agp_i810_unbind_memory(struct vga_pci_softc *sc, struct agp_memory *mem)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	u_int32_t i;

	if (mem->am_type == 2) {
		WRITE4(AGP_I810_GTT +
		    (u_int32_t)(mem->am_offset >> AGP_PAGE_SHIFT) * 4, 0);
		mem->am_offset = 0;
		mem->am_is_bound = 0;
		return (0);
	}

	if (mem->am_type != 1)
		return (agp_generic_unbind_memory(sc, mem));

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		WRITE4(AGP_I810_GTT + (i >> AGP_PAGE_SHIFT) * 4, 0);

	return (0);
}
