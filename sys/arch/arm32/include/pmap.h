/* $NetBSD: pmap.h,v 1.3 1996/03/14 23:11:29 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name "RiscBSD" nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RISCBSD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_ARM32_PMAP_H_
#define	_ARM32_PMAP_H_

#include <machine/pte.h>

/*
 * virtual address to page table entry and
 * to physical address.
 */

#define vtopte(va) \
	((pt_entry_t *)(PROCESS_PAGE_TBLS_BASE + (arm_byte_to_page((unsigned int)(va)) << 2)))
        
        
#define vtophys(va) \
	((*vtopte(va) & PG_FRAME) | ((unsigned int)(va) & ~PG_FRAME))

/*
 * Pmap stuff
 */
struct pmap {
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	vm_offset_t		pm_pptpt;	/* PA of pt's page table */
	vm_offset_t		pm_vptpt;	/* VA of pt's page table */
	boolean_t		pm_pdchanged;	/* pdir changed */
	short			pm_dref;	/* page directory ref count */
	short			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
};

typedef struct pmap *pmap_t;

/*
 * For speed we store the both the virtual address and the page table
 * entry address for each page hook.
 */
 
typedef struct {
	vm_offset_t va;
	pt_entry_t *pte;
} pagehook_t;
             
/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */

typedef struct pv_entry {
	struct pv_entry *pv_next;       /* next pv_entry */
	pmap_t          pv_pmap;        /* pmap where mapping lies */
	vm_offset_t     pv_va;          /* virtual address for mapping */
	int             pv_flags;       /* flags */
} *pv_entry_t;

struct pv_page;

struct pv_page_info {
	TAILQ_ENTRY(pv_page) pgi_list;
	struct pv_entry *pgi_freelist;
	int pgi_nfree;
};

/*
 * This is basically:
 * ((NBPG - sizeof(struct pv_page_info)) / sizeof(struct pv_entry))
 */
 
#define NPVPPG	((NBPG - sizeof(struct pv_page_info)) / sizeof(struct pv_entry))

struct pv_page {
	struct pv_page_info pvp_pgi;
	struct pv_entry pvp_pv[NPVPPG];
};

#ifdef  _KERNEL
pv_entry_t      pv_table;               /* array of entries, one per page */
extern pmap_t	kernel_pmap;
extern struct pmap	kernel_pmap_store;

#define pmap_kernel()	(&kernel_pmap_store)
#define pmap_update()	tlbflush()
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)

boolean_t pmap_testbit __P((vm_offset_t, int));
void pmap_changebit __P((vm_offset_t, int, int));

static __inline vm_offset_t
pmap_phys_address(int ppn)
{
        return(arm_page_to_byte(ppn));
}

#endif

#endif

/* End of pmap.h */
