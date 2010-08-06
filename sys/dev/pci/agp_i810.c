/*	$OpenBSD: agp_i810.c,v 1.67 2010/08/06 13:09:52 oga Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
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

#define READ1(off)	bus_space_read_1(isc->map->bst, isc->map->bsh, off)
#define READ4(off)	bus_space_read_4(isc->map->bst, isc->map->bsh, off)
#define WRITE4(off,v)	bus_space_write_4(isc->map->bst, isc->map->bsh, off, v)

/*
 * Intel IGP gtt bits.
 */
/* PTE is enabled */
#define	INTEL_ENABLED	0x1
/* I810/I815 only, memory is in dcache */
#define	INTEL_LOCAL	0x2
/* Memory is snooped, must not be accessed through gtt from the cpu. */
#define	INTEL_COHERENT	0x6	

enum {
	CHIP_NONE	= 0,	/* not integrated graphics */
	CHIP_I810	= 1,	/* i810/i815 */
	CHIP_I830	= 2,	/* i830/i845 */
	CHIP_I855	= 3,	/* i852GM/i855GM/i865G */
	CHIP_I915	= 4,	/* i915G/i915GM */
	CHIP_I965	= 5,	/* i965/i965GM */
	CHIP_G33	= 6,	/* G33/Q33/Q35 */
	CHIP_G4X	= 7,	/* G4X */
	CHIP_PINEVIEW	= 8,	/* Pineview/Pineview M */
	CHIP_IRONLAKE	= 9,	/* Clarkdale/Arrandale */
};

struct agp_i810_softc {
	struct device		 dev;
	bus_dma_segment_t	 scrib_seg;
	struct agp_softc	*agpdev;
	struct agp_gatt		*gatt;
	struct vga_pci_bar	*map;
	struct vga_pci_bar	*gtt_map;
	u_int32_t		*gtt_backup;	/* saved gtt for suspend */
	bus_dmamap_t		 scrib_dmamap;
	bus_addr_t		 isc_apaddr;
	bus_size_t		 isc_apsize;	/* current aperture size */
	int			 chiptype;	/* i810-like or i830 */
	u_int32_t		 dcache_size;	/* i810 only */
	u_int32_t		 stolen;	/* number of i830/845 gtt
						   entries for stolen memory */
};

void	agp_i810_attach(struct device *, struct device *, void *);
int	agp_i810_activate(struct device *arg, int act);
void	agp_i810_configure(struct agp_i810_softc *);
int	agp_i810_probe(struct device *, void *, void *);
int	agp_i810_get_chiptype(struct pci_attach_args *);
void	agp_i810_bind_page(void *, bus_size_t, paddr_t, int);
void	agp_i810_unbind_page(void *, bus_size_t);
void	agp_i810_flush_tlb(void *);
int	agp_i810_enable(void *, u_int32_t mode);
struct agp_memory * agp_i810_alloc_memory(void *, int, vsize_t);
int	agp_i810_free_memory(void *, struct agp_memory *);
int	agp_i810_bind_memory(void *, struct agp_memory *, bus_size_t);
int	agp_i810_unbind_memory(void *, struct agp_memory *);
void	intagp_write_gtt(struct agp_i810_softc *, bus_size_t, paddr_t);
int	intagp_gmch_match(struct pci_attach_args *);

extern void	intagp_dma_sync(bus_dma_tag_t, bus_dmamap_t,
		    bus_addr_t, bus_size_t, int);

struct cfattach intagp_ca = {
	sizeof(struct agp_i810_softc), agp_i810_probe, agp_i810_attach,
	NULL, agp_i810_activate,
};

struct cfdriver intagp_cd = {
	NULL, "intagp", DV_DULL
};

struct agp_methods agp_i810_methods = {
	agp_i810_bind_page,
	agp_i810_unbind_page,
	agp_i810_flush_tlb,
	intagp_dma_sync,
	agp_i810_enable,
	agp_i810_alloc_memory,
	agp_i810_free_memory,
	agp_i810_bind_memory,
	agp_i810_unbind_memory,
};

