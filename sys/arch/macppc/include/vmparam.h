/*	$OpenBSD: vmparam.h,v 1.14 2004/11/28 01:36:39 mickey Exp $	*/
/*	$NetBSD: vmparam.h,v 1.1 1996/09/30 16:34:38 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MACHINE_VMPARAM_H
#define MACHINE_VMPARAM_H

#define	USRTEXT		PAGE_SIZE
#define	USRSTACK	VM_MAXUSER_ADDRESS

#ifndef	MAXTSIZ
#define	MAXTSIZ		(16*1024*1024)		/* max text size */
#endif

#ifndef	DFLDSIZ
#define	DFLDSIZ		(32*1024*1024)		/* default data size */
#endif

#ifndef	MAXDSIZ
#define	MAXDSIZ		(512*1024*1024)		/* max data size */
#endif

#ifndef	DFLSSIZ
#define	DFLSSIZ		(1*1024*1024)		/* default stack size */
#endif

#ifndef	MAXSSIZ
#define	MAXSSIZ		(32*1024*1024)		/* max stack size */
#endif

/*
 * Size of shared memory map
 */
#ifndef	SHMMAXPGS
#define	SHMMAXPGS       8192			/* 32mb */
#endif

/*
 * Size of User Raw I/O map
 */
#define	USRIOSIZE	1024

/*
 * Would like to have MAX addresses = 0, but this doesn't (currently) work
 */
#define	VM_MIN_ADDRESS		((vm_offset_t)0)
#define	VM_MAXUSER_ADDRESS	((vm_offset_t)0xfffff000)
#define	VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS
#define	VM_MIN_KERNEL_ADDRESS	((vm_offset_t)(PPC_KERNEL_SR << ADDR_SR_SHIFT))

/* ppc_kvm_stolen is so that vm space can be stolen before vm is fully
 * initialized.
 */
extern vm_offset_t ppc_kvm_stolen;
#define VM_KERN_ADDRESS_SIZE  (PPC_SEGMENT_LENGTH - (32 * 1024 * 1024))
#define	VM_MAX_KERNEL_ADDRESS	(VM_MIN_KERNEL_ADDRESS + VM_KERN_ADDRESS_SIZE)

#define	VM_PHYS_SIZE		(USRIOSIZE * PAGE_SIZE)

#define __HAVE_PMAP_PHYSSEG
struct pmap_physseg {
	struct pted_pv_head *pvent;
	char *attrs;
	/* NULL ??? */
};

#define	VM_PHYSSEG_MAX	32	/* actually we could have this many segments */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define	VM_PHYSSEG_NOADD	/* can't add RAM after vm_mem_init */

#define VM_NFREELIST		1
#define VM_FREELIST_DEFAULT	0

#endif
