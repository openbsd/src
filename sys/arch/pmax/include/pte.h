/*	$NetBSD: pte.h,v 1.1 1996/10/13 09:28:56 jonathan Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef  __MIPS_PTE_H__
#define  __MIPS_PTE_H__

/* XXX hack to make libkvm.old happy - MIPS1 is only defined if we are
       building a kernel - but not for libkvm.old - so we define MIPS1
       here - which is no problem at the moment - because the pmax port
       at the moment only supports MIPS1 cpu based machines
*/
#if !defined(MIPS1) && !defined(MIPS3)
#define MIPS1
#endif

#if defined(MIPS1) && defined(MIPS3)
#error  Cannot yet support both  "MIPS1" (r2000 family) and "MIPS3" (r4000 family) in the same kernel.
#endif

#ifdef MIPS1
#include <machine/mips1_pte.h>
#endif

#ifdef MIPS3
#include <machine/mips3_pte.h>
#endif


#if defined(_KERNEL) && !defined(_LOCORE)
/*
 * Kernel virtual address to page table entry and visa versa.
 */
#define	kvtopte(va) \
	(Sysmap + (((vm_offset_t)(va) - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT))
#define	ptetokv(pte) \
	((((pt_entry_t *)(pte) - Sysmap) << PGSHIFT) + VM_MIN_KERNEL_ADDRESS)

extern	pt_entry_t *Sysmap;		/* kernel pte table */
extern	u_int Sysmapsize;		/* number of pte's in Sysmap */
#endif	/* defined(_KERNEL) && !defined(_LOCORE) */

#endif /* __MIPS_PTE_H__ */
