/*      $NetBSD: pmap.h,v 1.11 1995/11/12 14:41:41 ragge Exp $     */

/* 
 * Copyright (c) 1987 Carnegie-Mellon University
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * Changed for the VAX port. /IC
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
 *
 *	@(#)pmap.h	7.6 (Berkeley) 5/10/91
 */


#ifndef	PMAP_H
#define	PMAP_H

#include "machine/mtpr.h"


#define VAX_PAGE_SIZE	NBPG
#define VAX_SEG_SIZE	NBSEG

/*
 *  Pmap structure
 *
 * p0br == PR_P0BR in user struct, p0br is also == SBR in pmap_kernel()
 * p1br is the same for stack space, stack is base of alloced pte mem
 */

typedef struct pmap {
	vm_offset_t		 pm_stack; /* Base of alloced p1 pte space */
	struct pcb		*pm_pcb; /* Pointer to PCB for this pmap */
	int                      ref_count;   /* reference count        */
	struct pmap_statistics   stats;       /* statistics             */
	simple_lock_data_t       lock;        /* lock on pmap           */
} *pmap_t;

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */

typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;/* if not NULL, pmap where mapping lies */
	vm_offset_t	 pv_va;		/* virtual address for mapping */
	int		 pv_flags;	/* flags */
} *pv_entry_t;

#define	PV_REF	0x00000001	/* Simulated phys ref bit */

#define PHYS_TO_PV(phys_page) (&pv_table[((phys_page)>>PAGE_SHIFT)])

/* ROUND_PAGE used before vm system is initialized */
#define ROUND_PAGE(x)   (((uint)(x) + PAGE_SIZE-1)& ~(PAGE_SIZE - 1))

/* Mapping macros used when allocating SPT */
#define	MAPVIRT(ptr, count)					\
	(vm_offset_t)ptr = virtual_avail;			\
	virtual_avail += (count) * NBPG;

#define	MAPPHYS(ptr, count, perm)				\
	pmap_map(virtual_avail, avail_start, avail_start +	\
	    (count) * NBPG, perm);				\
	(vm_offset_t)ptr = virtual_avail;			\
	virtual_avail += (count) * NBPG;				\
	avail_start += (count) * NBPG;

#ifdef	_KERNEL
pv_entry_t	pv_table;		/* array of entries, 
					   one per LOGICAL page */
struct pmap	kernel_pmap_store;

#define pa_index(pa)	                atop(pa)
#define pa_to_pvh(pa)	                (&pv_table[atop(pa)])

#define	pmap_kernel()			(&kernel_pmap_store)

#endif	/* _KERNEL */

/* Routines that are best to define as macros */
#define	pmap_copy(a,b,c,d,e) 		/* Dont do anything */
#define	pmap_update()	mtpr(0,PR_TBIA)	/* Update buffes */
#define	pmap_pageable(a,b,c,d)		/* Dont do anything */
#define	pmap_collect(pmap)		/* No need so far */
#define	pmap_reference(pmap)	if(pmap) (pmap)->ref_count++
#define	pmap_pinit(pmap)	(pmap)->ref_count=1;
#define	pmap_phys_address(phys) ((u_int)(phys)<<PAGE_SIZE)

#endif PMAP_H
