/*	$OpenBSD: agp_i810.c,v 1.11 2006/02/11 21:15:21 matthieu Exp $	*/
/*	$NetBSD: agp_i810.c,v 1.15 2003/01/31 00:07:39 thorpej Exp $	*/

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
#define READ4(off)	bus_space_read_4(isc->bst, isc->bsh, off)
#define WRITE4(off,v)	bus_space_write_4(isc->bst, isc->bsh, off, v)
#define WRITEGTT(off,v)	bus_space_write_4(isc->gtt_bst, isc->gtt_bsh, off, v)

#define CHIP_I810 0	/* i810/i815 */
#define CHIP_I830 1	/* i830/i845 */
#define CHIP_I855 2	/* i852GM/i855GM/i865G */
#define CHIP_I915 3	/* i915G/i915GM */

#define WRITE_GATT(off,v)						   \
	do {								   \
		if (isc->chiptype == CHIP_I915)				   \
			WRITEGTT((u_int32_t)((off) >> AGP_PAGE_SHIFT) * 4, \
			    v);						   \
		else							   \
			WRITE4(AGP_I810_GTT +				   \
			    (u_int32_t)((off) >> AGP_PAGE_SHIFT) * 4, v);  \
	} while (0)

struct agp_i810_softc {
	struct agp_gatt *gatt;
	int chiptype;			/* i810-like or i830 */
	u_int32_t dcache_size;		/* i810 only */
	u_int32_t stolen;		/* number of i830/845 gtt entries
					   for stolen memory */
	bus_space_tag_t bst;		/* bus_space tag */
	bus_space_handle_t bsh;		/* bus_space handle */
	bus_space_tag_t gtt_bst;	/* GATT bus_space tag */
	bus_space_handle_t gtt_bsh;	/* GATT bus_space handle */
	struct pci_attach_args bridge_pa;
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
agp_i810_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa,
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
	memcpy(&isc->bridge_pa, pchb_pa, sizeof *pchb_pa);

