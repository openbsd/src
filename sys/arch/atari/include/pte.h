/*	$NetBSD: pte.h,v 1.2 1995/04/30 14:02:14 leo Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 *
 * from: Utah $Hdr: pte.h 1.11 89/09/03$
 *
 *	@(#)pte.h	7.3 (Berkeley) 5/8/91
 */

#ifndef _MACHINE_PTE_H_
#define _MACHINE_PTE_H_

/*
 * ATARI hardware segment/page table entries
 */

struct pte {
	u_int pte;
};

typedef u_int	pt_entry_t;

struct ste {
	u_int ste;
};

typedef u_int	st_entry_t;

#define	PT_ENTRY_NULL	((u_int *) 0)
#define	ST_ENTRY_NULL	((u_int *) 0)

#define	SG_V		0x00000002	/* segment is valid */
#define	SG_NV		0x00000000
#define	SG_PROT		0x00000004	/* access protection mask */
#define	SG_RO		0x00000004
#define	SG_RW		0x00000000
#define SG_U		0x00000008	/* modified bit (68040) */
#define	SG_FRAME	0xffffe000
#define SG_IMASK	0xff000000
#define SG_ISHIFT	24
#define SG_PMASK	0x00ffe000
#define SG_PSHIFT	13

/* 68040 additions */
#define SG4_MASK1	0xfe000000	/* pointer table 1 index mask */
#define SG4_SHIFT1	25
#define SG4_MASK2	0x01fc0000	/* pointer table 2 index mask */
#define	SG4_SHIFT2	18
#define SG4_MASK3	0x0003e000	/* page table index mask */
#define	SG4_SHIFT3	13
#define	SG4_ADDR1	0xfffffe00	/* pointer table address mask */
#define	SG4_ADDR2	0xffffff00	/* page table address mask */

#define	PG_V		0x00000001
#define	PG_NV		0x00000000
#define	PG_PROT		0x00000004
#define	PG_U		0x00000008
#define	PG_M		0x00000010
#define	PG_W		0x00000100
#define	PG_RO		0x00000004
#define	PG_RW		0x00000000
#define PG_FRAME	0xffffe000
#define	PG_CI		0x00000040
#define PG_SHIFT	13
#define	PG_PFNUM(x)	(((x) & PG_FRAME) >> PG_SHIFT)

/* 68040 additions */
#define PG_CMASK	0x00000060	/* cache mode mask */
#define PG_CWT		0x00000000	/* writethrough caching */
#define PG_CCB		0x00000020	/* copyback caching */
#define PG_CIS		0x00000040	/* cache inhibited serialized */
#define PG_CIN		0x00000060	/* cache inhibited nonserialized */
#define PG_SO		0x00000080	/* supervisor only */

#define ATARI_040RTSIZE		512	/* root (level 1) table size */
#define ATARI_040STSIZE		512	/* segment (level 2) table size */
#define	ATARI_040PTSIZE		128	/* page (level 3) table size */
#define ATARI_STSIZE		1024	/* segment table size */
/*
 * ATARI_MAX_COREUPT	maximum number of incore user page tables
 * ATARI_USER_PTSIZE	the number of bytes for user pagetables
 * ATARI_PTBASE		the VA start of the map from which upt's are allocated
 * ATARI_PTSIZE		the size of the map from which upt's are allocated
 * ATARI_KPTSIZE	size of kernel page table
 * ATARI_MAX_KPTSIZE	the most number of bytes for kpt pages
 * ATARI_MAX_PTSIZE	the number of bytes to map everything
 */
#define ATARI_MAX_COREUPT	1024
#define ATARI_UPTSIZE		roundup(VM_MAXUSER_ADDRESS / NPTEPG, NBPG)
#define ATARI_UPTBASE		0x10000000
#define ATARI_UPTMAXSIZE \
    roundup((ATARI_MAX_COREUPT * ATARI_UPTSIZE), NBPG)
#define ATARI_MAX_KPTSIZE \
    (ATARI_MAX_COREUPT * ATARI_UPTSIZE / NPTEPG)
#define ATARI_KPTSIZE \
    roundup((VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / NPTEPG, NBPG)
#define ATARI_MAX_PTSIZE	roundup(0xffffffff / NPTEPG, NBPG)

/*
 * Kernel virtual address to page table entry and to physical address.
 */
#define	kvtopte(va) \
	(&Sysmap[((unsigned)(va) - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT])
#define	ptetokv(pt) \
	((((u_int *)(pt) - Sysmap) << PGSHIFT) + VM_MIN_KERNEL_ADDRESS)
#define	kvtophys(va) \
	((kvtopte(va)->pg_pfnum << PGSHIFT) | ((int)(va) & PGOFSET))


#endif /* !_MACHINE_PTE_H_ */
