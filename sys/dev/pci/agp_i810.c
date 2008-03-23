/*	$OpenBSD: agp_i810.c,v 1.34 2008/03/23 19:54:47 oga Exp $	*/
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

#include "agp_intel.h"

#define READ1(off)	bus_space_read_1(isc->bst, isc->bsh, off)
#define READ4(off)	bus_space_read_4(isc->bst, isc->bsh, off)
#define WRITE4(off,v)	bus_space_write_4(isc->bst, isc->bsh, off, v)

#define WRITEGTT(off,v)	bus_space_write_4(isc->gtt_bst, isc->gtt_bsh, off, v)
#define WRITE_GATT(off,v)	agp_i810_write_gatt(isc, off, v)


enum {
	CHIP_I810	= 0,	/* i810/i815 */
	CHIP_I830	= 1,	/* i830/i845 */
	CHIP_I855	= 2,	/* i852GM/i855GM/i865G */
	CHIP_I915	= 3,	/* i915G/i915GM */
	CHIP_I965	= 4,	/* i965/i965GM */
	CHIP_G33	= 5	/* G33/Q33/Q35 */
};

struct agp_i810_softc {
	u_int32_t	initial_aperture;/* aperture size at startup */
	struct agp_gatt
			*gatt;
	int		chiptype;	/* i810-like or i830 */
	u_int32_t	dcache_size;	/* i810 only */
	u_int32_t	stolen;		/* number of i830/845 gtt entries
					   for stolen memory */
	bus_space_tag_t	bst;		/* bus_space tag */
	bus_space_handle_t bsh;		/* bus_space handle */
	bus_size_t	bsz;			/* bus_space size */
	bus_space_tag_t	gtt_bst;	/* GATT bus_space tag */
	bus_space_handle_t gtt_bsh;	/* GATT bus_space handle */
	struct pci_attach_args vga_pa;
};

int	agp_i810_vgamatch(struct pci_attach_args *);
int	agp_i810_set_aperture(struct agp_softc *, u_int32_t);
int	agp_i810_bind_page(struct agp_softc *, off_t, bus_addr_t);
int	agp_i810_unbind_page(struct agp_softc *, off_t);
void	agp_i810_flush_tlb(struct agp_softc *);
int	agp_i810_enable(struct agp_softc *, u_int32_t mode);
struct agp_memory * agp_i810_alloc_memory(struct agp_softc *, int, vsize_t);
int	agp_i810_free_memory(struct agp_softc *, struct agp_memory *);
int	agp_i810_bind_memory(struct agp_softc *, struct agp_memory *,
	    off_t);
int	agp_i810_unbind_memory(struct agp_softc *, struct agp_memory *);
void	agp_i810_write_gatt(struct agp_i810_softc *, bus_size_t, u_int32_t);

struct agp_methods agp_i810_methods = {
	agp_generic_get_aperture,
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

/* XXXthorpej -- duplicated code (see arch/i386/pci/pchb.c) */
int
agp_i810_vgamatch(struct pci_attach_args *pa)
{

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82810_IGD:
	case PCI_PRODUCT_INTEL_82810_DC100_IGD:
	case PCI_PRODUCT_INTEL_82810E_IGD:
	case PCI_PRODUCT_INTEL_82815_IGD:
	case PCI_PRODUCT_INTEL_82830M_IGD:
	case PCI_PRODUCT_INTEL_82845G_IGD:
	case PCI_PRODUCT_INTEL_82855GM_IGD:
	case PCI_PRODUCT_INTEL_82865G_IGD:
	case PCI_PRODUCT_INTEL_82915G_IGD_1:
	case PCI_PRODUCT_INTEL_82915G_IGD_2:
	case PCI_PRODUCT_INTEL_82915GM_IGD_1:
	case PCI_PRODUCT_INTEL_82915GM_IGD_2:
	case PCI_PRODUCT_INTEL_82945G_IGD_1:
	case PCI_PRODUCT_INTEL_82945G_IGD_2:
	case PCI_PRODUCT_INTEL_82945GM_IGD_1:
	case PCI_PRODUCT_INTEL_82945GM_IGD_2:
	case PCI_PRODUCT_INTEL_82G965_IGD_1:
	case PCI_PRODUCT_INTEL_82G965_IGD_2:
	case PCI_PRODUCT_INTEL_82Q965_IGD_1:
	case PCI_PRODUCT_INTEL_82Q965_IGD_2:
	case PCI_PRODUCT_INTEL_82GM965_IGD_1:
	case PCI_PRODUCT_INTEL_82GM965_IGD_2:
	case PCI_PRODUCT_INTEL_82G33_IGD_1:
	case PCI_PRODUCT_INTEL_82G33_IGD_2:
		return (1);
	}

