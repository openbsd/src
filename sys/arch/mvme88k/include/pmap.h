/*	$OpenBSD: pmap.h,v 1.5 1999/02/09 06:36:27 smurph Exp $ */
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

#include <machine/psl.h>		/* get standard goodies		*/
#include <vm/vm_param.h>
#include <vm/vm_prot.h>			/* vm_prot_t 			*/
#include <machine/mmu.h>		/* batc_template_t, BATC_MAX, etc.*/
#include <machine/pcb.h>		/* pcb_t, etc.*/

typedef struct sdt_entry *sdt_ptr_t;

/*
 * PMAP structure
 */
typedef struct pmap *pmap_t;

struct pmap {
    sdt_ptr_t		sdt_paddr;	/* physical pointer to sdt */
    sdt_ptr_t		sdt_vaddr;	/* virtual pointer to sdt */
    int			ref_count;	/* reference count */
    simple_lock_data_t	lock;
    struct pmap_statistics stats;	/* pmap statistics */

#ifdef DEBUG
    pmap_t		next;
    pmap_t		prev;
#endif

   /* for OMRON_PMAP */
   batc_template_t i_batc[BATC_MAX];  /* instruction BATCs */
   batc_template_t d_batc[BATC_MAX];  /* data BATCs */
   /* end OMRON_PMAP */

}; 

#include <vm/vm.h>

#define PMAP_NULL ((pmap_t) 0)

extern	pmap_t	kernel_pmap;

#define PMAP_ACTIVATE(pmap, th, my_cpu)	_pmap_activate(pmap, th, my_cpu)
#define PMAP_DEACTIVATE(pmap, th, my_cpu) _pmap_deactivate(pmap, th, my_cpu)

#define PMAP_CONTEXT(pmap, thread)

#define pmap_resident_count(pmap) ((pmap)->stats.resident_count)

/* Used in builtin/device_pager.c */
#define pmap_phys_address(frame)        ((vm_offset_t) (M88K_PTOB(frame)))

/* Used in kern/mach_timedev.c */
#define pmap_phys_to_frame(phys)        ((int) (M88K_BTOP(phys)))

/*
 * Since Our PCB has no infomation about the mapping,
 * we have nothing to do in PMAP_PCB_INITIALIZE.
 * XXX
 */
/* Used in machine/pcb.c */
#define PMAP_PCB_INITIALIZE(x)

/*
 * Modes used when calling pmap_cache_fulsh().
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

void _pmap_activate(pmap_t pmap, pcb_t, int my_cpu);
void _pmap_deactivate(pmap_t pmap, pcb_t, int my_cpu);
void pmap_activate(pmap_t my_pmap, pcb_t);
void pmap_deactivate(pmap_t pmap, pcb_t);
int pmap_check_transaction(pmap_t pmap, vm_offset_t va, vm_prot_t type);

vm_offset_t pmap_map_batc(
      vm_offset_t virt,
      vm_offset_t start,
      vm_offset_t end,
      vm_prot_t prot,
      unsigned cmode);

#ifdef JUNK
int pmap_attribute(
    pmap_t pmap,
    vm_offset_t address,
    vm_size_t size,
    vm_machine_attribute_t attribute,
    vm_machine_attribute_val_t* value);  /* IN/OUT */
#endif /* JUNK */

void pmap_bootstrap(
    vm_offset_t load_start, /* IN */
    vm_offset_t *phys_start, /* IN/OUT */
    vm_offset_t *phys_end, /* IN */
    vm_offset_t *virt_start, /* OUT */
    vm_offset_t *virt_end); /* OUT */

pt_entry_t *pmap_pte(pmap_t map, vm_offset_t virt);
void pmap_cache_ctrl(pmap_t pmap, vm_offset_t s, vm_offset_t e, unsigned mode);
void pmap_zero_page(vm_offset_t phys);
void pmap_remove_all(vm_offset_t phys);
vm_offset_t pmap_extract_unlocked(pmap_t pmap, vm_offset_t va);
pmap_t pmap_kernel(void);
void copy_to_phys(vm_offset_t srcva, vm_offset_t dstpa, int bytecount);
void copy_from_phys(vm_offset_t srcpa, vm_offset_t dstva, int bytecount);
void pmap_redzone(pmap_t pmap, vm_offset_t va);
boolean_t pmap_verify_free(vm_offset_t phys);
boolean_t pmap_valid_page(vm_offset_t p);
void icache_flush(vm_offset_t pa);
void pmap_dcache_flush(pmap_t pmap, vm_offset_t va);
void pmap_cache_flush(pmap_t pmap, vm_offset_t virt, int bytes, int mode);
void pmap_print (pmap_t pmap);
void pmap_print_trace (pmap_t pmap, vm_offset_t va, boolean_t long_format);

#if 0
#ifdef OMRON_PMAP
 void pmap_set_batc(
    pmap_t pmap,
    boolean_t data,
    int i,
    vm_offset_t va,
    vm_offset_t pa,
    boolean_t super,
    boolean_t wt,
    boolean_t global,
    boolean_t ci,
    boolean_t wp,
    boolean_t valid);

 void use_batc(
    task_t task, 
    boolean_t data,         /* for data-cmmu ? */
    int i,                  /* batc number */
    vm_offset_t va,         /* virtual address */
    vm_offset_t pa,         /* physical address */
    boolean_t s,            /* for super-mode ? */
    boolean_t wt,           /* is writethrough */
    boolean_t g,            /* is global ? */
    boolean_t ci,           /* is cache inhibited ? */
    boolean_t wp,           /* is write-protected ? */
    boolean_t v);           /* is valid ? */
#endif
#endif /* 0 */

#endif /* endif  _MACHINE_PMAP_H_ */