	if ((error = agp_map_aperture(sc))) {
		printf(": can't map aperture\n");
		free(isc, M_DEVBUF);
		return (error);
	}

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82810_GC:
	case PCI_PRODUCT_INTEL_82810_DC100_GC:
	case PCI_PRODUCT_INTEL_82810E_GC:
	case PCI_PRODUCT_INTEL_82815_FULL_GRAPH:
		isc->chiptype = CHIP_I810;
		break;
	case PCI_PRODUCT_INTEL_82830MP_IV:
	case PCI_PRODUCT_INTEL_82845G_IGD:
		isc->chiptype = CHIP_I830;
		break;
	case PCI_PRODUCT_INTEL_82852GM_AGP:
	case PCI_PRODUCT_INTEL_82865_IGD:
		isc->chiptype = CHIP_I855;
		break;
	case PCI_PRODUCT_INTEL_82915G_IV:
	case PCI_PRODUCT_INTEL_82915GM_IGD:
		isc->chiptype = CHIP_I915;
		break;
	}

	error = pci_mapreg_map(pa,
	    (isc->chiptype == CHIP_I915) ? AGP_I915_MMADR : AGP_I810_MMADR,
	    PCI_MAPREG_TYPE_MEM, 0, &isc->bst, &isc->bsh, NULL, NULL, 0);
	if (error != 0) {
		printf(": can't map mmadr registers\n");
		return (error);
	}

	if (isc->chiptype == CHIP_I915) {
		error = pci_mapreg_map(pa, AGP_I915_GTTADR, PCI_MAPREG_TYPE_MEM,
		    0, &isc->gtt_bst, &isc->gtt_bsh, NULL, NULL, 0);
		if (error != 0) {
			printf(": can't map gatt registers\n");
			agp_generic_detach(sc);
			return (error);
		}
	}

	gatt = malloc(sizeof(struct agp_gatt), M_DEVBUF, M_NOWAIT);
	if (!gatt) {
 		agp_generic_detach(sc);
 		return (ENOMEM);
	}
	isc->gatt = gatt;

	gatt->ag_entries = AGP_GET_APERTURE(sc) >> AGP_PAGE_SHIFT;

	if (isc->chiptype == CHIP_I810) {
		int dummyseg;
		/* Some i810s have on-chip memory called dcache */
		if (READ1(AGP_I810_DRT) & AGP_I810_DRT_POPULATED)
			isc->dcache_size = 4 * 1024 * 1024;
		else
			isc->dcache_size = 0;

		/* According to the specs the gatt on the i810 must be 64k */
		if (agp_alloc_dmamem(sc->sc_dmat, 64 * 1024,
		    0, &gatt->ag_dmamap, (caddr_t *)&gatt->ag_virtual,
		    &gatt->ag_physical, &gatt->ag_dmaseg, 1, &dummyseg) != 0) {
			free(gatt, M_DEVBUF);
			agp_generic_detach(sc);
			return (ENOMEM);
		}

		gatt->ag_size = gatt->ag_entries * sizeof(u_int32_t);
		memset(gatt->ag_virtual, 0, gatt->ag_size);

		agp_flush_cache();
		/* Install the GATT. */
		WRITE4(AGP_I810_PGTBL_CTL, gatt->ag_physical | 1);
	} else if (isc->chiptype == CHIP_I830) {
		/* The i830 automatically initializes the 128k gatt on boot. */
		pcireg_t reg;
		u_int32_t pgtblctl;
		u_int16_t gcc1;

		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I830_GCC1);
		gcc1 = (u_int16_t)(reg >> 16);
		switch (gcc1 & AGP_I830_GCC1_GMS) {
		case AGP_I830_GCC1_GMS_STOLEN_512:
			isc->stolen = (512 - 132) * 1024 / 4096;
			break;
		case AGP_I830_GCC1_GMS_STOLEN_1024:
			isc->stolen = (1024 - 132) * 1024 / 4096;
			break;
		case AGP_I830_GCC1_GMS_STOLEN_8192:
			isc->stolen = (8192 - 132) * 1024 / 4096;
			break;
		default:
			isc->stolen = 0;
			printf(
			    ": unknown memory configuration, disabling\n");
			agp_generic_detach(sc);
			return (EINVAL);
		}
#ifdef DEBUG
		if (isc->stolen > 0) {
			printf(": detected %dk stolen memory",
			    isc->stolen * 4);
		}
#endif

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	} else if (isc->chiptype == CHIP_I915) {
		/* The 915G automatically initializes the 256k gatt on boot. */
		pcireg_t reg;
		u_int32_t pgtblctl;
		u_int16_t gcc1;

		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I855_GCC1);
		gcc1 = (u_int16_t)(reg >> 16);
		switch (gcc1 & AGP_I855_GCC1_GMS) {
		case AGP_I855_GCC1_GMS_STOLEN_1M:
			isc->stolen = (1024 - 260) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_4M:
			isc->stolen = (4096 - 260) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_8M:
			isc->stolen = (8192 - 260) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_16M:
			isc->stolen = (16384 - 260) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_32M:
			isc->stolen = (32768 - 260) * 1024 / 4096;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_48M:
			isc->stolen = (49152 - 260) * 1024 / 4096;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_64M:
			isc->stolen = (65536 - 260) * 1024 / 4096;
			break;
		default:
			isc->stolen = 0;
			printf(
			    ": unknown memory configuration, disabling\n");
			agp_generic_detach(sc);
			return (EINVAL);
		}
#ifdef DEBUG
		if (isc->stolen > 0) {
			printf(": detected %dk stolen memory",
			    isc->stolen * 4);
		}
#endif

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	} else {	/* CHIP_I855 */
		/* The 855GM automatically initializes the 128k gatt on boot. */
		pcireg_t reg;
		u_int32_t pgtblctl;
		u_int16_t gcc1;

		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I855_GCC1);
		gcc1 = (u_int16_t)(reg >> 16);
		switch (gcc1 & AGP_I855_GCC1_GMS) {
		case AGP_I855_GCC1_GMS_STOLEN_1M:
			isc->stolen = (1024 - 132) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_4M:
			isc->stolen = (4096 - 132) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_8M:
			isc->stolen = (8192 - 132) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_16M:
			isc->stolen = (16384 - 132) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_32M:
			isc->stolen = (32768 - 132) * 1024 / 4096;
			break;
		default:
			isc->stolen = 0;
			printf(
			    ": unknown memory configuration, disabling\n");
			agp_generic_detach(sc);
			return (EINVAL);
		}
