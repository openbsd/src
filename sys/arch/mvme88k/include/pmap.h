/*	$OpenBSD: pmap.h,v 1.23 2001/12/22 10:22:13 smurph Exp $ */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */
#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_
#define OMRON_PMAP

#include <machine/mmu.h>		/* batc_template_t, BATC_MAX, etc.*/
#include <machine/pcb.h>		/* pcb_t, etc.*/
#include <machine/psl.h>		/* get standard goodies		*/

typedef struct sdt_entry *sdt_ptr_t;

/*
 * PMAP structure
 */
typedef struct pmap *pmap_t;

struct pmap {
	sdt_ptr_t           sdt_paddr;	    /* physical pointer to sdt */
	sdt_ptr_t           sdt_vaddr;	    /* virtual pointer to sdt */
	int                 ref_count;	    /* reference count */
	struct simplelock   lock;
	struct pmap_statistics stats;	    /* pmap statistics */

	/* cpus using of this pmap; NCPU must be <= 32 */
	unsigned long      cpus_using;

#ifdef DEBUG
	pmap_t              next;
	pmap_t              prev;
#endif

	/* for OMRON_PMAP */
	batc_template_t i_batc[BATC_MAX];  /* instruction BATCs */
	batc_template_t d_batc[BATC_MAX];  /* data BATCs */
	/* end OMRON_PMAP */

}; 

#define PMAP_NULL ((pmap_t) 0)

/* 	The PV (Physical to virtual) List.
 *
 * For each vm_page_t, pmap keeps a list of all currently valid virtual
 * mappings of that page. An entry is a pv_entry_t; the list is the
 * pv_head_table. This is used by things like pmap_remove, when we must
 * find and remove all mappings for a particular physical page.
 */
typedef  struct pv_entry {
	struct pv_entry	*next;	/* next pv_entry */
	pmap_t		pmap;	/* pmap where mapping lies */
	vm_offset_t	va;	/* virtual address for mapping */
} *pv_entry_t;

#ifdef	_KERNEL

extern	pmap_t		kernel_pmap;
extern	struct pmap	kernel_pmap_store;
extern	caddr_t		vmmap;

#define	pmap_kernel()		(&kernel_pmap_store)
#define pmap_resident_count(pmap) ((pmap)->stats.resident_count)
/* Used in builtin/device_pager.c */
#define pmap_phys_address(frame)        ((vm_offset_t) (ptoa(frame)))

#define pmap_update(pmap)	/* nothing (yet) */

#define PMAP_ACTIVATE(proc)	pmap_activate(proc)
#define PMAP_DEACTIVATE(proc)	pmap_deactivate(proc)
#define PMAP_CONTEXT(pmap, thread)

/*
 * Modes used when calling pmap_cache_flush().
 */
#define	FLUSH_CACHE		0
#define	FLUSH_CODE_CACHE	1
#define	FLUSH_DATA_CACHE	2
#define	FLUSH_LOCAL_CACHE	3
#define	FLUSH_LOCAL_CODE_CACHE	4
#define	FLUSH_LOCAL_DATA_CACHE	5

/**************************************************************************/
/*** Prototypes for public functions defined in pmap.c ********************/
/**************************************************************************/

void pmap_bootstrap __P((vm_offset_t, vm_offset_t *, vm_offset_t *,
			 vm_offset_t *, vm_offset_t *));
void pmap_cache_ctrl __P((pmap_t, vm_offset_t, vm_offset_t, unsigned));
pt_entry_t *pmap_pte __P((pmap_t, vm_offset_t));
void pmap_cache_ctrl __P((pmap_t, vm_offset_t, vm_offset_t, unsigned));
void pmap_zero_page __P((vm_offset_t));
void pmap_remove_all __P((vm_offset_t));
vm_offset_t pmap_extract_unlocked __P((pmap_t, vm_offset_t));
void copy_to_phys __P((vm_offset_t, vm_offset_t, int));
void copy_from_phys __P((vm_offset_t, vm_offset_t, int));
void pmap_redzone __P((pmap_t, vm_offset_t));
void icache_flush __P((vm_offset_t));
void pmap_dcache_flush __P((pmap_t, vm_offset_t));
void pmap_cache_flush __P((pmap_t, vm_offset_t, int, int));
void pmap_print __P((pmap_t));
void pmap_print_trace __P((pmap_t, vm_offset_t, boolean_t));
vm_offset_t pmap_map __P((vm_offset_t, vm_offset_t, vm_offset_t,
			  vm_prot_t, unsigned int));
vm_offset_t pmap_map_batc __P((vm_offset_t, vm_offset_t, vm_offset_t,
			       vm_prot_t, unsigned int));
#endif	/* _KERNEL */

#endif /* _MACHINE_PMAP_H_ */
