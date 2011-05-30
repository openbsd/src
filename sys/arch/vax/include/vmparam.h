/*	$OpenBSD: vmparam.h,v 1.31 2011/05/30 22:25:23 oga Exp $	*/
/*	$NetBSD: vmparam.h,v 1.32 2000/03/07 00:05:59 matt Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Slightly modified for the VAX port /IC
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
#ifndef _MACHINE_VMPARAM_H_
#define _MACHINE_VMPARAM_H_

/*
 * Machine dependent constants for VAX.
 */

/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack. Immediately above the user stack
 * resides kernel.
 * */

#define USRTEXT		NBPG
#define USRSTACK	KERNBASE

/*
 * Virtual memory related constants, all in bytes
 */

#ifndef MAXTSIZ
#define MAXTSIZ		(8*1024*1024)		/* max text size */
#endif
#ifndef MAXDSIZ
#define MAXDSIZ		(32*1024*1024)		/* max data size */
#endif
#ifndef MAXSSIZ
#define MAXSSIZ		(8*1024*1024)		/* max stack size */
#endif
#ifndef DFLDSIZ
#define DFLDSIZ		(4*1024*1024)		/* initial data size limit */
#endif
#ifndef DFLSSIZ
#define DFLSSIZ		(512*1024)		/* initial stack size limit */
#endif

#define STACKGAP_RANDOM	32*1024

#define BRKSIZ		(8*1024*1024)

/* 
 * Size of shared memory map
 */

#ifndef SHMMAXPGS
#define SHMMAXPGS	64		/* XXXX should be 1024 */
#endif

#define VM_PHYSSEG_MAX		1
#define VM_PHYSSEG_NOADD
#define VM_PHYSSEG_STRAT	VM_PSTRAT_RANDOM

/* MD round macros */
#define	vax_round_page(x) (((vaddr_t)(x) + VAX_PGOFSET) & ~VAX_PGOFSET)
#define	vax_trunc_page(x) ((vaddr_t)(x) & ~VAX_PGOFSET)

/* user/kernel map constants */
#define VM_MIN_ADDRESS		((vaddr_t)PAGE_SIZE)
#define VM_MAXUSER_ADDRESS	((vaddr_t)KERNBASE)
#define VM_MAX_ADDRESS		((vaddr_t)KERNBASE)
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)KERNBASE)
#define VM_MAX_KERNEL_ADDRESS	((vaddr_t)(0xC0000000))

#define	USRIOSIZE		(8 * VAX_NPTEPG)	/* 512MB */
#define	VM_PHYS_SIZE		(USRIOSIZE*VAX_NBPG)

/*
 * This should be in <machine/pmap.h>, but needs to be in this file
 * due to include ordering issues.
 */
#define	__HAVE_VM_PAGE_MD

struct vm_page_md {
	struct pv_entry *pv_head;
	int		 pv_attr;	/* write/modified bits */
};

#define	VM_MDPAGE_INIT(pg) \
	do { \
		(pg)->mdpage.pv_head = NULL; \
		(pg)->mdpage.pv_attr = 0; \
	} while (0)

#endif /* _MACHINE_VMPARAM_H_ */