	return (0);
}

int
agp_i810_attach(struct agp_softc *sc, struct pci_attach_args *pa)
{
	struct agp_i810_softc *isc;
	struct agp_gatt *gatt;
	bus_addr_t mmaddr, gmaddr;
	int error;
	u_int memtype = 0;

	isc = malloc(sizeof *isc, M_AGP, M_NOWAIT | M_ZERO);
	if (isc == NULL) {
		printf("can't allocate chipset-specific softc\n");
		return (ENOMEM);
	}
	sc->sc_chipc = isc;
	sc->sc_methods = &agp_i810_methods;

	if (pci_find_device(&isc->vga_pa, agp_i810_vgamatch) == 0) {
#if NAGP_INTEL > 0

		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_INTEL_82815_HB:
		case PCI_PRODUCT_INTEL_82845G_HB:
		case PCI_PRODUCT_INTEL_82865G_HB:
			return (agp_intel_attach(sc, pa));
		}
#endif
		printf("no integrated graphics\n");
		free(isc, M_AGP);
		return (ENOENT);
	}

	/* XXXfvdl */
	sc->sc_dmat = isc->vga_pa.pa_dmat;

	switch (PCI_PRODUCT(isc->vga_pa.pa_id)) {
	case PCI_PRODUCT_INTEL_82810_IGD:
	case PCI_PRODUCT_INTEL_82810_DC100_IGD:
	case PCI_PRODUCT_INTEL_82810E_IGD:
	case PCI_PRODUCT_INTEL_82815_IGD:
		isc->chiptype = CHIP_I810;
		break;
	case PCI_PRODUCT_INTEL_82830M_IGD:
	case PCI_PRODUCT_INTEL_82845G_IGD:
		isc->chiptype = CHIP_I830;
		break;
	case PCI_PRODUCT_INTEL_82855GM_IGD:
	case PCI_PRODUCT_INTEL_82865G_IGD:
		isc->chiptype = CHIP_I855;
		break;
	case PCI_PRODUCT_INTEL_82915G_IGD_1:
	case PCI_PRODUCT_INTEL_82915G_IGD_2:
	case PCI_PRODUCT_INTEL_82915GM_IGD_1:
	case PCI_PRODUCT_INTEL_82915GM_IGD_2:
	case PCI_PRODUCT_INTEL_82945G_IGD_1:
	case PCI_PRODUCT_INTEL_82945G_IGD_2:
	case PCI_PRODUCT_INTEL_82945GM_IGD_1:
	case PCI_PRODUCT_INTEL_82945GM_IGD_2:
		isc->chiptype = CHIP_I915;
		break;
	case PCI_PRODUCT_INTEL_82Q965_IGD_1:
	case PCI_PRODUCT_INTEL_82Q965_IGD_2:
	case PCI_PRODUCT_INTEL_82G965_IGD_1:
	case PCI_PRODUCT_INTEL_82G965_IGD_2:
	case PCI_PRODUCT_INTEL_82GM965_IGD_1:
	case PCI_PRODUCT_INTEL_82GM965_IGD_2:
		isc->chiptype = CHIP_I965;
		break;
	case PCI_PRODUCT_INTEL_82G33_IGD_1:
	case PCI_PRODUCT_INTEL_82G33_IGD_2:
		isc->chiptype = CHIP_G33;
		break;
	}

	switch (isc->chiptype) {
	case CHIP_I915:
	case CHIP_G33:
		gmaddr = AGP_I915_GMADR;
		mmaddr = AGP_I915_MMADR;
		memtype = PCI_MAPREG_TYPE_MEM;
		break;
	case CHIP_I965:
		gmaddr = AGP_I965_GMADR;
		mmaddr = AGP_I965_MMADR;
		memtype = PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT;
		break;
	default:
		gmaddr = AGP_APBASE;
		mmaddr = AGP_I810_MMADR;
		memtype = PCI_MAPREG_TYPE_MEM;
		break;
	}

	error = agp_map_aperture(&isc->vga_pa, sc, gmaddr, memtype);
	if (error != 0) {
		printf("can't map aperture\n");
		free(isc, M_AGP);
		return (error);
	}

	if (isc->chiptype == CHIP_I965) 
		memtype = pci_mapreg_type(isc->vga_pa.pa_pc,
		    isc->vga_pa.pa_tag, mmaddr);

	error = pci_mapreg_map(&isc->vga_pa, mmaddr, memtype, 0,
	    &isc->bst, &isc->bsh, NULL, &isc->bsz, 0);
	if (error != 0) {
		printf("can't map mmadr registers\n");
		agp_generic_detach(sc);
		return (error);
	}

	if (isc->chiptype == CHIP_I915 || isc->chiptype == CHIP_G33) {
		error = pci_mapreg_map(&isc->vga_pa, AGP_I915_GTTADR, memtype,
		    0, &isc->gtt_bst, &isc->gtt_bsh, NULL, NULL, 0);
		if (error != 0) {
			printf("can't map gatt registers\n");
			agp_generic_detach(sc);
			return (error);
		}
	}

	isc->initial_aperture = AGP_GET_APERTURE(sc);

	gatt = malloc(sizeof(struct agp_gatt), M_AGP, M_NOWAIT);
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
			free(gatt, M_AGP);
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

		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I830_GCC1);
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
			printf("unknown memory configuration, disabling\n");
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
	} else if (isc->chiptype == CHIP_I855 || isc->chiptype == CHIP_I915 ||
		   isc->chiptype == CHIP_I965 || isc->chiptype == CHIP_G33) {
		pcireg_t reg;
		u_int32_t pgtblctl, stolen;
		u_int16_t gcc1;

		/* Stolen memory is set up at the beginning of the aperture by
		 * the BIOS, consisting of the GATT followed by 4kb for the
		 * BIOS display.
		 */

		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I855_GCC1);
		gcc1 = (u_int16_t)(reg >> 16);
                switch (isc->chiptype) {
		case CHIP_I855:
		/* The 855GM automatically initializes the 128k gatt on boot. */
			stolen = 128 + 4;
			break;
                case CHIP_I915:
		/* The 915G automatically initializes the 256k gatt on boot. */
			stolen = 256 + 4;
			break;
		case CHIP_I965:
			switch (READ4(AGP_I810_PGTBL_CTL) &
			    AGP_I810_PGTBL_SIZE_MASK) {
			case AGP_I810_PGTBL_SIZE_512KB:
				stolen = 512 + 4;
				break;
			case AGP_I810_PGTBL_SIZE_256KB:
				stolen = 256 + 4;
				break;
			case AGP_I810_PGTBL_SIZE_128KB:
			default:
				stolen = 128 + 4;
				break;
			}
			break;
		case CHIP_G33:
			switch (gcc1 & AGP_G33_PGTBL_SIZE_MASK) {
			case AGP_G33_PGTBL_SIZE_2M:
				stolen = 2048 + 4;
				break;
			case AGP_G33_PGTBL_SIZE_1M:
			default:
				stolen = 1024 + 4;
				break;
			}
			break;
		default:
			printf("bad chiptype\n");
			agp_generic_detach(sc);
			return (EINVAL);
		}

		switch (gcc1 & AGP_I855_GCC1_GMS) {
		case AGP_I855_GCC1_GMS_STOLEN_1M:
			isc->stolen = (1024 - stolen) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_4M:
			isc->stolen = (4096 - stolen) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_8M:
			isc->stolen = (8192 - stolen) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_16M:
			isc->stolen = (16384 - stolen) * 1024 / 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_32M:
			isc->stolen = (32768 - stolen) * 1024 / 4096;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_48M:
			isc->stolen = (49152 - stolen) * 1024 / 4096;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_64M:
			isc->stolen = (65536 - stolen) * 1024 / 4096;
			break;
		case AGP_G33_GCC1_GMS_STOLEN_128M:
			isc->stolen = (131072 - stolen) * 1024 / 4096;
			break;
		case AGP_G33_GCC1_GMS_STOLEN_256M:
			isc->stolen = (262144 - stolen) * 1024 / 4096;
			break;
		default:
			isc->stolen = 0;
			printf("unknown memory configuration, disabling\n");
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

#if 0
int
agp_i810_detach(struct agp_softc *sc)
{
	int error;
	struct agp_i810_softc *isc = sc->sc_chipc;

	error = agp_generic_detach(sc);
	if (error)
		return (error);

	/* Clear the GATT base. */
	if (sc->chiptype == CHIP_I810) {
		WRITE4(AGP_I810_PGTBL_CTL, 0);
	} else {
		unsigned int pgtblctl;
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl &= ~1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);
	}

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(sc, isc->initial_aperture);

	if (sc->chiptype == CHIP_I810) {
		agp_free_dmamem(sc->sc_dmat, gatt->ag_size, gatt->ag_dmamap,
		    (void *)gatt->ag_virtual, &gatt->ag_dmaseg, 1);
	}
	free(sc->gatt, M_AGP);

	return (0);
}
#endif

