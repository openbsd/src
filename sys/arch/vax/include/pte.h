/*      $NetBSD: pte.h,v 1.7 1996/01/28 12:31:24 ragge Exp $      */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "machine/param.h"

#ifndef ASSEMBLER

/*
 * VAX page table entries
 */
struct pte {
	unsigned int	pg_pfn:21;	/* Page Frame Number or 0 */
	unsigned int	pg_u:1;         /* Uniform bit, does WHAT?? XXX */
	unsigned int	pg_w:1;         /* Wired bit */
	unsigned int	pg_sref:1;	/* Help for ref simulation */
	unsigned int	pg_ref:1;	/* Simulated reference bit */
	unsigned int	pg_z:1;		/* Zero DIGITAL = 0 */
	unsigned int	pg_m:1;	        /* Modify DIGITAL */
	unsigned int	pg_prot:4;     	/* reserved at zero */
	unsigned int	pg_v:1;		/* valid bit */
};


typedef struct pte	pt_entry_t;	/* Mach page table entry */

#endif ASSEMBLER

#define	PT_ENTRY_NULL	((pt_entry_t *) 0)

#define PG_V            0x80000000
#define PG_NV           0x00000000
#define PG_PROT         0x78000000
#define PG_RW           0x20000000
#define PG_KW           0x10000000
#define PG_KR           0x18000000
#define	PG_URKW		0x70000000
#define PG_RO           0x78000000
#define PG_NONE         0x00000000
#define PG_M            0x04000000
#define PG_REF          0x01000000
#define PG_SREF         0x00800000
#define PG_W            0x00400000
#define PG_U            0x00200000
#define PG_FRAME        0x001fffff
#define	PG_PFNUM(x)	((x) >> PGSHIFT)

#ifndef ASSEMBLER
extern pt_entry_t *Sysmap;
/*
 * Kernel virtual address to page table entry and to physical address.
 */
#endif

#define	kvtopte(va) \
	(&Sysmap[((unsigned)(va) & ~KERNBASE) >> PGSHIFT])
#define	ptetokv(pt) \
	((((pt_entry_t *)(pt) - Sysmap) << PGSHIFT) + 0x80000000)
#define	kvtophys(va) \
	(((kvtopte(va))->pg_pfn << PGSHIFT) | ((int)(va) & PGOFSET))
#define	uvtopte(va, pcb) \
	(((unsigned)va < 0x40000000) || ((unsigned)va > 0x40000000) ? \
	&((pcb->P0BR)[(unsigned)va >> PGSHIFT]) : \
	&((pcb->P1BR)[((unsigned)va & 0x3fffffff) >> PGSHIFT]))