int
agp_i810_get_chiptype(struct pci_attach_args *pa)
{
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82810_IGD:
	case PCI_PRODUCT_INTEL_82810_DC100_IGD:
	case PCI_PRODUCT_INTEL_82810E_IGD:
	case PCI_PRODUCT_INTEL_82815_IGD:
		return (CHIP_I810);
		break;
	case PCI_PRODUCT_INTEL_82830M_IGD:
	case PCI_PRODUCT_INTEL_82845G_IGD:
		return (CHIP_I830);
		break;
	case PCI_PRODUCT_INTEL_82855GM_IGD:
	case PCI_PRODUCT_INTEL_82865G_IGD:
		return (CHIP_I855);
		break;
	case PCI_PRODUCT_INTEL_82915G_IGD_1:
	case PCI_PRODUCT_INTEL_82915G_IGD_2:
	case PCI_PRODUCT_INTEL_82915GM_IGD_1:
	case PCI_PRODUCT_INTEL_82915GM_IGD_2:
	case PCI_PRODUCT_INTEL_82945G_IGD_1:
	case PCI_PRODUCT_INTEL_82945G_IGD_2:
	case PCI_PRODUCT_INTEL_82945GM_IGD_1:
	case PCI_PRODUCT_INTEL_82945GM_IGD_2:
	case PCI_PRODUCT_INTEL_82945GME_IGD_1:
		return (CHIP_I915);
		break;
	case PCI_PRODUCT_INTEL_82946GZ_IGD_1:
	case PCI_PRODUCT_INTEL_82946GZ_IGD_2:
	case PCI_PRODUCT_INTEL_82Q965_IGD_1:
	case PCI_PRODUCT_INTEL_82Q965_IGD_2:
	case PCI_PRODUCT_INTEL_82G965_IGD_1:
	case PCI_PRODUCT_INTEL_82G965_IGD_2:
	case PCI_PRODUCT_INTEL_82GM965_IGD_1:
	case PCI_PRODUCT_INTEL_82GM965_IGD_2:
	case PCI_PRODUCT_INTEL_82GME965_IGD_1:
	case PCI_PRODUCT_INTEL_82GME965_IGD_2:
	case PCI_PRODUCT_INTEL_82G35_IGD_1:
	case PCI_PRODUCT_INTEL_82G35_IGD_2:
		return (CHIP_I965);
		break;
	case PCI_PRODUCT_INTEL_82G33_IGD_1:
	case PCI_PRODUCT_INTEL_82G33_IGD_2:
	case PCI_PRODUCT_INTEL_82Q35_IGD_1:
	case PCI_PRODUCT_INTEL_82Q35_IGD_2:
		return (CHIP_G33);
		break;
	case PCI_PRODUCT_INTEL_82GM45_IGD_1:
	case PCI_PRODUCT_INTEL_82Q45_IGD_1:
	case PCI_PRODUCT_INTEL_82G45_IGD_1:
	case PCI_PRODUCT_INTEL_82G41_IGD_1:
		return (CHIP_G4X);
	case PCI_PRODUCT_INTEL_PINEVIEW_IGC_1:
	case PCI_PRODUCT_INTEL_PINEVIEW_M_IGC_1:
		return (CHIP_PINEVIEW);
	case PCI_PRODUCT_INTEL_CLARKDALE_IGD:
	case PCI_PRODUCT_INTEL_ARRANDALE_IGD:
		return (CHIP_IRONLAKE);
		break;
	}
	return (CHIP_NONE);
}

/*
 * We're intel IGD, bus 0 function 0 dev 0 should be the GMCH, so it should
 * be Intel
 */
int
intagp_gmch_match(struct pci_attach_args *pa)
{
	if (pa->pa_bus == 0 && pa->pa_device == 0 && pa->pa_function == 0 &&
	    PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_HOST)
		return (1);
	return (0);
}

int
agp_i810_probe(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args	*pa = aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA)
		return (0);

	return (agp_i810_get_chiptype(pa) != CHIP_NONE);
}