int
agp_i810_set_aperture(struct agp_softc *sc, u_int32_t aperture)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	pcireg_t reg;
	u_int16_t gcc1, miscc;

	switch (isc->chiptype) {
	case CHIP_I810:
		/*
		 * Double check for sanity.
		 */
		if (aperture != (32 * 1024 * 1024) &&
		    aperture != (64 * 1024 * 1024)) {
			printf("agp: bad aperture size %d\n", aperture);
			return (EINVAL);
		}

		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I810_SMRAM);
		miscc = (u_int16_t)(reg >> 16);
		miscc &= ~AGP_I810_MISCC_WINSIZE;
		if (aperture == 32 * 1024 * 1024)
			miscc |= AGP_I810_MISCC_WINSIZE_32;
		else
			miscc |= AGP_I810_MISCC_WINSIZE_64;

		reg &= 0x0000ffff;
		reg |= ((pcireg_t)miscc) << 16;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_I810_SMRAM, reg);
		break;
	case CHIP_I830:
		if (aperture != (64 * 1024 * 1024) &&
		    aperture != (128 * 1024 * 1024)) {
			printf("agp: bad aperture size %d\n", aperture);
			return (EINVAL);
		}
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, AGP_I830_GCC0);
		gcc1 = (u_int16_t)(reg >> 16);
		gcc1 &= ~AGP_I830_GCC1_GMASIZE;
		if (aperture == 64 * 1024 * 1024)
			gcc1 |= AGP_I830_GCC1_GMASIZE_64;
		else
			gcc1 |= AGP_I830_GCC1_GMASIZE_128;

		reg &= 0x0000ffff;
		reg |= ((pcireg_t)gcc1) << 16;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, AGP_I830_GCC0, reg);
		break;
	case CHIP_I855:
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
		return agp_generic_set_aperture(sc, aperture);
	}

	return (0);
}

