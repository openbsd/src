/*	$OpenBSD: vmparam.h,v 1.30 2004/11/28 01:36:38 mickey Exp $	*/
/*	$NetBSD: vmparam.h,v 1.15 1994/10/27 04:16:34 cgd Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vmparam.h	5.9 (Berkeley) 5/12/91
 */

#ifndef _MACHINE_VM_PARAM_H_
#define _MACHINE_VM_PARAM_H_
/*
 * Machine dependent constants for 386.
 */

/*
 * Virtual address space arrangement. On 386, both user and kernel
 * share the address space, not unlike the vax.
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack. Immediately above the user stack
 * resides the user structure, which is UPAGES long and contains the
 * kernel stack.
 *
 * Immediately after the user structure is the page table map, and then
 * kernal address space.
 */
#define	USRTEXT		PAGE_SIZE
#define	USRSTACK	VM_MAXUSER_ADDRESS

/*
 * Virtual memory related constants, all in bytes
 */
#define	MAXTSIZ		(64*1024*1024)		/* max text size */
#ifndef DFLDSIZ
#define	DFLDSIZ		(64*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(1024*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(4*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(32*1024*1024)		/* max stack size */
#endif

/* I386 has a line where all code is executable: 0 - I386_MAX_EXE_ADDR */
#define I386_MAX_EXE_ADDR 0x20000000		/* exec line */

/*
 * Size of shared memory map
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	8192
#endif

/*
 * Size of User Raw I/O map
 */
#define	USRIOSIZE 	300

/* XXX Compatibility */
#define APTDPTDI	PDSLOT_APTE
#define PTDPTDI		PDSLOT_PTE

/* user/kernel map constants */
#define VM_MIN_ADDRESS		((vaddr_t)0)
#define VM_MAXUSER_ADDRESS	((vaddr_t)((PTDPTDI<<PDSHIFT) - USPACE))
#define VM_MAX_ADDRESS		((vaddr_t)((PTDPTDI<<PDSHIFT) + (PTDPTDI<<PGSHIFT)))
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)KERNBASE)
#define VM_MAX_KERNEL_ADDRESS	((vaddr_t)(APTDPTDI<<PDSHIFT))

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(USRIOSIZE*PAGE_SIZE)

#define	VM_PHYSSEG_MAX	4	/* actually we could have this many segments */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define	VM_PHYSSEG_NOADD	/* can't add RAM after vm_mem_init */

#define VM_NFREELIST		2
#define VM_FREELIST_DEFAULT	0
#define VM_FREELIST_FIRST16	1

/*
 * pmap specific data stored in the vm_physmem[] array 
 */
#define __HAVE_PMAP_PHYSSEG
struct pmap_physseg {
	struct pv_head *pvhead;		/* pv_head array */
	char *attrs;			/* attrs array */
};

#endif /* _MACHINE_VM_PARAM_H_ */