void
agp_i810_attach(struct device *parent, struct device *self, void *aux)
{
	struct agp_i810_softc		*isc = (struct agp_i810_softc *)self;
	struct agp_gatt 		*gatt;
	struct pci_attach_args		*pa = aux, bpa;
	struct vga_pci_softc		*vga = (struct vga_pci_softc *)parent;
	bus_addr_t			 mmaddr, gmaddr, tmp;
	pcireg_t			 memtype, reg;
	u_int32_t			 stolen;
	u_int16_t			 gcc1;

	isc->chiptype = agp_i810_get_chiptype(pa);

	switch (isc->chiptype) {
	case CHIP_I915:
	case CHIP_G33:
	case CHIP_PINEVIEW:
		gmaddr = AGP_I915_GMADR;
		mmaddr = AGP_I915_MMADR;
		memtype = PCI_MAPREG_TYPE_MEM;
		break;
	case CHIP_I965:
	case CHIP_G4X:
	case CHIP_IRONLAKE:
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

	if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, gmaddr, memtype,
	    &isc->isc_apaddr, &isc->isc_apsize, NULL) != 0) {
		printf("can't get aperture info\n");
		return;
	}

	isc->map = vga_pci_bar_map(vga, mmaddr, 0, BUS_SPACE_MAP_LINEAR);
	if (isc->map == NULL) {
		printf("can't map mmadr registers\n");
		return;
	}

	if (isc->chiptype == CHIP_I915 || isc->chiptype == CHIP_G33 ||
	    isc->chiptype == CHIP_PINEVIEW) {
		isc->gtt_map = vga_pci_bar_map(vga, AGP_I915_GTTADR, 0,
		    BUS_SPACE_MAP_LINEAR);
		if (isc->gtt_map == NULL) {
			printf("can't map gatt registers\n");
			goto out;
		}
	}

	gatt = malloc(sizeof(*gatt), M_AGP, M_NOWAIT | M_ZERO);
	if (gatt == NULL) {
		printf("can't alloc gatt\n");
		goto out;
	}
	isc->gatt = gatt;

	gatt->ag_entries = isc->isc_apsize >> AGP_PAGE_SHIFT;

	/*
	 * Find the GMCH, some of the registers we need to read for
	 * configuration purposes are on there. it's always at
	 * 0/0/0 (bus/dev/func).
	 */
	if (pci_find_device(&bpa, intagp_gmch_match) == 0) {
		printf("can't find GMCH\n");
		goto out;
	}

	/*
	 * Backup array to save gtt contents on suspend since we may lose
	 * BAR contents. Most agp drivers do not have this problem since the
	 * GTT ptes are in dma memory.
	 */
	isc->gtt_backup = malloc(sizeof(*isc->gtt_backup) *
	    (isc->isc_apsize / 4096), M_AGP, M_NOWAIT | M_ZERO); 

	switch (isc->chiptype) {
	case CHIP_I810:
		/* Some i810s have on-chip memory called dcache */
		if (READ1(AGP_I810_DRT) & AGP_I810_DRT_POPULATED)
			isc->dcache_size = 4 * 1024 * 1024;
		else
			isc->dcache_size = 0;

		/* According to the specs the gatt on the i810 must be 64k */
		if (agp_alloc_dmamem(pa->pa_dmat, 64 * 1024, &gatt->ag_dmamap,
		    &gatt->ag_physical, &gatt->ag_dmaseg) != 0) {
			goto out;
		}
		gatt->ag_size = gatt->ag_entries * sizeof(u_int32_t);

		if (bus_dmamem_map(pa->pa_dmat, &gatt->ag_dmaseg, 1, 64 * 1024,
		    (caddr_t *)&gatt->ag_virtual, BUS_DMA_NOWAIT) != 0)
			goto out;
		break;

	case CHIP_I830:
		/* The i830 automatically initializes the 128k gatt on boot. */

		reg = pci_conf_read(bpa.pa_pc, bpa.pa_tag, AGP_I830_GCC0);
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
			goto out;
		}
#ifdef DEBUG
		if (isc->stolen > 0) {
			printf(": detected %dk stolen memory",
			    isc->stolen * 4);
		} else
			printf(": no preallocated video memory\n");
