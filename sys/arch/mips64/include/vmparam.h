/*	$OpenBSD: vmparam.h,v 1.7 2004/11/11 16:12:59 pefo Exp $	*/
/*	$NetBSD: vmparam.h,v 1.5 1994/10/26 21:10:10 cgd Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: Utah Hdr: vmparam.h 1.16 91/01/18
 *	@(#)vmparam.h	8.2 (Berkeley) 4/22/94
 */

#ifndef _MIPS_VMPARAM_H_
#define _MIPS_VMPARAM_H_

/*
 * Machine dependent constants mips processors.
 */
/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.
 */
#define	USRTEXT		0x0000000000400000L
#define	USRSTACK	0x0000000080000000L	/* Start of user stack */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(64*1024*1024)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(128*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(1*1024*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(32*1024*1024)		/* max stack size */
#endif

/*
 * PTEs for mapping user space into the kernel for phyio operations.
 * 16 pte's are enough to cover 8 disks * MAXBSIZE.
 */
#ifndef USRIOSIZE
#define USRIOSIZE	32
#endif

/*
 * PTEs for system V style shared memory.
 * This is basically slop for kmempt which we actually allocate (malloc) from.
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	8192		/* 8mb */
#endif

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP		20

#define	VM_PHYSSEG_MAX	8	/* Max number of physical memory segments */
#define VM_PHYSSEG_STRAT VM_PSTRAT_BSEARCH
#define VM_PHYSSEG_NOADD


/* user/kernel map constants */
#ifdef __LP64__
#define VM_MIN_ADDRESS		((vaddr_t)0x0000000000000000L)
#define VM_MAXUSER_ADDRESS	((vaddr_t)0x0000000080000000L)
#define VM_MAX_ADDRESS		((vaddr_t)0x0000000080000000L)
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xffffffffc0000000L)
#else
#define VM_MIN_ADDRESS		((vaddr_t)0x00000000)
#define VM_MAXUSER_ADDRESS	((vaddr_t)0x80000000)
#define VM_MAX_ADDRESS		((vaddr_t)0x80000000)
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xc0000000)
#endif

#define	VM_NFREELIST		1
#define	VM_FREELIST_DEFAULT	0

/* Kernel page table size is variable. */
extern vaddr_t virtual_end;
#define VM_MAX_KERNEL_ADDRESS	virtual_end

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(USRIOSIZE*PAGE_SIZE)

#if defined(_KERNEL) && !defined(_LOCORE)
/*
 * pmap-specific data
 */

/* XXX - belongs in pmap.h, but put here because of ordering issues */
typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vaddr_t		pv_va;		/* virtual address for mapping */
	int		pv_flags;	/* Some flags for the mapping */
} *pv_entry_t;

#define __HAVE_VM_PAGE_MD
struct vm_page_md {
	struct pv_entry pvent;		/* pv list of this seg */
};

#define	VM_MDPAGE_INIT(pg) \
	do { \
		(pg)->mdpage.pvent.pv_next = NULL; \
		(pg)->mdpage.pvent.pv_pmap = NULL; \
		(pg)->mdpage.pvent.pv_va = 0; \
		(pg)->mdpage.pvent.pv_flags = 0; \
	} while (0)

#endif	/* _KERNEL && !_LOCORE */

#endif /* !_MIPS_VMPARAM_H_ */
