/*	$OpenBSD: pte.h,v 1.20 2015/03/13 23:23:13 mlarkin Exp $	*/
/*	$NetBSD: pte.h,v 1.11 1998/02/06 21:58:05 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pte.h rewritten by chuck based on the jolitz version, plus random
 * info on the pentium and other processors found on the net.   the
 * goal of this rewrite is to provide enough documentation on the MMU
 * hardware that the reader will be able to understand it without having
 * to refer to a hardware manual.
 */

#ifndef _MACHINE_PTE_H_
#define _MACHINE_PTE_H_

#if !defined(_LOCORE)

/*
 * here we define the data types for PDEs and PTEs
 */

typedef u_int32_t pd_entry_t;		/* PDE */
typedef u_int32_t pt_entry_t;		/* PTE */

#endif

/*
 * now we define various for playing with virtual addresses
 */

#define	PDSHIFT		22		/* offset of PD index in VA */
#define	NBPD		(1 << PDSHIFT)	/* # bytes mapped by PD (4MB) */
#define	PDOFSET		(NBPD-1)	/* mask for non-PD part of VA */
#if 0 /* not used? */
#define	NPTEPD		(NBPD / PAGE_SIZE)	/* # of PTEs in a PD */
#else
#define	PTES_PER_PTP	(NBPD / PAGE_SIZE)	/* # of PTEs in a PTP */
#endif

#define PAGE_MASK_L2	(NBPD - 1)

#define	i386_round_pdr(x)	((((unsigned)(x)) + PDOFSET) & ~PDOFSET)

/*
 * here we define the bits of the PDE/PTE, as described above:
 *
 * XXXCDC: need to rename these (PG_u == ugly).
 */

#define	PG_V		0x00000001	/* valid entry */
#define	PG_RO		0x00000000	/* read-only page */
#define	PG_RW		0x00000002	/* read-write page */
#define	PG_u		0x00000004	/* user accessible page */
#define	PG_PROT		0x00000806	/* all protection bits */
#define	PG_WT		0x00000008	/* write through */
#define	PG_N		0x00000010	/* non-cacheable */
#define	PG_U		0x00000020	/* has been used */
#define	PG_M		0x00000040	/* has been modified */
#define	PG_PAT		0x00000080	/* PAT bit. (on pte) */
#define PG_PS		0x00000080	/* 4MB page size (on pde) */
#define PG_G		0x00000100	/* global, don't TLB flush */
#define PG_AVAIL1	0x00000200	/* ignored by hardware */
#define PG_AVAIL2	0x00000400	/* ignored by hardware */
#define PG_AVAIL3	0x00000800	/* ignored by hardware */
#define	PG_PATLG	0x00001000	/* PAT on large pages */
#define PG_FRAME	0xfffff000	/* page frame mask */
#define PG_LGFRAME	0xffc00000	/* large (4M) page frame mask */

/* Cacheability bits when we are using PAT */
#define	PG_WB		(0)		/* The default */
#define	PG_WC		(PG_WT)		/* WT and CD is WC */
#define	PG_UCMINUS	(PG_N)		/* UC but mtrr can override */
#define	PG_UC		(PG_WT | PG_N)	/* hard UC */

/*
 * various shorthand protection codes
 */

#define	PG_KR		0x00000000	/* kernel read-only */
#define	PG_KW		0x00000002	/* kernel read-write */

/*
 * page protection exception bits
 */

#define PGEX_P		0x01	/* protection violation (vs. no mapping) */
#define PGEX_W		0x02	/* exception during a write cycle */
#define PGEX_U		0x04	/* exception while in user mode (upl) */
#define PGEX_I		0x10	/* instruction fetch blocked by NX */

#endif /* _MACHINE_PTE_H_ */
