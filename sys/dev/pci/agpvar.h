/*	$OpenBSD: agpvar.h,v 1.2 2002/07/15 13:23:48 mickey Exp $	*/
/*	$NetBSD: agpvar.h,v 1.4 2001/10/01 21:54:48 fvdl Exp $	*/

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
 *	$FreeBSD: src/sys/pci/agppriv.h,v 1.3 2000/07/12 10:13:04 dfr Exp $
 */

#ifndef _PCI_AGPVAR_H_
#define _PCI_AGPVAR_H_

#include <sys/lock.h>
#include <dev/pci/vga_pcivar.h>

/*
 * This structure is used to query the state of the AGP system.
 */
struct agp_info {
	u_int32_t	ai_mode;
	bus_addr_t	ai_aperture_base;
	bus_size_t	ai_aperture_size;
	vsize_t		ai_memory_allowed;
	vsize_t		ai_memory_used;
	u_int32_t	ai_devid;
};

struct agp_memory_info {
	vsize_t		ami_size;	/* size in bytes */
	bus_addr_t	ami_physical;	/* bogus hack for i810 */
	off_t		ami_offset;	/* page offset if bound */
	int		ami_is_bound;	/* non-zero if bound */
};

/* #define	AGP_DEBUG */
#ifdef AGP_DEBUG
#define AGP_DPF(x...) do {			\
    printf("agp: ");				\
    printf(##x);				\
} while (0)
#else
#define AGP_DPF(x...) do {} while (0)
#endif

#define AGPUNIT(x)	minor(x)

struct agp_methods {
	u_int32_t (*get_aperture)(struct vga_pci_softc *);
	int (*set_aperture)(struct vga_pci_softc *, u_int32_t);
	int (*bind_page)(struct vga_pci_softc *, off_t, bus_addr_t);
	int (*unbind_page)(struct vga_pci_softc *, off_t);
	void (*flush_tlb)(struct vga_pci_softc *);
	int (*enable)(struct vga_pci_softc *, u_int32_t mode);
	struct agp_memory *(*alloc_memory)(struct vga_pci_softc *, int, vsize_t);
	int (*free_memory)(struct vga_pci_softc *, struct agp_memory *);
	int (*bind_memory)(struct vga_pci_softc *, struct agp_memory *, off_t);
	int (*unbind_memory)(struct vga_pci_softc *, struct agp_memory *);
};

#define AGP_GET_APERTURE(sc)	 ((sc)->sc_methods->get_aperture(sc))
#define AGP_SET_APERTURE(sc,a)	 ((sc)->sc_methods->set_aperture((sc),(a)))
#define AGP_BIND_PAGE(sc,o,p)	 ((sc)->sc_methods->bind_page((sc),(o),(p)))
#define AGP_UNBIND_PAGE(sc,o)	 ((sc)->sc_methods->unbind_page((sc), (o)))
#define AGP_FLUSH_TLB(sc)	 ((sc)->sc_methods->flush_tlb(sc))
#define AGP_ENABLE(sc,m)	 ((sc)->sc_methods->enable((sc),(m)))
#define AGP_ALLOC_MEMORY(sc,t,s) ((sc)->sc_methods->alloc_memory((sc),(t),(s)))
#define AGP_FREE_MEMORY(sc,m)	 ((sc)->sc_methods->free_memory((sc),(m)))
#define AGP_BIND_MEMORY(sc,m,o)	 ((sc)->sc_methods->bind_memory((sc),(m),(o)))
#define AGP_UNBIND_MEMORY(sc,m)	 ((sc)->sc_methods->unbind_memory((sc),(m)))

/*
 * All chipset drivers must have this at the start of their softc.
 */

struct agp_gatt {
	u_int32_t	ag_entries;
	u_int32_t	*ag_virtual;
	bus_addr_t	ag_physical;
	bus_dmamap_t	ag_dmamap;
	bus_dma_segment_t ag_dmaseg;
	size_t		ag_size;
};


/*
 * Functions private to the AGP code.
 */

int agp_find_caps(pci_chipset_tag_t pct, pcitag_t pt);
int agp_map_aperture(struct vga_pci_softc *sc);
struct agp_gatt *agp_alloc_gatt(struct vga_pci_softc *sc);
void agp_free_gatt(struct vga_pci_softc *sc, struct agp_gatt *gatt);
void agp_flush_cache(void);
int agp_generic_attach(struct vga_pci_softc *sc);
int agp_generic_detach(struct vga_pci_softc *sc);
int agp_generic_enable(struct vga_pci_softc *sc, u_int32_t mode);
struct agp_memory *agp_generic_alloc_memory(struct vga_pci_softc *sc, int type,
						 vsize_t size);
int agp_generic_free_memory(struct vga_pci_softc *sc, struct agp_memory *mem);
int agp_generic_bind_memory(struct vga_pci_softc *sc, struct agp_memory *mem,
						off_t offset);
int agp_generic_unbind_memory(struct vga_pci_softc *sc, struct agp_memory *mem);

int agp_ali_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa, struct pci_attach_args *p);
int agp_amd_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa, struct pci_attach_args *p);
int agp_i810_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa, struct pci_attach_args *p);
int agp_intel_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa, struct pci_attach_args *p);
int agp_via_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa, struct pci_attach_args *p);
int agp_sis_attach(struct vga_pci_softc *sc, struct pci_attach_args *pa, struct pci_attach_args *p);

int agp_alloc_dmamem(bus_dma_tag_t, size_t, int, bus_dmamap_t *, caddr_t *,
		     bus_addr_t *, bus_dma_segment_t *, int, int *);
void agp_free_dmamem(bus_dma_tag_t tag, size_t size, bus_dmamap_t map,
		     caddr_t vaddr, bus_dma_segment_t *seg, int nseg) ;

#endif /* !_PCI_AGPVAR_H_ */