#ifdef DEBUG
		if (isc->stolen > 0) {
			printf(": detected %dk stolen memory",
			    isc->stolen * 4);
		}
#endif

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	}

	/*
	 * Make sure the chipset can see everything.
	 */
	agp_flush_cache();

	return (0);
}

u_int32_t
agp_i810_get_aperture(struct vga_pci_softc *sc)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	pcireg_t reg;

	if (isc->chiptype == CHIP_I810) {
		u_int16_t miscc;

		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I810_SMRAM);
		miscc = (u_int16_t)(reg >> 16);
		if ((miscc & AGP_I810_MISCC_WINSIZE) ==
		    AGP_I810_MISCC_WINSIZE_32)
			return (32 * 1024 * 1024);
		else
			return (64 * 1024 * 1024);
	} else if (isc->chiptype == CHIP_I830) {
		u_int16_t gcc1;

		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I830_GCC0);
		gcc1 = (u_int16_t)(reg >> 16);
		if ((gcc1 & AGP_I830_GCC1_GMASIZE) == AGP_I830_GCC1_GMASIZE_64)
			return (64 * 1024 * 1024);
		else
			return (128 * 1024 * 1024);
	} else if (isc->chiptype == CHIP_I915) {
		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I915_MSAC);
		if ((reg & AGP_I915_MSAC_GMASIZE) == AGP_I915_MSAC_GMASIZE_128) {
			return (128 * 1024 * 1024);
		} else {
			return (256 * 1024 * 1024);
		}
	} else {	/* CHIP_I855 */
		return (128 * 1024 * 1024);
	}
}

int
agp_i810_set_aperture(struct vga_pci_softc *sc, u_int32_t aperture)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	pcireg_t reg;

	if (isc->chiptype == CHIP_I810) {
		u_int16_t miscc;

		/*
		 * Double check for sanity.
		 */
		if (aperture != (32 * 1024 * 1024) &&
		    aperture != (64 * 1024 * 1024)) {
			printf("agp: bad aperture size %d\n", aperture);
			return (EINVAL);
		}

		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I810_SMRAM);
		miscc = (u_int16_t)(reg >> 16);
		miscc &= ~AGP_I810_MISCC_WINSIZE;
		if (aperture == 32 * 1024 * 1024)
			miscc |= AGP_I810_MISCC_WINSIZE_32;
		else
			miscc |= AGP_I810_MISCC_WINSIZE_64;

		reg &= 0x0000ffff;
		reg |= ((pcireg_t)miscc) << 16;
		pci_conf_write(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I810_SMRAM, reg);
	} else if (isc->chiptype == CHIP_I830) {
		u_int16_t gcc1;

		if (aperture != (64 * 1024 * 1024) &&
		    aperture != (128 * 1024 * 1024)) {
			printf("agp: bad aperture size %d\n", aperture);
			return (EINVAL);
		}
		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I830_GCC0);
		gcc1 = (u_int16_t)(reg >> 16);
		gcc1 &= ~AGP_I830_GCC1_GMASIZE;
		if (aperture == 64 * 1024 * 1024)
			gcc1 |= AGP_I830_GCC1_GMASIZE_64;
		else
			gcc1 |= AGP_I830_GCC1_GMASIZE_128;

		reg &= 0x0000ffff;
		reg |= ((pcireg_t)gcc1) << 16;
		pci_conf_write(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I830_GCC0, reg);
	} else if (isc->chiptype == CHIP_I915) {
		if (aperture != (128 * 1024 * 1024) &&
		    aperture != (256 * 1024 * 1024)) {
			printf("agp: bad aperture size %d\n", aperture);
			return (EINVAL);
		}
		reg = pci_conf_read(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I915_MSAC);
		reg &= ~AGP_I915_MSAC_GMASIZE;
		if (aperture == (128 * 1024 * 1024))
			reg |= AGP_I915_MSAC_GMASIZE_128;
		else
			reg |= AGP_I915_MSAC_GMASIZE_256;
		pci_conf_write(isc->bridge_pa.pa_pc,
		    isc->bridge_pa.pa_tag, AGP_I915_MSAC, reg);
	} else {	/* CHIP_I855 */
		if (aperture != (128 * 1024 * 1024)) {
			printf("agp: bad aperture size %d\n", aperture);
			return (EINVAL);
		}
	}

	return (0);
}