int
agp_i810_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
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

	WRITE_GATT(offset, physical);
	return (0);
}

int
agp_i810_unbind_page(struct agp_softc *sc, off_t offset)
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
agp_i810_flush_tlb(struct agp_softc *sc)
{
}

int
agp_i810_enable(struct agp_softc *sc, u_int32_t mode)
{
	return (0);
}

struct agp_memory *
agp_i810_alloc_memory(struct agp_softc *sc, int type, vsize_t size)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	struct agp_memory *mem;
	int error;

	if ((size & (AGP_PAGE_SIZE - 1)) != 0)
		return (0);

	if (sc->sc_allocated + size > sc->sc_maxmem)
		return (NULL);

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
		 * Bogus mapping of 1 or 4 pages for the hardware cursor.
		 */
		if (size != AGP_PAGE_SIZE && size != 4 * AGP_PAGE_SIZE) {
			printf("agp: trying to map %lu for hw cursor\n", size);
			return (NULL);
		}
	}

	mem = malloc(sizeof *mem, M_AGP, M_WAITOK | M_ZERO);
	mem->am_id = sc->sc_nextid++;
	mem->am_size = size;
	mem->am_type = type;

	if (type == 2) {
		/*
		 * Allocate and wire down the pages now so that we can
		 * get their physical address.
		 */
		mem->am_dmaseg = malloc(sizeof *mem->am_dmaseg, M_AGP,
		    M_WAITOK);
		if ((error = agp_alloc_dmamem(sc->sc_dmat, size, 0,
		    &mem->am_dmamap, &mem->am_virtual, &mem->am_physical,
		    mem->am_dmaseg, 1, &mem->am_nseg)) != 0) {
			free(mem->am_dmaseg, M_AGP);
			free(mem, M_AGP);
			printf("agp: agp_alloc_dmamem(%d)\n", error);
			return (NULL);
		}
		memset(mem->am_virtual, 0, size);
	} else if (type != 1) {
		if ((error = bus_dmamap_create(sc->sc_dmat, size,
		    size / PAGE_SIZE + 1, size, 0, BUS_DMA_NOWAIT,
		    &mem->am_dmamap)) != 0) {
			free(mem, M_AGP);
			printf("agp: bus_dmamap_create(%d)\n", error);
			return (NULL);
		}
	}

	TAILQ_INSERT_TAIL(&sc->sc_memory, mem, am_link);
	sc->sc_allocated += size;

	return (mem);
}

