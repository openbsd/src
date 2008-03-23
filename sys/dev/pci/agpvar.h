/*	$OpenBSD: agpvar.h,v 1.12 2008/03/23 19:54:47 oga Exp $	*/
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

#include <sys/rwlock.h>

/* #define	AGP_DEBUG */
#ifdef AGP_DEBUG
#define AGP_DPF(fmt, arg...) do { printf("agp: " fmt ,##arg); } while (0)
#else
#define AGP_DPF(fmt, arg...) do {} while (0)
#endif

#define AGPUNIT(x)	minor(x)

struct agpbus_attach_args {
        struct pci_attach_args apa_pci_args;
};

enum agp_acquire_state {
	AGP_ACQUIRE_FREE,
	AGP_ACQUIRE_USER,
	AGP_ACQUIRE_KERNEL
};

/*
 * Data structure to describe an AGP memory allocation.
 */
TAILQ_HEAD(agp_memory_list, agp_memory);
struct agp_memory {
	TAILQ_ENTRY(agp_memory) am_link;	/* wiring for the tailq */
	int		am_id;			/* unique id for block */
	vsize_t		am_size;		/* number of bytes allocated */
	int		am_type;		/* chipset specific type */
	off_t		am_offset;		/* page offset if bound */
	int		am_is_bound;		/* non-zero if bound */
	bus_addr_t	am_physical;
	caddr_t		am_virtual;
	bus_dmamap_t	am_dmamap;
	int		am_nseg;
	bus_dma_segment_t *am_dmaseg;
};

/*
 * This structure is used to query the state of the AGP system.
 */
struct agp_info {
	u_int32_t       ai_mode;
	bus_addr_t      ai_aperture_base;
	bus_size_t      ai_aperture_size;
	vsize_t         ai_memory_allowed;
	vsize_t         ai_memory_used;
	u_int32_t       ai_devid;
};

struct agp_memory_info {
        vsize_t         ami_size;       /* size in bytes */
        bus_addr_t      ami_physical;   /* bogus hack for i810 */
        off_t           ami_offset;     /* page offset if bound */
        int             ami_is_bound;   /* non-zero if bound */
};

struct agp_softc;

struct agp_methods {
	u_int32_t (*get_aperture)(struct agp_softc *);
	int	(*set_aperture)(struct agp_softc *, u_int32_t);
	int	(*bind_page)(struct agp_softc *, off_t, bus_addr_t);
	int	(*unbind_page)(struct agp_softc *, off_t);
	void	(*flush_tlb)(struct agp_softc *);
	int	(*enable)(struct agp_softc *, u_int32_t mode);
	struct agp_memory *
		(*alloc_memory)(struct agp_softc *, int, vsize_t);
	int	(*free_memory)(struct agp_softc *, struct agp_memory *);
	int	(*bind_memory)(struct agp_softc *, struct agp_memory *,
		    off_t);
	int	(*unbind_memory)(struct agp_softc *, struct agp_memory *);
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
struct agp_softc {
	struct device sc_dev;

	bus_space_tag_t		sc_bt, sc_memt;
	bus_space_handle_t	sc_bh;
	bus_addr_t		sc_apaddr;
	bus_size_t		sc_apsize;
	bus_dma_tag_t		sc_dmat;
	struct rwlock		sc_lock;	/* lock for access to GATT */
	pcitag_t		sc_pcitag;	/* PCI tag, in case we need it. */
	pcireg_t		sc_id;
	pci_chipset_tag_t	sc_pc;

	struct agp_methods 	*sc_methods;
	void			*sc_chipc;	/* chipset-dependent state */

	int			sc_opened;
	int			sc_capoff;			
	int			sc_apflags;
	int			sc_nextid;	/* next memory block id */