int
agp_i810_bind_page(struct vga_pci_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_i810_softc *isc = sc->sc_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT)) {
#ifdef DEBUG
		printf("agp: failed: offset 0x%08x, shift %d, entries %d\n",
		    (int)offset, AGP_PAGE_SHIFT,
		    isc->gatt->ag_entries);
#endif
		return (EINVAL);
	}

	if (isc->chiptype != CHIP_I810) {
		if ((offset >> AGP_PAGE_SHIFT) < isc->stolen) {
#ifdef DEBUG
			printf("agp: trying to bind into stolen memory\n");
#endif
			return (EINVAL);
		}
	}

	WRITE_GATT(offset, physical | 1);
	return (0);
}

int
agp_i810_unbind_page(struct vga_pci_softc *sc, off_t offset)
{
	struct agp_i810_softc *isc = sc->sc_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	if (isc->chiptype != CHIP_I810 ) {
		if ((offset >> AGP_PAGE_SHIFT) < isc->stolen) {
#ifdef DEBUG
			printf("agp: trying to unbind from stolen memory\n");
#endif
			return (EINVAL);
		}
	}

	WRITE_GATT(offset, 0);
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

	if ((size & (AGP_PAGE_SIZE - 1)) != 0)
		return 0;

	if (sc->sc_allocated + size > sc->sc_maxmem)
		return 0;

	if (type == 1) {
		/*
		 * Mapping local DRAM into GATT.
		 */
		if (isc->chiptype != CHIP_I810 )
			return (NULL);
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
		/*
		 * Allocate and wire down the page now so that we can
		 * get its physical address.
		 */
		mem->am_dmaseg = malloc(sizeof *mem->am_dmaseg, M_DEVBUF,
		    M_WAITOK);
		if (mem->am_dmaseg == NULL) {
			free(mem, M_DEVBUF);
			return (NULL);
		}
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
#if DEBUG
		printf("agp_i810_bind_memory: PGTBL_CTL is 0x%x - fixing\n",
		    regval);
#endif
		bus_space_write_4(isc->bst, isc->bsh, AGP_I810_PGTBL_CTL,
		    isc->gatt->ag_physical | 1);
	}

	if (mem->am_type == 2) {
		WRITE_GATT(offset, mem->am_physical | 1);
		mem->am_offset = offset;
		mem->am_is_bound = 1;
		return (0);
	}

	if (mem->am_type != 1)
		return (agp_generic_bind_memory(sc, mem, offset));

	if (isc->chiptype != CHIP_I810)
		return (EINVAL);

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
		WRITE4(AGP_I810_GTT +
		    (u_int32_t)(offset >> AGP_PAGE_SHIFT) * 4, i | 3);
	}
	mem->am_is_bound = 1;
	return (0);
}

int
agp_i810_unbind_memory(struct vga_pci_softc *sc, struct agp_memory *mem)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	u_int32_t i;

	if (mem->am_type == 2) {
		WRITE_GATT(mem->am_offset, 0);
		mem->am_offset = 0;
		mem->am_is_bound = 0;
		return (0);
	}

	if (mem->am_type != 1)
		return (agp_generic_unbind_memory(sc, mem));

	if (isc->chiptype != CHIP_I810)
		return (EINVAL);

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		WRITE4(AGP_I810_GTT + (i >> AGP_PAGE_SHIFT) * 4, 0);
	mem->am_is_bound = 0;
	return (0);
}
