/*	$OpenBSD: vmparam.h,v 1.1 2003/01/27 19:37:30 miod Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 */

/*
 * Machine dependent constants for m68k-based platforms.
 */
#ifndef	_M68K_VMPARAM_H_
#define	_M68K_VMPARAM_H_

/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.  LOWPAGES and HIGHPAGES are
 * the number of pages from the beginning of the P0 region to the
 * beginning of the text and from the beginning of the P1 region to the
 * beginning of the stack respectively.
 *
 * NOTE: the ONLY reason that HIGHPAGES is 0x100 instead of UPAGES (3)
 * is for HPUX compatibility.  Why??  Because HPUX's debuggers
 * have the user's stack hard-wired at FFF00000 for post-mortems,
 * and we must be compatible...
 */
#define	USRTEXT		8192
#define	USRSTACK	(-HIGHPAGES*NBPG)	/* Start of user stack */
#define	LOWPAGES	0
#define	HIGHPAGES	(0x100000/NBPG)

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(8*1024*1024)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(32*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(64*1024*1024)		/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		MAXDSIZ			/* max stack size */
#endif

/*
 * Sizes of the system and user portions of the system page table.
 */
#define	USRPTSIZE 	(1 * NPTEPG)	/* 4mb */

/*
 * PTEs for mapping user space into the kernel for phyio operations.
 * One page is enough to handle 4Mb of simultaneous raw IO operations.
 */
#ifndef USRIOSIZE
#define USRIOSIZE	(1 * NPTEPG)	/* 4mb */
#endif

/*
 * PTEs for system V style shared memory.
 * This is basically slop for kmempt which we actually allocate (malloc) from.
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	1024		/* 4mb */
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
#define	MAXSLP 		20

/* user/kernel map constants */
#define VM_MIN_ADDRESS		((vaddr_t)0)
#define VM_MAXUSER_ADDRESS	((vaddr_t)(USRSTACK))
#define VM_MAX_ADDRESS		((vaddr_t)(0-(UPAGES*NBPG)))
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)0)
#define VM_MAX_KERNEL_ADDRESS	((vaddr_t)(0-NBPG))

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(USRIOSIZE*PAGE_SIZE)

/* # of kernel PT pages (initial only, can grow dynamically) */
#define VM_KERNEL_PT_PAGES	((vsize_t)2)

/* pcb base */
#define	pcbb(p)			((u_int)(p)->p_addr)

/*
 * Constants which control the way the VM system deals with memory segments.
 */
#define	VM_PHYSSEG_NOADD

#define	VM_NFREELIST		1
#define	VM_FREELIST_DEFAULT	0

#ifndef _LOCORE

#include <machine/pte.h>	/* st_entry_t */

/* XXX - belongs in pmap.h, but put here because of ordering issues */
struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vaddr_t		pv_va;		/* virtual address for mapping */
	st_entry_t	*pv_ptste;	/* non-zero if VA maps a PT page */
	struct pmap	*pv_ptpmap;	/* if pv_ptste, pmap for PT page */
	int		pv_flags;	/* flags */
};

/*
 * pv_flags carries some PTE permission bits as well - make sure extra flags
 * values are > (1 << PG_SHIFT)
 */
/* header: all entries are cache inhibited */
#define	PV_CI		(0x01 << PG_SHIFT)
/* header: entry maps a page table page */
#define PV_PTPAGE	(0x02 << PG_SHIFT)

#define	__HAVE_VM_PAGE_MD
struct vm_page_md {
	struct pv_entry pvent;
};

#define	VM_MDPAGE_INIT(pg) do {			\
	(pg)->mdpage.pvent.pv_next = NULL;	\
	(pg)->mdpage.pvent.pv_pmap = NULL;	\
	(pg)->mdpage.pvent.pv_va = 0;		\
	(pg)->mdpage.pvent.pv_ptste = NULL;	\
	(pg)->mdpage.pvent.pv_ptpmap = NULL;	\
	(pg)->mdpage.pvent.pv_flags = 0;	\
} while (0)

#endif	/* _LOCORE */

#endif /* !_M68K_VMPARAM_H_ */