	u_int32_t		sc_maxmem;	/* allocation upper bound */
	u_int32_t		sc_allocated;	/* amount allocated */
	enum agp_acquire_state	sc_state;
	struct agp_memory_list	sc_memory;	/* list of allocated memory */
};

struct agp_gatt {
	u_int32_t	ag_entries;
	u_int32_t	*ag_virtual;
	bus_addr_t	ag_physical;
	bus_dmamap_t	ag_dmamap;
	bus_dma_segment_t ag_dmaseg;
	size_t		ag_size;
};


struct agp_product {
	int	ap_vendor;
	int	ap_product;
	int	(*ap_attach)(struct agp_softc *,
		     struct pci_attach_args *);
};
/* MD-defined */
extern 	const struct agp_product agp_products[];

void	agp_attach(struct device *, struct device *, void *);
int	agp_probe(struct device *, void *, void *);
paddr_t	agpmmap(void *, off_t, int);
int	agpioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	agpopen(dev_t, int, int, struct proc *);
int	agpclose(dev_t, int, int , struct proc *);
void	agp_set_pchb(struct pci_attach_args*);
/*
 * Functions private to the AGP code.
 */

int	agp_find_caps(pci_chipset_tag_t, pcitag_t);
int	agp_map_aperture(struct pci_attach_args *, 
	    struct agp_softc *, u_int32_t, u_int32_t);
u_int32_t agp_generic_get_aperture(struct agp_softc *);
int	agp_generic_set_aperture(struct agp_softc *, u_int32_t);
struct agp_gatt *
	agp_alloc_gatt(struct agp_softc *);
void	agp_free_gatt(struct agp_softc *, struct agp_gatt *);
void	agp_flush_cache(void);
int	agp_generic_attach(struct agp_softc *);
int	agp_generic_detach(struct agp_softc *);
int	agp_generic_enable(struct agp_softc *, u_int32_t);
struct agp_memory *
	agp_generic_alloc_memory(struct agp_softc *, int, vsize_t size);
int	agp_generic_free_memory(struct agp_softc *, struct agp_memory *);
int	agp_generic_bind_memory(struct agp_softc *, struct agp_memory *,
	    off_t);
int	agp_generic_unbind_memory(struct agp_softc *, struct agp_memory *);

int	agp_ali_attach(struct agp_softc *, struct pci_attach_args *);
int	agp_amd_attach(struct agp_softc *, struct pci_attach_args *);
int	agp_i810_attach(struct agp_softc *, struct pci_attach_args *);
int	agp_intel_attach(struct agp_softc *, struct pci_attach_args *);
int	agp_via_attach(struct agp_softc *, struct pci_attach_args *);
int	agp_sis_attach(struct agp_softc *, struct pci_attach_args *);

int	agp_alloc_dmamem(bus_dma_tag_t, size_t, int, bus_dmamap_t *,
	    caddr_t *, bus_addr_t *, bus_dma_segment_t *, int, int *);
void	agp_free_dmamem(bus_dma_tag_t, size_t, bus_dmamap_t,
	    caddr_t, bus_dma_segment_t *, int nseg) ;


/*
 * Kernel API
 */
/*
 * Find the AGP device and return it.
 */
void	*agp_find_device(int);

/*
 * Return the current owner of the AGP chipset.
 */
enum	 agp_acquire_state agp_state(void *);

/*
 * Query the state of the AGP system.
 */
void	 agp_get_info(void *, struct agp_info *);

/*
 * Acquire the AGP chipset for use by the kernel. Returns EBUSY if the
 * AGP chipset is already acquired by another user.
 */
int	 agp_acquire(void *);

/*
 * Release the AGP chipset.
 */
int	 agp_release(void *);

/*
 * Enable the agp hardware with the relavent mode. The mode bits are
 * defined in <dev/pci/agpreg.h>
 */
int	 agp_enable(void *, u_int32_t);

/*
 * Allocate physical memory suitable for mapping into the AGP
 * aperture.  The value returned is an opaque handle which can be
 * passed to agp_bind(), agp_unbind() or agp_deallocate().
 */
void	*agp_alloc_memory(void *, int, vsize_t);

/*
 * Free memory which was allocated with agp_allocate().
 */
void	 agp_free_memory(void *, void *);

/*
 * Bind memory allocated with agp_allocate() at a given offset within
 * the AGP aperture. Returns EINVAL if the memory is already bound or
 * the offset is not at an AGP page boundary.
 */
int	 agp_bind_memory(void *, void *, off_t);

/*
 * Unbind memory from the AGP aperture. Returns EINVAL if the memory
 * is not bound.
 */
int	 agp_unbind_memory(void *, void *);

/*
 * Retrieve information about a memory block allocated with
 * agp_alloc_memory().
 */
void	 agp_memory_info(void *, void *, struct agp_memory_info *);

#endif /* !_PCI_AGPVAR_H_ */
