/* $OpenBSD: vga_pcivar.h,v 1.8 2007/11/03 10:09:03 martin Exp $ */
/* $NetBSD: vga_pcivar.h,v 1.1 1998/03/22 15:16:19 drochner Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _PCI_VGA_PCIVAR_H_
#define _PCI_VGA_PCIVAR_H_

#define	DEVICE_IS_VGA_PCI(class)					\
	    (((PCI_CLASS(class) == PCI_CLASS_DISPLAY &&			\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_DISPLAY_VGA) ||	\
	     (PCI_CLASS(class) == PCI_CLASS_PREHISTORIC &&		\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_PREHISTORIC_VGA)) ? 1 : 0)

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

struct vga_pci_softc {
	struct device sc_dev;

#if 0
	struct vga_config *sc_vc;	/* VGA configuration */
#endif
#ifdef VESAFB
	int sc_width;
	int sc_height;
	int sc_depth;
	int sc_linebytes;
	u_int32_t sc_base;
	int sc_mode;			/* WSDISPLAY_MODE_EMUL or _DUMBFB */
	int sc_textmode;		/* original VESA text mode */
	int sc_gfxmode;			/* VESA graphics mode */
	u_char sc_cmap_red[256];	/* saved color map */
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];

#endif
#ifdef PCIAGP
	/* agp stuff */
	bus_space_tag_t sc_bt, sc_memt;
	bus_space_handle_t sc_bh;
	bus_addr_t sc_apaddr;
	bus_size_t sc_apsize;
	bus_dma_tag_t sc_dmat;
	struct lock sc_lock;		/* lock for access to GATT */
	pcitag_t sc_pcitag;		/* PCI tag, in case we need it. */
	pcireg_t sc_id;
	pci_chipset_tag_t sc_pc;

	struct agp_methods *sc_methods;
	void	*sc_chipc;		/* chipset-dependent state */

	int sc_opened;
	int sc_capoff;			
	int sc_apflags;
	int sc_nextid;	/* next memory block id */

	u_int32_t		sc_maxmem;	/* allocation upper bound */
	u_int32_t		sc_allocated;	/* amount allocated */
	enum agp_acquire_state	sc_state;
	struct agp_memory_list	sc_memory;	/* list of allocated memory */
#endif
};

#ifdef PCIAGP
struct agp_product {
	int	ap_vendor;
	int	ap_product;
	int	(*ap_attach)(struct vga_pci_softc *,
		     struct pci_attach_args *, struct pci_attach_args *);
};
/* MD-defined */
extern const struct agp_product agp_products[];

void agp_attach(struct device *, struct device *, void *);
paddr_t agp_mmap(void *, off_t, int);
int agp_ioctl(void *, u_long, caddr_t, int, struct proc *);
#endif /* PCIAGP */

int vga_pci_cnattach(bus_space_tag_t, bus_space_tag_t,
			  pci_chipset_tag_t, int, int, int);

#ifdef VESAFB
int vesafb_find_mode(struct vga_pci_softc *, int, int, int);
void vesafb_set_mode(struct vga_pci_softc *, int);
int vesafb_get_mode(struct vga_pci_softc *);
int vesafb_get_supported_depth(struct vga_pci_softc *);
#endif

#endif /* _PCI_VGA_PCIVAR_H_ */