int
agp_i810_free_memory(struct agp_softc *sc, struct agp_memory *mem)
{
	if (mem->am_is_bound)
		return (EBUSY);

	if (mem->am_type == 2) {
		agp_free_dmamem(sc->sc_dmat, mem->am_size, mem->am_dmamap,
		    mem->am_virtual, mem->am_dmaseg, mem->am_nseg);
		free(mem->am_dmaseg, M_AGP);
	}

	sc->sc_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->sc_memory, mem, am_link);
	free(mem, M_AGP);
	return (0);
}

int
agp_i810_bind_memory(struct agp_softc *sc, struct agp_memory *mem,
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
		for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
			WRITE_GATT(offset + i, (mem->am_physical + i));
		}
		mem->am_offset = offset;
		mem->am_is_bound = 1;
		return (0);
	}

	if (mem->am_type != 1)
		return (agp_generic_bind_memory(sc, mem, offset));

	if (isc->chiptype != CHIP_I810)
		return (EINVAL);

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		WRITE4(AGP_I810_GTT +
		    (u_int32_t)(offset >> AGP_PAGE_SHIFT) * 4, i | 3);
	mem->am_is_bound = 1;
	return (0);
}

int
agp_i810_unbind_memory(struct agp_softc *sc, struct agp_memory *mem)
{
	struct agp_i810_softc *isc = sc->sc_chipc;
	u_int32_t i;

	if (mem->am_type == 2) {
		for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
			WRITE_GATT(mem->am_offset + i, 0);
		}
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

void
agp_i810_write_gatt(struct agp_i810_softc *isc, bus_size_t off, u_int32_t v)
{
	u_int32_t d;

	d = v | 1;

	if (isc->chiptype == CHIP_I915 || isc->chiptype == CHIP_G33)
		WRITEGTT((u_int32_t)((off) >> AGP_PAGE_SHIFT) * 4, v ? d : 0);
	else if (isc->chiptype == CHIP_I965) {
		d |= (v & 0x0000000f00000000ULL) >> 28;
		WRITE4(AGP_I965_GTT +
		    (u_int32_t)((off) >> AGP_PAGE_SHIFT) * 4, v ? d : 0);
	} else
		WRITE4(AGP_I810_GTT +
		    (u_int32_t)((off) >> AGP_PAGE_SHIFT) * 4, v ? d : 0);
}
