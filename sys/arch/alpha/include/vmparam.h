/* $OpenBSD: vmparam.h,v 1.13 2004/11/28 01:36:38 mickey Exp $ */
/* $NetBSD: vmparam.h,v 1.18 2000/05/22 17:13:54 thorpej Exp $ */

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
 * from: Utah $Hdr: vmparam.h 1.16 91/01/18$
 *
 *	@(#)vmparam.h	8.2 (Berkeley) 4/22/94
 */

#ifndef	_ALPHA_VMPARAM_H_
#define	_ALPHA_VMPARAM_H_

/*
 * Machine dependent constants for Alpha.
 */

/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.  Immediately above the user stack
 * resides the user structure, which is UPAGES long and contains the
 * kernel stack.
 *
 * Digital UNIX (formerly DEC OSF/1) places the stack below the
 * text segment (i.e. growing downward from 4G).  We may want to
 * consider doing that at some point, but it might require changes
 * to the exec code.
 */
#define	USRTEXT		NBPG
#define	USRSTACK	((vaddr_t)0x0000000200000000)		/* 8G */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(1<<30)			/* max text size (1G) */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(1<<27)			/* initial data size (128M) */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(1<<30)			/* max data size (1G) */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(1<<21)			/* initial stack size (2M) */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(1<<25)			/* max stack size (32M) */
#endif

/*
 * PTEs for mapping user space into the kernel for phyio operations.
 * 64 pte's are enough to cover 8 disks * MAXBSIZE.
 */
#ifndef USRIOSIZE
#define USRIOSIZE	64
#endif

/*
 * PTEs for system V style shared memory.
 * This is basically slop for kmempt which we actually allocate (malloc) from.
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	4096		/* 32mb */
#endif

/*
 * Mach derived constants
 */

/* user/kernel map constants */
#define VM_MIN_ADDRESS		((vaddr_t)ALPHA_USEG_BASE)	    /* 0 */
#define VM_MAXUSER_ADDRESS	((vaddr_t)(ALPHA_USEG_END + 1L))    /* 4T */
#define VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)ALPHA_K1SEG_BASE)
#define VM_MAX_KERNEL_ADDRESS	((vaddr_t)ALPHA_K1SEG_END)

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(USRIOSIZE*NBPG)

/* some Alpha-specific constants */
#define	VPTBASE		((vaddr_t)0xfffffffc00000000)	/* Virt. pg table */

#define	VM_PHYSSEG_MAX		16		/* XXX */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define	VM_PHYSSEG_NOADD			/* no more after vm_mem_init */

#define	VM_NFREELIST		1
#define	VM_FREELIST_DEFAULT	0

/*
 * pmap-specific data stored in the vm_physmem[] array.
 */
#define __HAVE_PMAP_PHYSSEG
struct pmap_physseg {
	struct pv_head *pvhead;		/* pv list of this seg */
};

#endif	/* ! _ALPHA_VMPARAM_H_ */