#endif

		/* GATT address is already in there, make sure it's enabled */
		gatt->ag_physical = READ4(AGP_I810_PGTBL_CTL) & ~1;
		break;

	case CHIP_I855:
		/* FALLTHROUGH */
	case CHIP_I915:
		/* FALLTHROUGH */
	case CHIP_I965: 
		/* FALLTHROUGH */
	case CHIP_G33:
		/* FALLTHROUGH */
	case CHIP_G4X:
	case CHIP_PINEVIEW:
	case CHIP_IRONLAKE:

		/* Stolen memory is set up at the beginning of the aperture by
		 * the BIOS, consisting of the GATT followed by 4kb for the
		 * BIOS display.
		 */

		reg = pci_conf_read(bpa.pa_pc, bpa.pa_tag, AGP_I855_GCC1);
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
		case CHIP_G4X:
		case CHIP_PINEVIEW:
		case CHIP_IRONLAKE:
			/*
			 * GTT stolen is separate from graphics stolen on
			 * 4 series hardware. so ignore it in stolen gtt entries
			 * counting. However, 4Kb of stolen memory isn't mapped
			 * to the GTT.
			 */
			stolen = 4;
			break;
		default:
			printf("bad chiptype\n");
			goto out;
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
		case AGP_INTEL_GMCH_GMS_STOLEN_96M:
			isc->stolen = (98304 - stolen) * 1024 / 4096;
			break;
		case AGP_INTEL_GMCH_GMS_STOLEN_160M:
			isc->stolen = (163840 - stolen) * 1024 / 4096;
			break;
		case AGP_INTEL_GMCH_GMS_STOLEN_224M:
			isc->stolen = (229376 - stolen) * 1024 / 4096;
			break;
		case AGP_INTEL_GMCH_GMS_STOLEN_352M:
			isc->stolen = (360448 - stolen) * 1024 / 4096;
			break;
		default:
			isc->stolen = 0;
			printf("unknown memory configuration, disabling\n");
			goto out;
		}
#ifdef DEBUG
		if (isc->stolen > 0) {
			printf(": detected %dk stolen memory",
			    isc->stolen * 4);
		} else
			printf(": no preallocated video memory\n");
#endif

		/* GATT address is already in there, make sure it's enabled */
		gatt->ag_physical = READ4(AGP_I810_PGTBL_CTL) & ~1;
		break;

	default:
		printf(": unknown initialisation\n");
		return;
	}
	/* Intel recommends that you have a fake page bound to the gtt always */
	if (agp_alloc_dmamem(pa->pa_dmat, AGP_PAGE_SIZE, &isc->scrib_dmamap,
	    &tmp, &isc->scrib_seg) != 0) {
		printf(": can't get scribble page\n");
		return;
	}
	agp_i810_configure(isc);

	isc->agpdev = (struct agp_softc *)agp_attach_bus(pa, &agp_i810_methods,
	    isc->isc_apaddr, isc->isc_apsize, &isc->dev);
	return;
out:

	if (isc->gatt) {
		if (isc->gatt->ag_size != 0)
			agp_free_dmamem(pa->pa_dmat, isc->gatt->ag_size,
			    isc->gatt->ag_dmamap, &isc->gatt->ag_dmaseg);
		free(isc->gatt, M_AGP);
	}
	if (isc->gtt_map != NULL)
		vga_pci_bar_unmap(isc->gtt_map);
	if (isc->map != NULL)
		vga_pci_bar_unmap(isc->map);
}

int
agp_i810_activate(struct device *arg, int act)
{
	struct agp_i810_softc *isc = (struct agp_i810_softc *)arg;
	bus_space_tag_t bst = isc->map->bst;
	bus_space_handle_t bsh = isc->map->bsh;
	bus_size_t offset;

	if (isc->chiptype == CHIP_I915 ||
	    isc->chiptype == CHIP_G33 ||
	    isc->chiptype == CHIP_PINEVIEW) {
		bst = isc->gtt_map->bst;
		bsh = isc->gtt_map->bsh;
	}

	switch(isc->chiptype) {
	case CHIP_I915:
	case CHIP_G33:
	case CHIP_PINEVIEW:
		offset = 0;
		break;
	case CHIP_I965:
		offset = AGP_I965_GTT;
		break;
	case CHIP_G4X:
	case CHIP_IRONLAKE:
		offset = AGP_G4X_GTT;
		break;
	default:
		offset = AGP_I810_GTT;
		break;
	}

	switch (act) {
	case DVACT_SUSPEND:
		/*
		 * most agp-like drivers have the GTT ptes in dma memory, so
		 * just need the setup to be repeated on resume.
		 * in this case the gtt is held in a BAR, and thus we should
		 * restore the data on resume to make sure that we
		 * don't lose any state that we are depending on.
		 */
		if (isc->gtt_backup != NULL) {
			bus_space_read_region_4(bst, bsh, offset,
			    isc->gtt_backup, isc->isc_apsize / 4096);
		}
		break;
	case DVACT_RESUME:
		agp_i810_configure(isc);
		if (isc->gtt_backup != NULL) {
			bus_space_write_region_4(bst, bsh, offset,
			    isc->gtt_backup, isc->isc_apsize / 4096);
		}
		break;
	}

	return (0);
}
void
agp_i810_configure(struct agp_i810_softc *isc)
{
	bus_addr_t	tmp;

	tmp = isc->isc_apaddr;
	if (isc->chiptype == CHIP_I810) {
		tmp += isc->dcache_size;
	} else {  
		tmp += isc->stolen << AGP_PAGE_SHIFT;
	}

	agp_flush_cache();
	/* Install the GATT. */
	WRITE4(AGP_I810_PGTBL_CTL, isc->gatt->ag_physical | 1);

	/* initialise all gtt entries to point to scribble page */
	for (; tmp < (isc->isc_apaddr + isc->isc_apsize);
	    tmp += AGP_PAGE_SIZE)
		agp_i810_unbind_page(isc, tmp);
	/* XXX we'll need to restore the GTT contents when we go kms */

	/*
	 * Make sure the chipset can see everything.
	 */
	agp_flush_cache();
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

	if (sc->chiptype == CHIP_I810) {
		bus_dmamem_unmap(pa->pa_dmat, isc->gatt->ag_virtual,
		    gatt->ag_size);
		agp_free_dmamem(sc->sc_dmat, gatt->ag_size, gatt->ag_dmamap,
		    &gatt->ag_dmaseg);
	}
	free(sc->gatt, M_AGP);

	return (0);
}
#endif

void
agp_i810_bind_page(void *sc, bus_addr_t offset, paddr_t physical, int flags)
{
	struct agp_i810_softc *isc = sc;
	/*
	 * COHERENT mappings mean set the snoop bit. this should never be
	 * accessed by the gpu through the gtt.
	 */
	if (flags & BUS_DMA_COHERENT)
		physical |= INTEL_COHERENT;

	intagp_write_gtt(isc, offset - isc->isc_apaddr, physical);
}

void
agp_i810_unbind_page(void *sc, bus_size_t offset)
{
	struct agp_i810_softc *isc = sc;

	intagp_write_gtt(isc, offset - isc->isc_apaddr,
	    isc->scrib_dmamap->dm_segs[0].ds_addr);
}

/*
 * Writing via memory mapped registers already flushes all TLBs.
 */
void
agp_i810_flush_tlb(void *sc)
{
}

int
agp_i810_enable(void *sc, u_int32_t mode)
{
	return (0);
}

struct agp_memory *
agp_i810_alloc_memory(void *softc, int type, vsize_t size)
{
	struct agp_i810_softc	*isc = softc;
	struct agp_softc	*sc = isc->agpdev;
	struct agp_memory	*mem;
	int			 error;

	if ((size & (AGP_PAGE_SIZE - 1)) != 0)
		return (NULL);

	if (sc->sc_allocated + size > sc->sc_maxmem)
		return (NULL);

	if (type == 1) {
		/*
		 * Mapping local DRAM into GATT.
		 */
		if (isc->chiptype != CHIP_I810 || size != isc->dcache_size)
			return (NULL);
	} else if (type == 2) {
		/*
		 * Bogus mapping of 1 or 4 pages for the hardware cursor.
		 */
		if (size != AGP_PAGE_SIZE && size != 4 * AGP_PAGE_SIZE) {
#ifdef DEBUG
			printf("agp: trying to map %lu for hw cursor\n", size);
#endif
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
		if ((mem->am_dmaseg = malloc(sizeof (*mem->am_dmaseg), M_AGP,
		    M_WAITOK | M_CANFAIL)) == NULL) {
			free(mem, M_AGP);
			return (NULL);
		}

		if ((error = agp_alloc_dmamem(sc->sc_dmat, size,
		    &mem->am_dmamap, &mem->am_physical, mem->am_dmaseg)) != 0) {
			free(mem->am_dmaseg, M_AGP);
			free(mem, M_AGP);
			printf("agp: agp_alloc_dmamem(%d)\n", error);
			return (NULL);
		}
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
agp_i810_free_memory(void *softc, struct agp_memory *mem)
{
	struct agp_i810_softc	*isc = softc;
	struct agp_softc	*sc = isc->agpdev;

	if (mem->am_is_bound)
		return (EBUSY);

	if (mem->am_type == 2) {
		agp_free_dmamem(sc->sc_dmat, mem->am_size, mem->am_dmamap,
		    mem->am_dmaseg);
		free(mem->am_dmaseg, M_AGP);
	} else if (mem->am_type != 1) {
		bus_dmamap_destroy(sc->sc_dmat, mem->am_dmamap);
	}

	sc->sc_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->sc_memory, mem, am_link);
	free(mem, M_AGP);
	return (0);
}

int
agp_i810_bind_memory(void *sc, struct agp_memory *mem, bus_size_t offset)
{
	struct agp_i810_softc	*isc = sc;
	u_int32_t 		 regval, i;

	if (mem->am_is_bound != 0)
		return (EINVAL);

	if (isc->chiptype != CHIP_I810 && (offset >> AGP_PAGE_SHIFT) <
	    isc->stolen) {
#ifdef DEBUG
		printf("agp: trying to bind into stolen memory\n");
#endif
		return (EINVAL);
	}

	/*
	 * XXX evil hack: the PGTBL_CTL appearently gets overwritten by the
	 * X server for mysterious reasons which leads to crashes if we write
	 * to the GTT through the MMIO window.
	 * Until the issue is solved, simply restore it.
	 */
	regval = READ4(AGP_I810_PGTBL_CTL);
	if (regval != (isc->gatt->ag_physical | 1)) {
		printf("agp_i810_bind_memory: PGTBL_CTL is 0x%x - fixing\n",
		    regval);
		WRITE4(AGP_I810_PGTBL_CTL, isc->gatt->ag_physical |
		    INTEL_ENABLED);
	}

	if (mem->am_type == 2) {
		for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
			agp_i810_bind_page(isc, isc->isc_apaddr + offset + i,
			    mem->am_physical + i, 0);
		mem->am_offset = offset;
		mem->am_is_bound = 1;
		return (0);
	}

	if (mem->am_type != 1)
		return (agp_generic_bind_memory(isc->agpdev, mem, offset));

	if (isc->chiptype != CHIP_I810)
		return (EINVAL);

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		intagp_write_gtt(isc, i, i | INTEL_ENABLED | INTEL_LOCAL);
	mem->am_is_bound = 1;
	return (0);
}

int
agp_i810_unbind_memory(void *sc, struct agp_memory *mem)
{
	struct agp_i810_softc	*isc = sc;
	u_int32_t		 i;

	if (mem->am_is_bound == 0)
		return (EINVAL);

	if (mem->am_type == 2) {
		for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
			agp_i810_unbind_page(isc, isc->isc_apaddr +
			    mem->am_offset + i);
		mem->am_offset = 0;
		mem->am_is_bound = 0;
		return (0);
	}

	if (mem->am_type != 1)
		return (agp_generic_unbind_memory(isc->agpdev, mem));

	if (isc->chiptype != CHIP_I810)
		return (EINVAL);

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		intagp_write_gtt(isc, i, 0);
	mem->am_is_bound = 0;
	return (0);
}

void
intagp_write_gtt(struct agp_i810_softc *isc, bus_size_t off, paddr_t v)
{
	u_int32_t	pte = 0;
	bus_size_t	baseoff, wroff;

	if (isc->chiptype != CHIP_I810 &&
	    (off >> AGP_PAGE_SHIFT) < isc->stolen) {
		printf("intagp: binding into stolen memory!\n");
	}

	if (v != 0) {
		pte = v | INTEL_ENABLED;
		/* 965+ can do 36-bit addressing, add in the extra bits */
		if (isc->chiptype == CHIP_I965 ||
		    isc->chiptype == CHIP_G4X ||
		    isc->chiptype == CHIP_PINEVIEW ||
		    isc->chiptype == CHIP_G33 ||
		    isc->chiptype == CHIP_IRONLAKE) {
			pte |= (v & 0x0000000f00000000ULL) >> 28;
		}
	}

	wroff = (off >> AGP_PAGE_SHIFT) * 4;

	switch(isc->chiptype) {
	case CHIP_I915:
		/* FALLTHROUGH */
	case CHIP_G33:
	case CHIP_PINEVIEW:
		bus_space_write_4(isc->gtt_map->bst, isc->gtt_map->bsh,
		    wroff, pte);
		return;
	case CHIP_I965:
		baseoff = AGP_I965_GTT;
		break;
	case CHIP_G4X:
	case CHIP_IRONLAKE:
		baseoff = AGP_G4X_GTT;
		break;
	default:
		baseoff = AGP_I810_GTT;
		break;
	}
	bus_space_write_4(isc->map->bst, isc->map->bsh, baseoff + wroff, pte);
}
