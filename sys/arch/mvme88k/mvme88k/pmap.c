/*	$OpenBSD: pmap.c,v 1.68 2003/01/24 09:57:44 miod Exp $	*/
/*
 * Copyright (c) 2001, 2002, 2003 Miodrag Vallat
 * Copyright (c) 1998-2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
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
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/simplelock.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/msgbuf.h>
#include <sys/user.h>

#include <uvm/uvm.h>

#include <machine/asm_macro.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <machine/cpu_number.h>
#include <machine/pmap_table.h>

/*
 *  VM externals
 */
extern vaddr_t      avail_start, avail_end;
extern vaddr_t      virtual_avail, virtual_end;

/*
 * Macros to operate pm_cpus field
 */
#define SETBIT_CPUSET(cpu_number, cpuset) (*(cpuset)) |= (1 << (cpu_number));
#define CLRBIT_CPUSET(cpu_number, cpuset) (*(cpuset)) &= ~(1 << (cpu_number));

#ifdef	DEBUG
/*
 * Static variables, functions and variables for debugging
 */

/*
 * conditional debugging
 */
#define CD_NONE		0x00
#define CD_NORM		0x01
#define CD_FULL		0x02

#define CD_ACTIVATE	0x0000004	/* pmap_activate */
#define CD_KMAP		0x0000008	/* pmap_expand_kmap */
#define CD_MAP		0x0000010	/* pmap_map */
#define CD_CACHE	0x0000020	/* pmap_cache_ctrl */
#define CD_BOOT		0x0000040	/* pmap_bootstrap */
#define CD_INIT		0x0000080	/* pmap_init */
#define CD_CREAT	0x0000100	/* pmap_create */
#define CD_FREE		0x0000200	/* pmap_release */
#define CD_DESTR	0x0000400	/* pmap_destroy */
#define CD_RM		0x0000800	/* pmap_remove */
#define CD_RMAL		0x0001000	/* pmap_remove_all */
#define CD_PROT		0x0002000	/* pmap_protect */
#define CD_EXP		0x0004000	/* pmap_expand */
#define CD_ENT		0x0008000	/* pmap_enter */
#define CD_UPD		0x0010000	/* pmap_update */
#define CD_COL		0x0020000	/* pmap_collect */
#define CD_CBIT		0x0040000	/* pmap_changebit */
#define CD_TBIT		0x0080000	/* pmap_testbit */
#define CD_CREF		0x0100000	/* pmap_clear_reference */
#define CD_PGMV		0x0200000	/* pagemove */
#define CD_ALL		0x0FFFFFC

int pmap_con_dbg = CD_NONE;
#endif /* DEBUG */

struct pool pmappool, pvpool;

caddr_t vmmap;
pt_entry_t *vmpte, *msgbufmap;

struct pmap kernel_pmap_store;
pmap_t kernel_pmap = &kernel_pmap_store;

typedef struct kpdt_entry *kpdt_entry_t;
struct kpdt_entry {
	kpdt_entry_t	next;
	paddr_t		phys;
};
#define	KPDT_ENTRY_NULL		((kpdt_entry_t)0)

kpdt_entry_t	kpdt_free;

/*
 * MAX_KERNEL_VA_SIZE must fit into the virtual address space between
 * VM_MIN_KERNEL_ADDRESS and VM_MAX_KERNEL_ADDRESS.
 */

#define	MAX_KERNEL_VA_SIZE	(256*1024*1024)	/* 256 Mb */

/*
 * Size of kernel page tables, which is enough to map MAX_KERNEL_VA_SIZE
 */
#define	KERNEL_PDT_SIZE	(atop(MAX_KERNEL_VA_SIZE) * sizeof(pt_entry_t))

/*
 * Size of kernel page tables for mapping onboard IO space.
 */
#if defined(MVME188)
#define	M188_PDT_SIZE	(atop(UTIL_SIZE) * sizeof(pt_entry_t))
#else
#define	M188_PDT_SIZE 0
#endif

#if defined(MVME187) || defined(MVME197)
#define	M1x7_PDT_SIZE	(atop(OBIO_SIZE) * sizeof(pt_entry_t))
#else
#define	M1x7_PDT_SIZE 0
#endif

#if defined(MVME188) && defined(MVME187) || defined(MVME197)
#define	OBIO_PDT_SIZE	((brdtyp == BRD_188) ? M188_PDT_SIZE : M1x7_PDT_SIZE)
#else
#define	OBIO_PDT_SIZE	MAX(M188_PDT_SIZE, M1x7_PDT_SIZE)
#endif

#define MAX_KERNEL_PDT_SIZE	(KERNEL_PDT_SIZE + OBIO_PDT_SIZE)

/*
 * Two pages of scratch space.
 * Used in pmap_copy_page() and pmap_zero_page().
 */
vaddr_t phys_map_vaddr1, phys_map_vaddr2;

#define PV_ENTRY_NULL	((pv_entry_t) 0)

static pv_entry_t pg_to_pvh(struct vm_page *);

static __inline pv_entry_t
pg_to_pvh(struct vm_page *pg)
{
	return &pg->mdpage.pvent;
}

/*
 *	Locking primitives
 */

/*
 *	We raise the interrupt level to splvm, to block interprocessor
 *	interrupts during pmap operations.
 */
#define	SPLVM(spl)	spl = splvm();
#define	SPLX(spl)	splx(spl);

#define PMAP_LOCK(pmap,spl) \
	do { \
		SPLVM(spl); \
		simple_lock(&(pmap)->pm_lock); \
	} while (0)
#define PMAP_UNLOCK(pmap, spl) \
	do { \
		simple_unlock(&(pmap)->pm_lock); \
		SPLX(spl); \
	} while (0)

#define ETHERPAGES 16
void *etherbuf = NULL;
int etherlen;

#ifdef	PMAP_USE_BATC

/*
 * number of BATC entries used
 */
int batc_used;

/*
 * keep track BATC mapping
 */
batc_entry_t batc_entry[BATC_MAX];

#endif	/* PMAP_USE_BATC */

vaddr_t kmapva = 0;
extern vaddr_t bugromva;
extern vaddr_t sramva;
extern vaddr_t obiova;

/*
 * Internal routines
 */
void flush_atc_entry(long, vaddr_t, boolean_t);
pt_entry_t *pmap_expand_kmap(vaddr_t, vm_prot_t);
void pmap_remove_range(pmap_t, vaddr_t, vaddr_t);
void pmap_expand(pmap_t, vaddr_t);
void pmap_release(pmap_t);
vaddr_t pmap_map(vaddr_t, paddr_t, paddr_t, vm_prot_t, u_int);
pt_entry_t *pmap_pte(pmap_t, vaddr_t);
void pmap_remove_all(struct vm_page *);
void pmap_changebit(struct vm_page *, int, int);
boolean_t pmap_testbit(struct vm_page *, int);

/*
 * quick PTE field checking macros
 */
#define	pmap_pte_w(pte)		(*(pte) & PG_W)
#define	pmap_pte_m(pte)		(*(pte) & PG_M)
#define	pmap_pte_u(pte)		(*(pte) & PG_U)
#define	pmap_pte_prot(pte)	(*(pte) & PG_PROT)

#define	pmap_pte_w_chg(pte, nw)		((nw) ^ pmap_pte_w(pte))
#define	pmap_pte_prot_chg(pte, np)	((np) ^ pmap_pte_prot(pte))

/*
 * Convert machine-independent protection code to M88K protection bits.
 */
static __inline u_int32_t
m88k_protection(pmap_t pmap, vm_prot_t prot)
{
	pt_entry_t p;

	p = (prot & VM_PROT_WRITE) ? PG_RW : PG_RO;
	/*
	 * XXX this should not be necessary anymore now that pmap_enter
	 * does the correct thing... -- miod
	 */
#ifdef M88110
	if (cputyp == CPU_88110) {
		p |= PG_U;
		/* if the map is the kernel's map and since this
		 * is not a paged kernel, we go ahead and mark
		 * the page as modified to avoid an exception
		 * upon writing to the page the first time.  XXX smurph
		 */
		if (pmap == kernel_pmap) {
			if (p & PG_PROT)
				p |= PG_M;
		}
	}
#endif
	return p;
}

/*
 * Routine:	FLUSH_ATC_ENTRY
 *
 * Function:
 *	Flush atc(TLB) which maps given virtual address, in the CPUs which
 *	are specified by 'users', for the operating mode specified by
 *      'kernel'.
 *
 * Parameters:
 *	users	bit patterns of the CPUs which may hold the TLB, and
 *		should be flushed
 *	va	virtual address that should be flushed
 *      kernel  TRUE if supervisor mode, FALSE if user mode
 */
void
flush_atc_entry(long users, vaddr_t va, boolean_t kernel)
{
	int cpu;
	long tusers = users;

#ifdef DIAGNOSTIC
	if ((tusers != 0) && (ff1(tusers) >= MAX_CPUS)) {
		printf("ff1 users = %d!\n", ff1(tusers));
		panic("bogus amount of users!!!");
	}
#endif

	while ((cpu = ff1(tusers)) != 32) {
		if (cpu_sets[cpu]) { /* just checking to make sure */
			cmmu_flush_remote_tlb(cpu, kernel, va, PAGE_SIZE);
		}
		tusers &= ~(1 << cpu);
	}
}

/*
 * Routine:	PMAP_PTE
 *
 * Function:
 *	Given a map and a virtual address, compute a (virtual) pointer
 *	to the page table entry (PTE) which maps the address .
 *	If the page table associated with the address does not
 *	exist, PT_ENTRY_NULL is returned (and the map may need to grow).
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	virt	virtual address for which page table entry is desired
 *
 *    Otherwise the page table address is extracted from the segment table,
 *    the page table index is added, and the result is returned.
 */
pt_entry_t *
pmap_pte(pmap_t pmap, vaddr_t virt)
{
	sdt_entry_t *sdt;

#ifdef DIAGNOSTIC
	/*XXX will this change if physical memory is not contiguous? */
	/* take a look at PDTIDX XXXnivas */
	if (pmap == PMAP_NULL)
		panic("pmap_pte: pmap is NULL");
#endif

	sdt = SDTENT(pmap,virt);
	/*
	 * Check whether page table is exist or not.
	 */
	if (!SDT_VALID(sdt))
		return (PT_ENTRY_NULL);

	return (pt_entry_t *)(PG_PFNUM(*(sdt + SDT_ENTRIES)) << PDT_SHIFT) +
		PDTIDX(virt);
}

/*
 * Routine:	PMAP_EXPAND_KMAP (internal)
 *
 * Function:
 *    Allocate a page descriptor table (pte_table) and validate associated
 * segment table entry, returning pointer to page table entry. This is
 * much like 'pmap_expand', except that table space is acquired
 * from an area set up by pmap_bootstrap, instead of through
 * uvm_km_zalloc. (Obviously, because uvm_km_zalloc uses the kernel map
 * for allocation - which we can't do when trying to expand the
 * kernel map!) Note that segment tables for the kernel map were
 * all allocated at pmap_bootstrap time, so we only need to worry
 * about the page table here.
 *
 * Parameters:
 *	virt	VA for which translation tables are needed
 *	prot	protection attributes for segment entries
 *
 * Extern/Global:
 *	kpdt_free	kernel page table free queue
 *
 * Calls:
 *	m88k_protection
 *
 * This routine simply dequeues a table from the kpdt_free list,
 * initializes all its entries (invalidates them), and sets the
 * corresponding segment table entry to point to it. If the kpdt_free
 * list is empty - we panic (no other places to get memory, sorry). (Such
 * a panic indicates that pmap_bootstrap is not allocating enough table
 * space for the kernel virtual address space).
 *
 */
pt_entry_t *
pmap_expand_kmap(vaddr_t virt, vm_prot_t prot)
{
	sdt_entry_t template, *sdt;
	kpdt_entry_t kpdt_ent;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_KMAP | CD_FULL)) == (CD_KMAP | CD_FULL))
		printf("(pmap_expand_kmap: %x) v %x\n", curproc,virt);
#endif

	template = m88k_protection(kernel_pmap, prot) | SG_V;

	/* segment table entry derivate from map and virt. */
	sdt = SDTENT(kernel_pmap, virt);
	if (SDT_VALID(sdt))
		panic("pmap_expand_kmap: segment table entry VALID");

	kpdt_ent = kpdt_free;
	if (kpdt_ent == KPDT_ENTRY_NULL)
		panic("pmap_expand_kmap: Ran out of kernel pte tables");

	kpdt_free = kpdt_free->next;
	/* physical table */
	*sdt = kpdt_ent->phys | template;
	/* virtual table */
	*(sdt + SDT_ENTRIES) = (vaddr_t)kpdt_ent | template;
	kpdt_ent->phys = (paddr_t)0;
	kpdt_ent->next = NULL;

	return (pt_entry_t *)(kpdt_ent) + PDTIDX(virt);
}

/*
 * Routine:	PMAP_MAP
 *
 * Function:
 *    Map memory at initialization. The physical addresses being
 * mapped are not managed and are never unmapped.
 *
 * Parameters:
 *	virt	virtual address of range to map
 *	start	physical address of range to map
 *	end	physical address of end of range
 *	prot	protection attributes
 *	cmode	cache control attributes
 *
 * Calls:
 *	pmap_pte
 *	pmap_expand_kmap
 *
 * Special Assumptions
 *	For now, VM is already on, only need to map the specified
 * memory. Used only by pmap_bootstrap() and vm_page_startup().
 *
 * For each page that needs mapping:
 *	pmap_pte is called to obtain the address of the page table
 *	table entry (PTE). If the page table does not exist,
 *	pmap_expand_kmap is called to allocate it. Finally, the page table
 *	entry is set to point to the physical page.
 *
 *	initialize template with paddr, prot, dt
 *	look for number of phys pages in range
 *	{
 *		pmap_pte(virt)	- expand if necessary
 *		stuff pte from template
 *		increment virt one page
 *		increment template paddr one page
 *	}
 *
 */
vaddr_t
pmap_map(vaddr_t virt, paddr_t start, paddr_t end, vm_prot_t prot, u_int cmode)
{
	u_int npages;
	u_int num_phys_pages;
	pt_entry_t template, *pte;
	paddr_t	 page;
#ifdef	PMAP_USE_BATC
	batc_template_t	batctmp;
	int i;
#endif

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_MAP | CD_NORM)) == (CD_MAP | CD_NORM))
		printf ("(pmap_map: %x) phys address from %x to %x mapped at virtual %x, prot %x cmode %x\n",
			curproc, start, end, virt, prot, cmode);
#endif

#ifdef DIAGNOSTIC
	if (start > end)
		panic("pmap_map: start greater than end address");
#endif

	template = m88k_protection(kernel_pmap, prot) | cmode | PG_V;

#ifdef	PMAP_USE_BATC
	batctmp.bits = 0;
	batctmp.field.sup = 1;	     /* supervisor */
	if (template & CACHE_WT)
		batctmp.field.wt = 1;	 /* write through */
	if (template & CACHE_GLOBAL)
		batctmp.field.g = 1;     /* global */
	if (template & CACHE_INH)
		batctmp.field.ci = 1;	 /* cache inhibit */
	if (template & PG_PROT)
		batctmp.field.wp = 1; /* protection */
	batctmp.field.v = 1;	     /* valid */
#endif

	page = trunc_page(start);
	npages = atop(round_page(end) - page);
	for (num_phys_pages = npages; num_phys_pages != 0; num_phys_pages--) {
#ifdef	PMAP_USE_BATC

#ifdef DEBUG
		if ((pmap_con_dbg & (CD_MAP | CD_FULL)) == (CD_MAP | CD_FULL))
			printf("(pmap_map: %x) num_phys_pg=%x, virt=%x, "
			    "align V=%d, page=%x, align P=%d\n",
			    curproc, num_phys_pages, virt,
			    BATC_BLK_ALIGNED(virt), page,
			    BATC_BLK_ALIGNED(page));
#endif

		if (BATC_BLK_ALIGNED(virt) && BATC_BLK_ALIGNED(page) &&
		     num_phys_pages >= BATC_BLKBYTES/PAGE_SIZE &&
		     batc_used < BATC_MAX ) {
			/*
			 * map by BATC
			 */
			batctmp.field.lba = M88K_BTOBLK(virt);
			batctmp.field.pba = M88K_BTOBLK(page);

			for (i = 0; i < MAX_CPUS; i++)
				if (cpu_sets[i])
					cmmu_set_pair_batc_entry(i, batc_used,
								 batctmp.bits);
			batc_entry[batc_used] = batctmp.field;
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_MAP | CD_NORM)) == (CD_MAP | CD_NORM)) {
				printf("(pmap_map: %x) BATC used=%d, data=%x\n", curproc, batc_used, batctmp.bits);
			}
			if (pmap_con_dbg & CD_MAP)
				for (i = 0; i < BATC_BLKBYTES; i += PAGE_SIZE) {
					pte = pmap_pte(kernel_pmap, virt + i);
					if (PDT_VALID(pte))
						printf("(pmap_map: %x) va %x is already mapped: pte %x\n",
						    curproc, virt + i, *pte);
				}
#endif
			batc_used++;
			virt += BATC_BLKBYTES;
			page += BATC_BLKBYTES;
			num_phys_pages -= BATC_BLKBYTES/PAGE_SIZE;
			continue;
		}
#endif	/* PMAP_USE_BATC */
	
		if ((pte = pmap_pte(kernel_pmap, virt)) == PT_ENTRY_NULL)
			pte = pmap_expand_kmap(virt,
			    VM_PROT_READ | VM_PROT_WRITE);

#ifdef DEBUG
		if ((pmap_con_dbg & (CD_MAP | CD_FULL)) == (CD_MAP | CD_FULL))
			if (PDT_VALID(pte))
				printf("(pmap_map: %x) pte @ 0x%p already valid\n", curproc, pte);
#endif

		*pte = template | page;
		virt += PAGE_SIZE;
		page += PAGE_SIZE;
	}
	return virt;
}

/*
 * Routine:	PMAP_CACHE_CONTROL
 *
 * Function:
 *	Set the cache-control bits in the page table entries(PTE) which maps
 *	the specified virtual address range.
 *
 * Parameters:
 *	pmap_t		pmap
 *	vaddr_t		s
 *	vaddr_t		e
 *	u_int		mode
 *
 * Calls:
 *	pmap_pte
 *	invalidate_pte
 *	flush_atc_entry
 *
 *  This routine sequences through the pages of the specified range.
 * For each, it calls pmap_pte to acquire a pointer to the page table
 * entry (PTE). If the PTE is invalid, or non-existant, nothing is done.
 * Otherwise, the cache-control bits in the PTE's are adjusted as specified.
 *
 */
void
pmap_cache_ctrl(pmap_t pmap, vaddr_t s, vaddr_t e, u_int mode)
{
	int spl;
	pt_entry_t *pte;
	vaddr_t va, pteva;
	boolean_t kflush;
	int cpu;
	u_int users;

#ifdef DEBUG
	if (mode & CACHE_MASK) {
		printf("(cache_ctrl) illegal mode %x\n",mode);
		return;
	}
	if ((pmap_con_dbg & (CD_CACHE | CD_NORM)) == (CD_CACHE | CD_NORM)) {
		printf("(pmap_cache_ctrl: %x) pmap %x, va %x, mode %x\n", curproc, pmap, s, mode);
	}
#endif /* DEBUG */

#ifdef DIAGNOSTIC
	if (pmap == PMAP_NULL)
		panic("pmap_cache_ctrl: pmap is NULL");
#endif

	PMAP_LOCK(pmap, spl);

	users = pmap->pm_cpus;
	if (pmap == kernel_pmap) {
		kflush = TRUE;
	} else {
		kflush = FALSE;
	}

	for (va = s; va < e; va += PAGE_SIZE) {
		if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
			continue;
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_CACHE | CD_NORM)) == (CD_CACHE | CD_NORM)) {
			printf("(cache_ctrl) pte@0x%p\n", pte);
		}
#endif /* DEBUG */
		/*
		 * Invalidate pte temporarily to avoid being written back
		 * the modified bit and/or the reference bit by any other cpu.
		 * XXX
		 */
		*pte = (invalidate_pte(pte) & CACHE_MASK) | mode;
		flush_atc_entry(users, va, kflush);

		/*
		 * Data cache should be copied back and invalidated.
		 */
		pteva = ptoa(PG_PFNUM(*pte));
		for (cpu = 0; cpu < MAX_CPUS; cpu++)
			if (cpu_sets[cpu])
				cmmu_flush_remote_cache(cpu, pteva, PAGE_SIZE);
	}
	PMAP_UNLOCK(pmap, spl);
}

/*
 * Routine:	PMAP_BOOTSTRAP
 *
 * Function:
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, allocate the kernel
 *	translation table space, and map control registers
 *	and other IO addresses.
 *
 * Parameters:
 *	load_start	PA where kernel was loaded
 *	&phys_start	PA of first available physical page
 *	&phys_end	PA of last available physical page
 *	&virtual_avail	VA of first available page (after kernel bss)
 *	&virtual_end	VA of last available page (end of kernel address space)
 *
 * Extern/Global:
 *
 *	PAGE_SIZE	VM (software) page size
 *	kernelstart	start symbol of kernel text
 *	etext		end of kernel text
 *	phys_map_vaddr1 VA of page mapped arbitrarily for debug/IO
 *	phys_map_vaddr2 VA of page mapped arbitrarily for debug/IO
 *
 * Calls:
 *	simple_lock_init
 *	pmap_map
 *
 *    The physical address 'load_start' is mapped at
 * VM_MIN_KERNEL_ADDRESS, which maps the kernel code and data at the
 * virtual address for which it was (presumably) linked. Immediately
 * following the end of the kernel code/data, sufficient page of
 * physical memory are reserved to hold translation tables for the kernel
 * address space. The 'phys_start' parameter is adjusted upward to
 * reflect this allocation. This space is mapped in virtual memory
 * immediately following the kernel code/data map.
 *
 *    A pair of virtual pages are reserved for debugging and IO
 * purposes. They are arbitrarily mapped when needed. They are used,
 * for example, by pmap_copy_page and pmap_zero_page.
 *
 * For m88k, we have to map BUG memory also. This is a read only
 * mapping for 0x10000 bytes. We will end up having load_start as
 * 0 and VM_MIN_KERNEL_ADDRESS as 0 - yes sir, we have one-to-one
 * mapping!!!
 */

void
pmap_bootstrap(vaddr_t load_start, paddr_t *phys_start, paddr_t *phys_end,
    vaddr_t *virt_start, vaddr_t *virt_end)
{
	kpdt_entry_t kpdt_virt;
	sdt_entry_t *kmap;
	vaddr_t vaddr, virt, kernel_pmap_size, pdt_size;
	paddr_t s_text, e_text, kpdt_phys;
	apr_template_t apr_data;
	pt_entry_t *pte;
	int i;
	pmap_table_t ptable;
	extern void *kernelstart, *etext;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_NORM)) == (CD_BOOT | CD_NORM)) {
		printf("pmap_bootstrap: \"load_start\" 0x%x\n", load_start);
	}
#endif
#ifdef DIAGNOSTIC
	if (!PAGE_ALIGNED(load_start))
		panic("pmap_bootstrap: \"load_start\" not on the m88k page boundary: 0x%x", load_start);
#endif

	simple_lock_init(&kernel_pmap->pm_lock);

	/*
	 * Allocate the kernel page table from the front of available
	 * physical memory, i.e. just after where the kernel image was loaded.
	 */
	/*
	 * The calling sequence is
	 *    ...
	 *  pmap_bootstrap(&kernelstart,...);
	 *  kernelstart is the first symbol in the load image.
	 *  We link the kernel such that &kernelstart == 0x10000 (size of
	 *							BUG ROM)
	 *  The expression (&kernelstart - load_start) will end up as
	 *	0, making *virt_start == *phys_start, giving a 1-to-1 map)
	 */

	*phys_start = round_page(*phys_start);
	*virt_start = *phys_start +
	    (trunc_page((vaddr_t)&kernelstart) - load_start);

	/*
	 * Initialize kernel_pmap structure
	 */
	kernel_pmap->pm_count = 1;
	kernel_pmap->pm_cpus = 0;
	kernel_pmap->pm_stpa = kmap = (sdt_entry_t *)(*phys_start);
	kernel_pmap->pm_stab = (sdt_entry_t *)(*virt_start);
	kmapva = *virt_start;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("kernel_pmap->pm_stpa = 0x%x\n",kernel_pmap->pm_stpa);
		printf("kernel_pmap->pm_stab = 0x%x\n",kernel_pmap->pm_stab);
	}
#endif

	/*
	 * Reserve space for segment table entries.
	 * One for the regular segment table and one for the shadow table
	 * The shadow table keeps track of the virtual address of page
	 * tables. This is used in virtual-to-physical address translation
	 * functions. Remember, MMU cares only for physical addresses of
	 * segment and page table addresses. For kernel page tables, we
	 * really don't need this virtual stuff (since the kernel will
	 * be mapped 1-to-1) but for user page tables, this is required.
	 * Just to be consistent, we will maintain the shadow table for
	 * kernel pmap also.
	 */
	kernel_pmap_size = 2 * SDT_SIZE;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("     kernel segment start = 0x%x\n", kernel_pmap->pm_stpa);
		printf("kernel segment table size = 0x%x\n", kernel_pmap_size);
		printf("       kernel segment end = 0x%x\n", ((paddr_t)kernel_pmap->pm_stpa) + kernel_pmap_size);
	}
#endif
	/* init all segment descriptors to zero */
	bzero(kernel_pmap->pm_stab, kernel_pmap_size);

	*phys_start += kernel_pmap_size;
	*virt_start += kernel_pmap_size;
	
	/* make sure page tables are page aligned!! XXX smurph */
	*phys_start = round_page(*phys_start);
	*virt_start = round_page(*virt_start);
	
	/* save pointers to where page table entries start in physical memory */
	kpdt_phys = *phys_start;
	kpdt_virt = (kpdt_entry_t)*virt_start;
	
	/* might as well round up to a page - XXX smurph */
	pdt_size = round_page(MAX_KERNEL_PDT_SIZE);
	kernel_pmap_size += pdt_size;
	*phys_start += pdt_size;
	*virt_start += pdt_size;

	/* init all page descriptors to zero */
	bzero((void *)kpdt_phys, pdt_size);
#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("--------------------------------------\n");
		printf("        kernel page start = 0x%x\n", kpdt_phys);
		printf("   kernel page table size = 0x%x\n", pdt_size);
		printf("          kernel page end = 0x%x\n", *phys_start);
	}
#endif

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("kpdt_phys = 0x%x\n",kpdt_phys);
		printf("kpdt_virt = 0x%x\n",kpdt_virt);
		printf("end of kpdt at (virt)0x%08x, (phys)0x%08x\n",
		       *virt_start,*phys_start);
	}
#endif
	/*
	 * init the kpdt queue
	 */
	kpdt_free = kpdt_virt;
	for (i = pdt_size / PDT_SIZE; i != 0; i--) {
		kpdt_virt->next = (kpdt_entry_t)((vaddr_t)kpdt_virt + PDT_SIZE);
		kpdt_virt->phys = kpdt_phys;
		kpdt_virt = kpdt_virt->next;
		kpdt_phys += PDT_SIZE;
	}
	kpdt_virt->next = KPDT_ENTRY_NULL; /* terminate the list */

	/*
	 * Map the kernel image into virtual space
	 */

	s_text = load_start;	     /* paddr of text */
	e_text = load_start +
	    ((vaddr_t)&etext - trunc_page((vaddr_t)&kernelstart));
	/* paddr of end of text section*/
	e_text = round_page(e_text);

	/* map the first 64k (BUG ROM) read only, cache inhibited (? XXX) */
	vaddr = pmap_map(0, 0, 0x10000, VM_PROT_WRITE | VM_PROT_READ,
	    CACHE_INH);

	/* map the kernel text read only */
	vaddr = pmap_map(trunc_page((vaddr_t)&kernelstart),
	    s_text, e_text, VM_PROT_WRITE | VM_PROT_READ,
	    CACHE_GLOBAL);  /* shouldn't it be RO? XXX*/

	vaddr = pmap_map(vaddr, e_text, (paddr_t)kmap,
	    VM_PROT_WRITE | VM_PROT_READ, CACHE_GLOBAL);

	/*
	 * Map system segment & page tables - should be cache inhibited?
	 * 88200 manual says that CI bit is driven on the Mbus while accessing
	 * the translation tree. I don't think we need to map it CACHE_INH
	 * here...
	 */
	if (kmapva != vaddr) {
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
			printf("(pmap_bootstrap) correcting vaddr\n");
		}
#endif
		while (vaddr < (*virt_start - kernel_pmap_size))
			vaddr = round_page(vaddr + 1);
	}
	vaddr = pmap_map(vaddr, (paddr_t)kmap, *phys_start,
	    VM_PROT_WRITE | VM_PROT_READ, CACHE_INH);

	if (vaddr != *virt_start) {
		/*
		 * This should never happen because we now round the PDT
		 * table size up to a page boundry in the quest to get
		 * mc88110 working. - XXX smurph
		 */
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
			printf("1: vaddr %x *virt_start 0x%x *phys_start 0x%x\n", vaddr,
			       *virt_start, *phys_start);
		}
#endif
		*virt_start = vaddr;
		*phys_start = round_page(*phys_start);
	}

#if defined (MVME187) || defined (MVME197)
	/*
	 *  Get ethernet buffer - need etherlen bytes physically contiguous.
	 *  1 to 1 mapped as well???. There is actually a bug in the macros
	 *  used by the 1x7 ethernet driver. Remove this when that is fixed.
	 *  XXX -nivas
	 */
	if (brdtyp == BRD_187 || brdtyp == BRD_197) {
		*phys_start = vaddr;
		etherlen = ETHERPAGES * NBPG;
		etherbuf = (void *)vaddr;

		vaddr = pmap_map(vaddr, *phys_start, *phys_start + etherlen,
		    VM_PROT_WRITE | VM_PROT_READ, CACHE_INH);

		*virt_start += etherlen;
		*phys_start += etherlen;

		if (vaddr != *virt_start) {
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
				printf("2: vaddr %x *virt_start %x *phys_start %x\n", vaddr,
				       *virt_start, *phys_start);
			}
#endif
			*virt_start = vaddr;
			*phys_start = round_page(*phys_start);
		}
	}

#endif /* defined (MVME187) || defined (MVME197) */

	*virt_start = round_page(*virt_start);
	*virt_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Map a few more pages for phys routines and debugger.
	 */

	phys_map_vaddr1 = round_page(*virt_start);
	phys_map_vaddr2 = phys_map_vaddr1 + PAGE_SIZE * max_cpus;

	/*
	 * To make 1:1 mapping of virt:phys, throw away a few phys pages.
	 * XXX what is this? nivas
	 */

	*phys_start += 2 * PAGE_SIZE * max_cpus;
	*virt_start += 2 * PAGE_SIZE * max_cpus;

	/*
	 * Map all IO space 1-to-1. Ideally, I would like to not do this
	 * but have va for the given IO address dynamically allocated. But
	 * on the 88200, 2 of the BATCs are hardwired to map the IO space
	 * 1-to-1; I decided to map the rest of the IO space 1-to-1.
	 * And bug ROM & the SRAM need to be mapped 1-to-1 if we ever want to
	 * execute bug system calls after the MMU has been turned on.
	 * OBIO should be mapped cache inhibited.
	 */

	ptable = pmap_table_build(0);
#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("pmap_bootstrap: -> pmap_table_build\n");
	}
#endif

	for (; ptable->size != (size_t)(-1); ptable++){
		if (ptable->size) {
			/*
			 * size-1, 'cause pmap_map rounds up to next pagenumber
			 */
			pmap_map(ptable->virt_start, ptable->phys_start,
			    ptable->phys_start + (ptable->size - 1),
			    ptable->prot, ptable->cacheability);
		}
	}

	/*
	 * Allocate all the submaps we need. Note that SYSMAP just allocates
	 * kernel virtual address with no physical backing memory. The idea
	 * is physical memory will be mapped at this va before using that va.
	 * This means that if different physical pages are going to be mapped
	 * at different times, we better do a tlb flush before using it -
	 * else we will be referencing the wrong page.
	 */

#define	SYSMAP(c, p, v, n)	\
({ \
	v = (c)virt; \
    	if ((p = pmap_pte(kernel_pmap, virt)) == PT_ENTRY_NULL) \
    		pmap_expand_kmap(virt, VM_PROT_READ | VM_PROT_WRITE); \
	virt += ((n) * PAGE_SIZE); \
})

	virt = *virt_start;

	SYSMAP(caddr_t, vmpte, vmmap, 1);
	*vmpte = PG_NV;

	SYSMAP(struct msgbuf *, msgbufmap, msgbufp, btoc(MSGBUFSIZE));

	*virt_start = virt;

	/*
	 * Set translation for UPAGES at UADDR. The idea is we want to
	 * have translations set up for UADDR. Later on, the ptes for
	 * for this address will be set so that kstack will refer
	 * to the u area. Make sure pmap knows about this virtual
	 * address by doing vm_findspace on kernel_map.
	 */

	for (i = 0, virt = UADDR; i < UPAGES; i++, virt += PAGE_SIZE) {
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
			printf("setting up mapping for Upage %d @ %x\n", i, virt);
		}
#endif
		if ((pte = pmap_pte(kernel_pmap, virt)) == PT_ENTRY_NULL)
			pmap_expand_kmap(virt, VM_PROT_READ | VM_PROT_WRITE);
	}
	/*
	 * Switch to using new page tables
	 */

	apr_data.bits = 0;
	apr_data.field.st_base = atop(kernel_pmap->pm_stpa);
	apr_data.field.wt = 1;
	apr_data.field.g  = 1;
	apr_data.field.ci = 0;
	apr_data.field.te = 1; /* Translation enable */
#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		show_apr(apr_data.bits);
	}
#endif
	/* Invalidate entire kernel TLB. */
#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("invalidating tlb %x\n", apr_data.bits);
	}
#endif

	for (i = 0; i < MAX_CPUS; i++)
		if (cpu_sets[i]) {
			/* Invalidate entire kernel TLB. */
			cmmu_flush_remote_tlb(i, 1, 0, -1);
			/* still physical */
			/*
			 * Set valid bit to DT_INVALID so that the very first
			 * pmap_enter() on these won't barf in
			 * pmap_remove_range().
			 */
			pte = pmap_pte(kernel_pmap, phys_map_vaddr1);
			*pte = PG_NV;
			pte = pmap_pte(kernel_pmap, phys_map_vaddr2);
			*pte = PG_NV;
			/* Load supervisor pointer to segment table. */
			cmmu_remote_set_sapr(i, apr_data.bits);
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
				printf("Processor %d running virtual.\n", i);
			}
#endif
			SETBIT_CPUSET(i, &kernel_pmap->pm_cpus);
		}

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("running virtual - avail_next 0x%x\n", *phys_start);
	}
#endif
}

/*
 * Routine:	PMAP_INIT
 *
 * Function:
 *	Initialize the pmap module. It is called by vm_init, to initialize
 *	any structures that the pmap system needs to map virtual memory.
 *
 * Parameters:
 *	phys_start	physical address of first available page
 *			(was last set by pmap_bootstrap)
 *	phys_end	physical address of last available page
 *
 * Extern/Globals
 *	pmap_phys_start
 *	pmap_phys_end
 *
 * Calls:
 *	pool_init
 *
 *   This routine does not really have much to do. It initializes
 * pools for pmap structures and pv_entry structures.
 */
void
pmap_init(void)
{
#ifdef DEBUG
	if ((pmap_con_dbg & (CD_INIT | CD_NORM)) == (CD_INIT | CD_NORM))
		printf("pmap_init()\n");
#endif

	pool_init(&pmappool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pvpool, sizeof(pv_entry_t), 0, 0, 0, "pvpl", NULL);
} /* pmap_init() */

/*
 * Routine:	PMAP_ZERO_PAGE
 *
 * Function:
 *	Zeroes the specified page.
 *
 * Parameters:
 *	pg		page to zero
 *
 * Extern/Global:
 *	phys_map_vaddr1
 *
 * Calls:
 *	m88k_protection
 *
 * Special Assumptions:
 *	no locking required
 *
 *	This routine maps the physical pages at the 'phys_map' virtual
 * address set up in pmap_bootstrap. It flushes the TLB to make the new
 * mappings effective, and zeros all the bits.
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	vaddr_t srcva;
	int spl;
	int cpu;
	pt_entry_t *srcpte;

	cpu = cpu_number();
	srcva = (vaddr_t)(phys_map_vaddr1 + (cpu * PAGE_SIZE));
	srcpte = pmap_pte(kernel_pmap, srcva);

	SPLVM(spl);
	cmmu_flush_tlb(TRUE, srcva, PAGE_SIZE);
	*srcpte = trunc_page(pa) |
	    m88k_protection(kernel_pmap, VM_PROT_READ | VM_PROT_WRITE) |
	    CACHE_GLOBAL | PG_V;
	SPLX(spl);

	bzero((void *)srcva, PAGE_SIZE);
	/* force the data out */
	cmmu_flush_remote_data_cache(cpu, pa, PAGE_SIZE);
}

/*
 * Routine:	PMAP_CREATE
 *
 * Function:
 *	Create and return a physical map. If the size specified for the
 *	map is zero, the map is an actual physical map, and may be referenced
 *	by the hardware. If the size specified is non-zero, the map will be
 *	used in software only, and is bounded by that size.
 *
 *  This routines allocates a pmap structure.
 */
struct pmap *
pmap_create(void)
{
	pmap_t pmap;
	sdt_entry_t *segdt;
	u_int s;
#ifdef	PMAP_USE_BATC
	int i;
#endif

	pmap = pool_get(&pmappool, PR_WAITOK);
	bzero(pmap, sizeof(*pmap));

	/*
	 * Allocate memory for *actual* segment table and *shadow* table.
	 */
	s = round_page(2 * SDT_SIZE);
#ifdef DEBUG
	if ((pmap_con_dbg & (CD_CREAT | CD_NORM)) == (CD_CREAT | CD_NORM)) {
		printf("(pmap_create: %x) need %d pages for sdt\n",
		       curproc, atop(s));
	}
#endif

	segdt = (sdt_entry_t *)uvm_km_zalloc(kernel_map, s);
	if (segdt == NULL)
		panic("pmap_create: kmem_alloc failure");
	
	/*
	 * Initialize pointer to segment table both virtual and physical.
	 */
	pmap->pm_stab = segdt;
	if (pmap_extract(kernel_pmap, (vaddr_t)segdt,
	    (paddr_t *)&pmap->pm_stpa) == FALSE)
		panic("pmap_create: pmap_extract failed!");

	if (!PAGE_ALIGNED(pmap->pm_stpa))
		panic("pmap_create: sdt_table 0x%x not aligned on page boundary",
		    (int)pmap->pm_stpa);

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_CREAT | CD_NORM)) == (CD_CREAT | CD_NORM)) {
		printf("(pmap_create: %x) pmap=0x%p, pm_stab=0x%x, pm_stpa=0x%x\n",
		       curproc, pmap, pmap->pm_stab, pmap->pm_stpa);
	}
#endif

#ifdef MVME188
	if (brdtyp == BRD_188) {
		/*
		 * memory for page tables should be CACHE DISABLED on MVME188
		 */
		pmap_cache_ctrl(kernel_pmap,
		    (vaddr_t)segdt, (vaddr_t)segdt+ (SDT_SIZE*2),
		    CACHE_INH);
	}
#endif
	/*
	 * Initialize SDT_ENTRIES.
	 */
	/*
	 * There is no need to clear segment table, since kmem_alloc would
	 * provides us clean pages.
	 */

	/*
	 * Initialize pmap structure.
	 */
	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);
	pmap->pm_cpus = 0;

#ifdef	PMAP_USE_BATC
	/* initialize block address translation cache */
	for (i = 0; i < BATC_MAX; i++) {
		pmap->pm_ibatc[i].bits = 0;
		pmap->pm_dbatc[i].bits = 0;
	}
#endif

	return pmap;
}

/*
 * Routine:	PMAP_RELEASE
 *
 *	Internal procedure used by pmap_destroy() to actualy deallocate
 *	the tables.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	pmap_pte
 *	uvm_km_free
 *	PT_FREE
 *
 * Special Assumptions:
 *	No locking is needed, since this is only called which the
 * 	pm_count field of the pmap structure goes to zero.
 *
 * This routine sequences of through the user address space, releasing
 * all translation table space back to the system using PT_FREE.
 * The loops are indexed by the virtual address space
 * ranges represented by the table group sizes(PDT_TABLE_GROUP_VA_SPACE).
 *
 */
void
pmap_release(pmap_t pmap)
{
	unsigned long sdt_va;	/* outer loop index */
	sdt_entry_t *sdttbl;	/* ptr to first entry in the segment table */
	pt_entry_t *gdttbl;	/* ptr to first entry in a page table */
	u_int i, j;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_FREE | CD_NORM)) == (CD_FREE | CD_NORM))
		printf("(pmap_release: %x) pmap %x\n", curproc, pmap);
#endif

	sdttbl = pmap->pm_stab;    /* addr of segment table */
	/*
	  This contortion is here instead of the natural loop
	  because of integer overflow/wraparound if VM_MAX_ADDRESS
	  is near 0xffffffff
	*/
	i = VM_MIN_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
	j = VM_MAX_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
	if (j < 1024)
		j++;

	/* Segment table Loop */
	for (; i < j; i++) {
		sdt_va = PDT_TABLE_GROUP_VA_SPACE*i;
		if ((gdttbl = pmap_pte(pmap, (vaddr_t)sdt_va)) != PT_ENTRY_NULL) {
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_FREE | CD_FULL)) == (CD_FREE | CD_FULL))
				printf("(pmap_release: %x) free page table = 0x%x\n",
				       curproc, gdttbl);
#endif
			PT_FREE(gdttbl);
		}
	} /* Segment Loop */

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_FREE | CD_FULL)) == (CD_FREE | CD_FULL))
		printf("(pmap_release: %x) free segment table = 0x%x\n",
		       curproc, sdttbl);
#endif
	/*
	 * Freeing both *actual* and *shadow* segment tables
	 */
	uvm_km_free(kernel_map, (vaddr_t)sdttbl, round_page(2*SDT_SIZE));

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_FREE | CD_NORM)) == (CD_FREE | CD_NORM))
		printf("(pmap_release: %x) pm_count = 0\n", curproc);
#endif
}

/*
 * Routine:	PMAP_DESTROY
 *
 * Function:
 *	Retire the given physical map from service. Should only be called
 *	if the map contains no valid mappings.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	pmap_release
 *	pool_put
 *
 * Special Assumptions:
 *	Map contains no valid mappings.
 *
 *  This routine decrements the reference count in the pmap
 * structure. If it goes to zero, pmap_release is called to release
 * the memory space to the system. Then, call pool_put to free the
 * pmap structure.
 */
void
pmap_destroy(pmap_t pmap)
{
	int count;

#ifdef DIAGNOSTIC
	if (pmap == kernel_pmap)
		panic("pmap_destroy: Attempt to destroy kernel pmap");
#endif

	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
	if (count == 0) {
		pmap_release(pmap);
		pool_put(&pmappool, pmap);
	}
}


/*
 * Routine:	PMAP_REFERENCE
 *
 * Function:
 *	Add a reference to the specified  pmap.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Under a pmap read lock, the pm_count field of the pmap structure
 * is incremented. The function then returns.
 */
void
pmap_reference(pmap_t pmap)
{

	simple_lock(&pmap->pm_lock);
	pmap->pm_count++;
	simple_unlock(&pmap->pm_lock);
}

/*
 * Routine:	PMAP_REMOVE_RANGE (internal)
 *
 * Function:
 *	Invalidate page table entries associated with the
 *	given virtual address range. The entries given are the first
 *	(inclusive) and last (exclusive) entries for the VM pages.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s		virtual address of start of range to remove
 *	e		virtual address of end of range to remove
 *
 * External/Global:
 *	pv lists
 *
 * Calls:
 *	pmap_pte
 *	pool_put
 *	invalidate_pte
 *	flush_atc_entry
 *
 * Special Assumptions:
 *	The pmap must be locked.
 *
 *   This routine sequences through the pages defined by the given
 * range. For each page, pmap_pte is called to obtain a (virtual)
 * pointer to the page table  entry (PTE) associated with the page's
 * virtual address. If the page table entry does not exist, or is invalid,
 * nothing need be done.
 *
 *  If the PTE is valid, the routine must invalidated the entry. The
 * 'modified' bit, if on, is referenced to the VM, and into the appropriate
 * entry in the PV list entry. Next, the function must find the PV
 * list entry associated with this pmap/va (if it doesn't exist - the function
 * panics). The PV list entry is unlinked from the list, and returned to
 * its zone.
 */
void
pmap_remove_range(pmap_t pmap, vaddr_t s, vaddr_t e)
{
	pt_entry_t *pte, opte;
	pv_entry_t prev, cur, pvl;
	struct vm_page *pg;
	paddr_t pa;
	vaddr_t va;
	u_int users;
	boolean_t kflush;

	/*
	 * pmap has been locked by the caller.
	 */
	users = pmap->pm_cpus;
	if (pmap == kernel_pmap) {
		kflush = TRUE;
	} else {
		kflush = FALSE;
	}

	/*
	 * Loop through the range in vm_page_size increments.
	 * Do not assume that either start or end fail on any
	 * kind of page boundary (though this may be true!?).
	 */

	for (va = s; va < e; va += PAGE_SIZE) {
		sdt_entry_t *sdt;

		sdt = SDTENT(pmap,va);

		if (!SDT_VALID(sdt)) {
			va &= SDT_MASK;	/* align to segment */
			if (va <= e - (1<<SDT_SHIFT))
				va += (1<<SDT_SHIFT) - PAGE_SIZE; /* no page table, skip to next seg entry */
			else /* wrap around */
				break;
			continue;
		}

		pte = pmap_pte(pmap, va);

		if (!PDT_VALID(pte)) {
			continue;	 /* no page mapping */
		}

		/*
		 * Update statistics.
		 */
		pmap->pm_stats.resident_count--;
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;

		pa = ptoa(PG_PFNUM(*pte));
		pg = PHYS_TO_VM_PAGE(pa);

		if (pg != NULL) {
			/*
			 * Remove the mapping from the pvlist for
			 * this physical page.
			 */
			pvl = pg_to_pvh(pg);

#ifdef DIAGNOSTIC
			if (pvl->pv_pmap == PMAP_NULL)
				panic("pmap_remove_range: null pv_list");
#endif

			if (pvl->pv_va == va && pvl->pv_pmap == pmap) {
				/*
				 * Hander is the pv_entry. Copy the next one
				 * to hander and free the next one (we can't
				 * free the hander)
				 */
				cur = pvl->pv_next;
				if (cur != PV_ENTRY_NULL) {
					*pvl = *cur;
					pool_put(&pvpool, cur);
				} else {
					pvl->pv_pmap =  PMAP_NULL;
				}

			} else {

				for (cur = pvl; cur != PV_ENTRY_NULL;
				    cur = cur->pv_next) {
					if (cur->pv_va == va && cur->pv_pmap == pmap)
						break;
					prev = cur;
				}
				if (cur == PV_ENTRY_NULL) {
					printf("pmap_remove_range: looking for VA "
					       "0x%x (pa 0x%x) PV list at 0x%p\n", va, pa, pvl);
					panic("pmap_remove_range: mapping not in pv_list");
				}

				prev->pv_next = cur->pv_next;
				pool_put(&pvpool, cur);
			}
		} /* if (pg != NULL) */

		/*
		 * Reflect modify bits to pager and zero (invalidate,
		 * remove) the pte entry.
		 */

		/*
		 * Invalidate pte temporarily to avoid being written back
		 * the modified bit and/or the reference bit by any other cpu.
		 */
		opte = invalidate_pte(pte);
		flush_atc_entry(users, va, kflush);

		if (opte & PG_M) {
			if (pg != NULL) {
				/* keep track ourselves too */
				pvl->pv_flags |= PG_M;
			}
		}

	} /* for (va = s; ...) */
}

/*
 * Routine:	PMAP_REMOVE
 *
 * Function:
 *	Remove the given range of addresses from the specified map.
 *	It is assumed that start and end are properly rounded to the VM page
 *	size.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s
 *	e
 *
 * Special Assumptions:
 *	Assumes not all entries must be valid in specified range.
 *
 * Calls:
 *	pmap_remove_range
 *
 *  After taking pmap read lock, pmap_remove_range is called to do the
 * real work.
 */
void
pmap_remove(pmap_t pmap, vaddr_t s, vaddr_t e)
{
	int spl;

	if (pmap == PMAP_NULL)
		return;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_RM | CD_NORM)) == (CD_RM | CD_NORM))
		printf("(pmap_remove: %x) map %x s %x e %x\n", curproc, pmap, s, e);
#endif

#ifdef DIAGNOSTIC
	if (s >= e)
		panic("pmap_remove: start greater than end address");
#endif

	PMAP_LOCK(pmap, spl);
	pmap_remove_range(pmap, s, e);
	PMAP_UNLOCK(pmap, spl);
}

/*
 * Routine:	PMAP_REMOVE_ALL
 *
 * Function:
 *	Removes this physical page from all physical maps in which it
 *	resides. Reflects back modify bits to the pager.
 *
 * Parameters:
 *	pg		physical pages which is to
 *			be removed from all maps
 *
 * Extern/Global:
 *	pv lists
 *
 * Calls:
 *	simple_lock
 *	pmap_pte
 *	pool_put
 *
 *  If the page specified by the given address is not a managed page,
 * this routine simply returns. Otherwise, the PV list associated with
 * that page is traversed. For each pmap/va pair pmap_pte is called to
 * obtain a pointer to the page table entry (PTE) associated with the
 * va (the PTE must exist and be valid, otherwise the routine panics).
 * The hardware 'modified' bit in the PTE is examined. If it is on, the
 * corresponding bit in the PV list entry corresponding
 * to the physical page is set to 1.
 * Then, the PTE is invalidated, and the PV list entry is unlinked and
 * freed.
 *
 *  At the end of this function, the PV list for the specified page
 * will be null.
 */
void
pmap_remove_all(struct vm_page *pg)
{
	pt_entry_t *pte;
	pv_entry_t pvl;
	vaddr_t va;
	pmap_t pmap;
	int spl;
#ifdef DEBUG
	int dbgcnt = 0;
#endif

	if (pg == NULL) {
		/* not a managed page. */
#ifdef DEBUG
		if (pmap_con_dbg & CD_RMAL)
			printf("(pmap_remove_all: %x) vm page 0x%x not a managed page\n", curproc, pg);
#endif
		return;
	}

	SPLVM(spl);
	/*
	 * Walk down PV list, removing all mappings.
	 * We don't have to lock the pv list, since we have the entire pmap
	 * system.
	 */
remove_all_Retry:

	pvl = pg_to_pvh(pg);

	/*
	 * Loop for each entry on the pv list
	 */
	while ((pmap = pvl->pv_pmap) != PMAP_NULL) {
		va = pvl->pv_va;
		if (!simple_lock_try(&pmap->pm_lock))
			goto remove_all_Retry;

		pte = pmap_pte(pmap, va);

		/*
		 * Do a few consistency checks to make sure
		 * the PV list and the pmap are in synch.
		 */
#ifdef DIAGNOSTIC
		if (pte == PT_ENTRY_NULL) {
#ifdef DEBUG
			printf("(pmap_remove_all: %p) vm page %p pmap %x va %x dbgcnt %x\n",
			       curproc, pg, pmap, va, dbgcnt);
#endif
			panic("pmap_remove_all: pte NULL");
		}
#endif	/* DIAGNOSTIC */
		if (!PDT_VALID(pte)) {
			pvl = pvl->pv_next;
			goto next;	/* no page mapping */
		}
		if (pmap_pte_w(pte)) {
#ifdef DEBUG
			if (pmap_con_dbg & CD_RMAL)
				printf("pmap_remove_all: wired mapping for %lx not removed\n",
				    pg);
#endif
			pvl = pvl->pv_next;
			goto next;
		}

		pmap_remove_range(pmap, va, va + PAGE_SIZE);

#ifdef DEBUG
		dbgcnt++;
#endif
		/*
		 * Do not free any page tables,
		 * leaves that for when VM calls pmap_collect().
		 */
next:
		simple_unlock(&pmap->pm_lock);
	}
	SPLX(spl);
}

/*
 * Routine:	PMAP_PROTECT
 *
 * Function:
 *	Sets the physical protection on the specified range of this map
 *	as requested.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s		start address of start of range
 *	e		end address of end of range
 *	prot		desired protection attributes
 *
 *	Calls:
 *		PMAP_LOCK, PMAP_UNLOCK
 *		CHECK_PAGE_ALIGN
 *		panic
 *		pmap_pte
 *		SDT_NEXT
 *		PDT_VALID
 *
 *  This routine sequences through the pages of the specified range.
 * For each, it calls pmap_pte to acquire a pointer to the page table
 * entry (PTE). If the PTE is invalid, or non-existant, nothing is done.
 * Otherwise, the PTE's protection attributes are adjusted as specified.
 */
void
pmap_protect(pmap_t pmap, vaddr_t s, vaddr_t e, vm_prot_t prot)
{
	int spl;
	pt_entry_t *pte, ap;
	vaddr_t va;
	u_int users;
	boolean_t kflush;

	if (pmap == PMAP_NULL || prot & VM_PROT_WRITE)
		return;
	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pmap, s, e);
		return;
	}

#ifdef DIAGNOSTIC
	if (s > e)
		panic("pmap_protect: start grater than end address");
#endif

	ap = m88k_protection(pmap, prot) & PG_PROT;

	PMAP_LOCK(pmap, spl);

	users = pmap->pm_cpus;
	if (pmap == kernel_pmap) {
		kflush = TRUE;
	} else {
		kflush = FALSE;
	}

	CHECK_PAGE_ALIGN(s, "pmap_protect");

	/*
	 * Loop through the range in vm_page_size increment.
	 */
	for (va = s; va < e; va += PAGE_SIZE) {
		pte = pmap_pte(pmap, va);
		if (pte == PT_ENTRY_NULL) {
			va &= SDT_MASK;	/* align to segment */
			if (va <= e - (1 << SDT_SHIFT)) {
				/* no page table, skip to next seg entry */
				va += (1 << SDT_SHIFT) - PAGE_SIZE;
			} else {
				/* wrap around */
				break;
			}
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_PROT | CD_FULL)) == (CD_PROT | CD_FULL))
				printf("(pmap_protect: %x) no page table: skip to 0x%x\n", curproc, va + PAGE_SIZE);
#endif
			continue;
		}

		if (!PDT_VALID(pte)) {
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_PROT | CD_FULL)) == (CD_PROT | CD_FULL))
				printf("(pmap_protect: %x) pte invalid pte @ 0x%x\n", curproc, pte);
#endif
			continue;	 /*  no page mapping */
		}

		/*
		 * Invalidate pte temporarily to avoid the
		 * modified bit and/or the reference bit being
		 * written back by any other cpu.
		 */
		*pte = (invalidate_pte(pte) & ~PG_PROT) | ap;
		flush_atc_entry(users, va, kflush);
		pte++;
	}
	PMAP_UNLOCK(pmap, spl);
}

/*
 * Routine:	PMAP_EXPAND
 *
 * Function:
 *	Expands a pmap to be able to map the specified virtual address.
 *	New kernel virtual memory is allocated for a page table
 *
 *	Must be called with the pmap system and the pmap unlocked, since
 *	these must be unlocked to use vm_allocate or vm_deallocate (via
 *	uvm_km_zalloc). Thus it must be called in a unlock/lock loop
 *	that checks whether the map has been expanded enough. ( We won't loop
 *	forever, since page table aren't shrunk.)
 *
 * Parameters:
 *	pmap	point to pmap structure
 *	v	VA indicating which tables are needed
 *
 * Extern/Global:
 *	user_pt_map
 *	kernel_pmap
 *
 * Calls:
 *	pmap_pte
 *	uvm_km_free
 *	uvm_km_zalloc
 *	pmap_extract
 *
 * Special Assumptions
 *	no pmap locks held
 *      pmap != kernel_pmap
 *
 * 1:	This routine immediately allocates space for a page table.
 *
 * 2:	The page table entries (PTEs) are initialized (set invalid), and
 *	the corresponding segment table entry is set to point to the new
 *	page table.
 */
void
pmap_expand(pmap_t pmap, vaddr_t v)
{
	int i, spl;
	vaddr_t pdt_vaddr;
	paddr_t pdt_paddr;
	sdt_entry_t *sdt;
	pt_entry_t *pte;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_EXP | CD_NORM)) == (CD_EXP | CD_NORM))
		printf ("(pmap_expand: %x) map %x v %x\n", curproc, pmap, v);
#endif

	CHECK_PAGE_ALIGN(v, "pmap_expand");

	/* XXX */
	pdt_vaddr = uvm_km_zalloc(kernel_map, PAGE_SIZE);
	if (pmap_extract(kernel_pmap, pdt_vaddr, &pdt_paddr) == FALSE)
		panic("pmap_expand: pmap_extract failed");

#ifdef MVME188
	if (brdtyp == BRD_188) {
		/*
		 * the pages for page tables should be CACHE DISABLED on MVME188
		 */
		pmap_cache_ctrl(kernel_pmap, pdt_vaddr, pdt_vaddr+PAGE_SIZE, CACHE_INH);
	}
#endif

	PMAP_LOCK(pmap, spl);

	if ((pte = pmap_pte(pmap, v)) != PT_ENTRY_NULL) {
		/*
		 * Someone else caused us to expand
		 * during our vm_allocate.
		 */
		PMAP_UNLOCK(pmap, spl);
		/* XXX */
		uvm_km_free(kernel_map, pdt_vaddr, PAGE_SIZE);

#ifdef DEBUG
		if (pmap_con_dbg & CD_EXP)
			printf("(pmap_expand: %x) table has already been allocated\n", curproc);
#endif
		return;
	}
	/*
	 * Apply a mask to V to obtain the vaddr of the beginning of
	 * its containing page 'table group',i.e. the group of
	 * page  tables that fit eithin a single VM page.
	 * Using that, obtain the segment table pointer that references the
	 * first page table in the group, and initialize all the
	 * segment table descriptions for the page 'table group'.
	 */
	v &= ~((1 << (LOG2_PDT_TABLE_GROUP_SIZE + PDT_BITS + PG_BITS)) - 1);

	sdt = SDTENT(pmap,v);

	/*
	 * Init each of the segment entries to point the freshly allocated
	 * page tables.
	 */
	for (i = PDT_TABLE_GROUP_SIZE; i>0; i--) {
		*((sdt_entry_t *)sdt) = pdt_paddr | SG_RW | SG_V;
		*((sdt_entry_t *)(sdt + SDT_ENTRIES)) = pdt_vaddr | SG_RW | SG_V;
		sdt++;
		pdt_paddr += PDT_SIZE;
		pdt_vaddr += PDT_SIZE;
	}
	PMAP_UNLOCK(pmap, spl);
}

/*
 * Routine:	PMAP_ENTER
 *
 * Function:
 *	Insert the given physical page (p) at the specified virtual
 *	address (v) in the target phisical map with the protecton requested.
 *	If specified, the page will be wired down, meaning that the
 *	related pte can not be reclaimed.
 *
 * N.B.: This is the only routine which MAY NOT lazy-evaluation or lose
 *	information. That is, this routine must actually insert this page
 *	into the given map NOW.
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	va	VA of page to be mapped
 *	pa	PA of page to be mapped
 *	prot	protection attributes for page
 *	wired	wired attribute for page
 *
 * Extern/Global:
 *	pv lists
 *
 * Calls:
 *	m88k_protection
 *	pmap_pte
 *	pmap_expand
 *	pmap_remove_range
 *	PT_FREE
 *
 *	This routine starts off by calling pmap_pte to obtain a (virtual)
 * pointer to the page table entry corresponding to given virtual
 * address. If the page table itself does not exist, pmap_expand is
 * called to allocate it.
 *
 *      If the page table entry (PTE) already maps the given physical page,
 * all that is needed is to set the protection and wired attributes as
 * given. TLB entries are flushed and pmap_enter returns.
 *
 *	If the page table entry (PTE) maps a different physical page than
 * that given, the old mapping is removed by a call to map_remove_range.
 * And execution of pmap_enter continues.
 *
 *	To map the new physical page, the routine first inserts a new
 * entry in the PV list exhibiting the given pmap and virtual address.
 * It then inserts the physical page address, protection attributes, and
 * wired attributes into the page table entry (PTE).
 *
 *
 *	get machine-dependent prot code
 *	get the pte for this page
 *	if necessary pmap expand(pmap,v)
 *	if (changing wired attribute or protection) {
 * 		flush entry from TLB
 *		update template
 *		for (ptes per vm page)
 *			stuff pte
 *	} else if (mapped at wrong addr)
 *		flush entry from TLB
 *		pmap_remove_range
 *	} else {
 *		enter mapping in pv_list
 *		setup template and stuff ptes
 *	}
 *
 */
int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	int spl;
	pt_entry_t *pte, ap, template;
	paddr_t old_pa;
	pv_entry_t pv_e, pvl;
	u_int users;
	boolean_t kflush;
	boolean_t wired = (flags & PMAP_WIRED) != 0;
	struct vm_page *pg;

	CHECK_PAGE_ALIGN(va, "pmap_entry - VA");
	CHECK_PAGE_ALIGN(pa, "pmap_entry - PA");

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_ENT | CD_NORM)) == (CD_ENT | CD_NORM)) {
		if (pmap == kernel_pmap)
			printf("(pmap_enter: %x) pmap kernel va %x pa %x\n", curproc, va, pa);
		else
			printf("(pmap_enter: %x) pmap %x va %x pa %x\n", curproc, pmap, va, pa);
	}
#endif

	ap = m88k_protection(pmap, prot);

	/*
	 * Must allocate a new pvlist entry while we're unlocked;
	 * zalloc may cause pageout (which will lock the pmap system).
	 * If we determine we need a pvlist entry, we will unlock
	 * and allocate one. Then will retry, throwing away
	 * the allocated entry later (if we no longer need it).
	 */
	pv_e = PV_ENTRY_NULL;

	PMAP_LOCK(pmap, spl);
	users = pmap->pm_cpus;

Retry:
	/*
	 * Expand pmap to include this pte.
	 */
	while ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL) {
		if (pmap == kernel_pmap) {
			pmap_expand_kmap(va, VM_PROT_READ|VM_PROT_WRITE);
		} else {
			/*
			 * Must unlock to expand the pmap.
			 */
			PMAP_UNLOCK(pmap, spl);
			pmap_expand(pmap, va);
			PMAP_LOCK(pmap, spl);
		}
	}
	/*
	 *	Special case if the physical page is already mapped
	 *	at this address.
	 */
	old_pa = ptoa(PG_PFNUM(*pte));
	if (old_pa == pa) {
		if (pmap == kernel_pmap) {
			kflush = TRUE;
		} else {
			kflush = FALSE;
		}

		/*
		 * May be changing its wired attributes or protection
		 */

		if (wired && !(pmap_pte_w(pte)))
			pmap->pm_stats.wired_count++;
		else if (!wired && pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;

		if ((unsigned long)pa >= MAXPHYSMEM)
			template = CACHE_INH | PG_V;
		else
			template = CACHE_GLOBAL | PG_V;
		if (wired)
			template |= PG_W;

		/*
		 * If there is a same mapping, we have nothing to do.
		 */
		if (!PDT_VALID(pte) || pmap_pte_w_chg(pte, template & PG_W) ||
		    (pmap_pte_prot_chg(pte, ap & PG_PROT))) {

			/*
			 * Invalidate pte temporarily to avoid being written
			 * back the modified bit and/or the reference bit by
			 * any other cpu.
			 */
			template |= (invalidate_pte(pte) & PG_M);
			*pte++ = template | ap | trunc_page(pa);
			flush_atc_entry(users, va, kflush);
		}

	} else { /* if ( pa == old_pa) */
		/*
		 * Remove old mapping from the PV list if necessary.
		 */

		pg = PHYS_TO_VM_PAGE(pa);
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_ENT | CD_NORM)) == (CD_ENT | CD_NORM)) {
			if (va == phys_map_vaddr1 || va == phys_map_vaddr2) {
				printf("vaddr1 0x%x vaddr2 0x%x va 0x%x pa 0x%x managed %x\n",
				       phys_map_vaddr1, phys_map_vaddr2, va, old_pa,
				       pg != NULL ? 1 : 0);
				printf("pte %x pfn %x valid %x\n",
				       pte, PG_PFNUM(*pte), PDT_VALID(pte));
			}
		}
#endif
		if (va == phys_map_vaddr1 || va == phys_map_vaddr2) {
			flush_atc_entry(users, va, TRUE);
		} else {
			pmap_remove_range(pmap, va, va + PAGE_SIZE);
		}

		if (pg != NULL) {
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_ENT | CD_NORM)) == (CD_ENT | CD_NORM)) {
				if (va == phys_map_vaddr1 || va == phys_map_vaddr2) {
					printf("va 0x%x and managed pa 0x%x\n", va, pa);
				}
			}
#endif
			/*
			 *	Enter the mappimg in the PV list for this
			 *	physical page.
			 */
			pvl = pg_to_pvh(pg);

			if (pvl->pv_pmap == PMAP_NULL) {
				/*
				 *	No mappings yet
				 */
				pvl->pv_va = va;
				pvl->pv_pmap = pmap;
				pvl->pv_next = PV_ENTRY_NULL;

			} else {
#ifdef DEBUG
				/*
				 * check that this mapping is not already there
				 */
				{
					pv_entry_t e = pvl;
					while (e != PV_ENTRY_NULL) {
						if (e->pv_pmap == pmap && e->pv_va == va)
							panic("pmap_enter: already in pv_list");
						e = e->pv_next;
					}
				}
#endif
				/*
				 * Add new pv_entry after header.
				 */
				if (pv_e == PV_ENTRY_NULL) {
					pv_e = pool_get(&pvpool, PR_NOWAIT);
					goto Retry;
				}
				pv_e->pv_va = va;
				pv_e->pv_pmap = pmap;
				pv_e->pv_next = pvl->pv_next;
				pvl->pv_next = pv_e;
				/*
				 * Remember that we used the pvlist entry.
				 */
				pv_e = PV_ENTRY_NULL;
			}
		}

		/*
		 * And count the mapping.
		 */
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;

		if ((unsigned long)pa >= MAXPHYSMEM)
			template = CACHE_INH | PG_V;
		else
			template = CACHE_GLOBAL | PG_V;
		if (wired)
			template |= PG_W;

		if (flags & VM_PROT_WRITE)
			template |= PG_U | PG_M;
		else if (flags & VM_PROT_ALL)
			template |= PG_U;

		*pte = template | ap | trunc_page(pa);

	} /* if (pa == old_pa) ... else */

	PMAP_UNLOCK(pmap, spl);

	if (pv_e != PV_ENTRY_NULL)
		pool_put(&pvpool, pv_e);

	return 0;
}

/*
 * Routine:	pmap_unwire
 *
 * Function:	Change the wiring attributes for a map/virtual-address pair.
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	v	virtual address of page to be unwired
 *
 * Calls:
 *	pmap_pte
 *
 * Special Assumptions:
 *	The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap_t pmap, vaddr_t v)
{
	pt_entry_t *pte;
	int spl;

	PMAP_LOCK(pmap, spl);

	if ((pte = pmap_pte(pmap, v)) == PT_ENTRY_NULL)
		panic("pmap_unwire: pte missing");

	if (pmap_pte_w(pte)) {
		/* unwired mapping */
		pmap->pm_stats.wired_count--;
		*pte &= ~PG_W;
	}

	PMAP_UNLOCK(pmap, spl);
}

/*
 * Routine:	PMAP_EXTRACT
 *
 * Function:
 *	Extract the physical page address associoated
 *	with the given map/virtual_address pair.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	va		virtual address
 *	pap		storage for result.
 *
 * Calls:
 *	PMAP_LOCK, PMAP_UNLOCK
 *	pmap_pte
 *
 * If BATC mapping is enabled and the specified pmap is kernel_pmap,
 * batc_entry is scanned to find out the mapping.
 *
 * Then the routine calls pmap_pte to get a (virtual) pointer to
 * the page table entry (PTE) associated with the given virtual
 * address. If the page table does not exist, or if the PTE is not valid,
 * then 0 address is returned. Otherwise, the physical page address from
 * the PTE is returned.
 */
boolean_t
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t *pte;
	paddr_t pa;
	int spl;
	boolean_t rv = FALSE;

#ifdef	PMAP_USE_BATC
	int i;
#endif

#ifdef DIAGNOSTIC
	if (pmap == PMAP_NULL)
		panic("pmap_extract: pmap is NULL");
#endif

#ifdef	PMAP_USE_BATC
	/*
	 * check BATC first
	 */
	if (pmap == kernel_pmap && batc_used != 0)
		for (i = batc_used - 1; i != 0; i--)
			if (batc_entry[i].lba == M88K_BTOBLK(va)) {
				if (pap != NULL)
					*pap = (batc_entry[i].pba << BATC_BLKSHIFT) |
						(va & BATC_BLKMASK);
				return TRUE;
			}
#endif

	PMAP_LOCK(pmap, spl);

	if ((pte = pmap_pte(pmap, va)) != PT_ENTRY_NULL) {
		if (PDT_VALID(pte)) {
			rv = TRUE;
			if (pap != NULL) {
				pa = ptoa(PG_PFNUM(*pte));
				pa |= (va & PAGE_MASK); /* offset within page */
				*pap = pa;
			}
		}
	}

	PMAP_UNLOCK(pmap, spl);
	return rv;
}

/*
 * Routine:	PMAP_COLLECT
 *
 * Runction:
 *	Garbage collects the physical map system for pages which are
 *	no longer used. there may well be pages which are not
 *	referenced, but others may be collected as well.
 *	Called by the pageout daemon when pages are scarce.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	PT_FREE
 *	pmap_pte
 *	pmap_remove_range
 *
 *	The intent of this routine is to release memory pages being used
 * by translation tables. They can be release only if they contain no
 * valid mappings, and their parent table entry has been invalidated.
 *
 *	The routine sequences through the entries user address space,
 * inspecting page-sized groups of page tables for wired entries. If
 * a full page of tables has no wired enties, any otherwise valid
 * entries are invalidated (via pmap_remove_range). Then, the segment
 * table entries corresponding to this group of page tables are
 * invalidated. Finally, PT_FREE is called to return the page to the
 * system.
 *
 *	If all entries in a segment table are invalidated, it too can
 * be returned to the system.
 *
 *	[Note: depending upon compilation options, tables may be in zones
 * or allocated through kmem_alloc. In the former case, the
 * module deals with a single table at a time.]
 */
void
pmap_collect( pmap_t pmap)
{
	vaddr_t sdt_va;		/* outer loop index */
	vaddr_t sdt_vt;		/* end of segment */
	sdt_entry_t *sdttbl;	/* ptr to first entry in seg table */
	sdt_entry_t *sdtp;	/* ptr to index into segment table */
	sdt_entry_t *sdt;	/* ptr to index into segment table */
	pt_entry_t *gdttbl;	/* ptr to first entry in a page table */
	pt_entry_t *gdttblend;	/* ptr to byte after last entry in
				   table group */
	pt_entry_t *gdtp;	/* ptr to index into a page table */
	boolean_t found_gdt_wired; /* flag indicating a wired page exists
				   in a page table's address range */
	int spl;
	u_int i, j;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_COL | CD_NORM)) == (CD_COL | CD_NORM))
		printf ("(pmap_collect: %x) pmap %x\n", curproc, pmap);
#endif

	PMAP_LOCK(pmap, spl);

	sdttbl = pmap->pm_stab; /* addr of segment table */
	sdtp = sdttbl;

	/*
	  This contortion is here instead of the natural loop
	  because of integer overflow/wraparound if VM_MAX_ADDRESS
	  is near 0xffffffff
	*/
	i = VM_MIN_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
	j = VM_MAX_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
	if (j < 1024)
		j++;

	/* Segment table loop */
	for (; i < j; i++, sdtp += PDT_TABLE_GROUP_SIZE) {
		sdt_va = VM_MIN_ADDRESS + PDT_TABLE_GROUP_VA_SPACE * i;

		gdttbl = pmap_pte(pmap, sdt_va);

		if (gdttbl == PT_ENTRY_NULL)
			continue; /* no maps in this range */

		gdttblend = gdttbl + (PDT_ENTRIES * PDT_TABLE_GROUP_SIZE);

		/* scan page maps for wired pages */
		found_gdt_wired = FALSE;
		for (gdtp = gdttbl; gdtp < gdttblend; gdtp++) {
			if (pmap_pte_w(gdtp)) {
				found_gdt_wired = TRUE;
				break;
			}
		}

		if (found_gdt_wired)
			continue; /* can't free this range */

		/* figure out end of range. Watch for wraparound */
		sdt_vt = sdt_va <= VM_MAX_ADDRESS - PDT_TABLE_GROUP_VA_SPACE ?
		    sdt_va + PDT_TABLE_GROUP_VA_SPACE : VM_MAX_ADDRESS;

		/* invalidate all maps in this range */
		pmap_remove_range(pmap, sdt_va, sdt_vt);

		/*
		 * we can safely deallocate the page map(s)
		 */
		for (sdt = sdtp; sdt < (sdtp + PDT_TABLE_GROUP_SIZE); sdt++) {
			*((sdt_entry_t *) sdt) = 0;
			*((sdt_entry_t *)(sdt + SDT_ENTRIES)) = 0;
		}

		/*
		 * we have to unlock before freeing the table, since PT_FREE
		 * calls uvm_km_free or free, which will invoke another
		 * pmap routine
		 */
		PMAP_UNLOCK(pmap, spl);
		PT_FREE(gdttbl);
		PMAP_LOCK(pmap, spl);
	}

	PMAP_UNLOCK(pmap, spl);

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_COL | CD_NORM)) == (CD_COL | CD_NORM))
		printf("(pmap_collect: %x) done\n", curproc);
#endif
}

/*
 * Routine:	PMAP_ACTIVATE
 *
 * Function:
 * 	Binds the given physical map to the given
 *	processor, and returns a hardware map description.
 *	In a mono-processor implementation the cpu
 *	argument is ignored, and the PMAP_ACTIVATE macro
 *	simply sets the MMU root pointer element of the PCB
 *	to the physical address of the segment descriptor table.
 *
 * Parameters:
 * 	p	pointer to proc structure
 *
 * Notes:
 *	If the specified pmap is not kernel_pmap, this routine makes arp
 *	template and stores it into UAPR (user area pointer register) in the
 *	CMMUs connected to the specified CPU.
 *
 *	If kernel_pmap is specified, only flushes the TLBs mapping kernel
 *	virtual space, in the CMMUs connected to the specified CPU.
 *
 */
void
pmap_activate(struct proc *p)
{
	apr_template_t apr_data;
	pmap_t pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	int cpu = cpu_number();

#ifdef	PMAP_USE_BATC
	int n;
#endif

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_ACTIVATE | CD_NORM)) == (CD_ACTIVATE | CD_NORM))
		printf("(pmap_activate: %x) pmap 0x%p\n", p, pmap);
#endif

	if (pmap != kernel_pmap) {
		/*
		 * Lock the pmap to put this cpu in its active set.
		 */

		simple_lock(&pmap->pm_lock);
		apr_data.bits = 0;
		apr_data.field.st_base = atop(pmap->pm_stpa);
		apr_data.field.wt = 0;
		apr_data.field.g  = 1;
		apr_data.field.ci = 0;
		apr_data.field.te = 1;

#ifdef	PMAP_USE_BATC
		/*
		 * cmmu_pmap_activate will set the uapr and the batc entries,
		 * then flush the *USER* TLB.  IF THE KERNEL WILL EVER CARE
		 * ABOUT THE BATC ENTRIES, THE SUPERVISOR TLBs SHOULB BE
		 * FLUSHED AS WELL.
		 */
		cmmu_pmap_activate(cpu, apr_data.bits,
				   pmap->pm_ibatc, pmap->pm_dbatc);
		for (n = 0; n < BATC_MAX; n++)
			*(register_t *)&batc_entry[n] = pmap->pm_ibatc[n].bits;
#else
		cmmu_set_uapr(apr_data.bits);
		cmmu_flush_tlb(FALSE, 0, -1);
#endif	/* PMAP_USE_BATC */

		/*
		 * Mark that this cpu is using the pmap.
		 */
		SETBIT_CPUSET(cpu, &(pmap->pm_cpus));

		simple_unlock(&pmap->pm_lock);
	}
}

/*
 * Routine:	PMAP_DEACTIVATE
 *
 * Function:
 *	Unbinds the given physical map from the given processor,
 *	i.e. the pmap i no longer is use on the processor.
 *
 * Parameters:
 *     	p		pointer to proc structure
 *
 * pmap_deactive simply clears the pm_cpus field in given pmap structure.
 *
 */
void
pmap_deactivate(struct proc *p)
{
	pmap_t pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	int cpu = cpu_number();
	
	if (pmap != kernel_pmap) {
		/*
		 * we expect the spl is already raised to sched level.
		 */
		simple_lock(&pmap->pm_lock);
		CLRBIT_CPUSET(cpu, &(pmap->pm_cpus));
		simple_unlock(&pmap->pm_lock);
	}
}

/*
 * Routine:	PMAP_COPY_PAGE
 *
 * Function:
 *	Copies the specified pages.
 *
 * Parameters:
 *	src	PA of source page
 *	dst	PA of destination page
 *
 * Extern/Global:
 *	phys_map_vaddr1
 *	phys_map_vaddr2
 *
 * Calls:
 *	m88k_protection
 *
 * Special Assumptions:
 *	no locking reauired
 *
 * This routine maps the physical pages at the 'phys_map' virtual
 * addresses set up in pmap_bootstrap. It flushes the TLB to make the
 * new mappings effective, and performs the copy.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);
	vaddr_t dstva, srcva;
	int spl;
	pt_entry_t template, *dstpte, *srcpte;
	int cpu = cpu_number();

	template = m88k_protection(kernel_pmap, VM_PROT_READ | VM_PROT_WRITE) |
	    CACHE_GLOBAL | PG_V;

	/*
	 * Map source physical address.
	 */
	srcva = (vaddr_t)(phys_map_vaddr1 + (cpu << PAGE_SHIFT));
	dstva = (vaddr_t)(phys_map_vaddr2 + (cpu << PAGE_SHIFT));

	srcpte = pmap_pte(kernel_pmap, srcva);
	dstpte = pmap_pte(kernel_pmap, dstva);

	SPLVM(spl);
	cmmu_flush_tlb(TRUE, srcva, PAGE_SIZE);
	*srcpte = template | trunc_page(src);

	/*
	 * Map destination physical address.
	 */
	cmmu_flush_tlb(TRUE, dstva, PAGE_SIZE);
	*dstpte  = template | trunc_page(dst);
	SPLX(spl);

	bcopy((void *)srcva, (void *)dstva, PAGE_SIZE);
	/* flush source, dest out of cache? */
	cmmu_flush_remote_data_cache(cpu, src, PAGE_SIZE);
	cmmu_flush_remote_data_cache(cpu, dst, PAGE_SIZE);
}

/*
 * Routine:	PMAP_CHANGEBIT
 *
 * Function:
 *	Update the pte bits on the specified physical page.
 *
 * Parameters:
 *	pg	physical page
 *	set	bits to set
 *	mask	bits to mask
 *
 * Extern/Global:
 *	pv_lists
 *
 * Calls:
 *	pmap_pte
 *
 * The pte bits corresponding to the page's frame index will be changed as
 * requested. The PV list will be traversed.
 * For each pmap/va the hardware the necessary bits in the page descriptor
 * table entry will be altered as well if necessary. If any bits were changed,
 * a TLB flush will be performed.
 */
void
pmap_changebit(struct vm_page *pg, int set, int mask)
{
	pv_entry_t pvl, pvep;
	pt_entry_t *pte, npte;
	pmap_t pmap;
	int spl;
	vaddr_t va;
	u_int users;
	boolean_t kflush;

	SPLVM(spl);

changebit_Retry:
	pvl = pg_to_pvh(pg);

	/*
	 * Clear saved attributes (modify, reference)
	 */
	pvl->pv_flags &= mask;

	if (pvl->pv_pmap == PMAP_NULL) {
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_CBIT | CD_NORM)) == (CD_CBIT | CD_NORM))
			printf("(pmap_changebit: %x) vm page 0x%x not mapped\n",
			    curproc, pg);
#endif
		SPLX(spl);
		return;
	}

	/* for each listed pmap, update the affected bits */
	for (pvep = pvl; pvep != PV_ENTRY_NULL; pvep = pvep->pv_next) {
		pmap = pvep->pv_pmap;
		va = pvep->pv_va;
		if (!simple_lock_try(&pmap->pm_lock)) {
			goto changebit_Retry;
		}
		users = pmap->pm_cpus;
		if (pmap == kernel_pmap) {
			kflush = TRUE;
		} else {
			kflush = FALSE;
		}

		pte = pmap_pte(pmap, va);

#ifdef DIAGNOSTIC
		/*
		 * Check for existing and valid pte
		 */
		if (pte == PT_ENTRY_NULL)
			panic("pmap_changebit: bad pv list entry.");
		if (!PDT_VALID(pte))
			panic("pmap_changebit: invalid pte");
		if (ptoa(PG_PFNUM(*pte)) != VM_PAGE_TO_PHYS(pg))
			panic("pmap_changebit: pte doesn't point to page");
#endif

		/*
		 * Update bits
		 */
		*pte = invalidate_pte(pte);
		npte = (*pte | set) & mask;

		/*
		 * Flush TLB of which cpus using pmap.
		 *
		 * Invalidate pte temporarily to avoid the modified bit
		 * and/or the reference being written back by any other cpu.
		 */
		if (npte != *pte) {
			*pte = npte;
			flush_atc_entry(users, va, kflush);
		}

		simple_unlock(&pmap->pm_lock);
	}
	SPLX(spl);
}

/*
 * Routine:	PMAP_CLEAR_MODIFY
 *
 * Function:
 *	Clears the modify bits on the specified physical page.
 */
boolean_t
pmap_clear_modify(struct vm_page *pg)
{
	boolean_t rv;

	rv = pmap_testbit(pg, PG_M);
	pmap_changebit(pg, 0, ~PG_M);
	return rv;
}

/*
 * Routine:	PMAP_TESTBIT
 *
 * Function:
 *	Test the modified/referenced bits of a physical page.
 *
 * Parameters:
 *	pg	physical page
 *	bit	bit to test
 *
 * Extern/Global:
 *	pv lists
 *
 * Calls:
 *	simple_lock, simple_unlock
 *	pmap_pte
 *
 * If the attribute list for the given page has the bit, this routine
 * returns TRUE.
 *
 * Otherwise, this routine walks the PV list corresponding to the
 * given page. For each pmap/va pair, the page descripter table entry is
 * examined. If the selected bit is found on, the function returns TRUE
 * immediately (doesn't need to walk remainder of list), and updates the
 * attribute list.
 */
boolean_t
pmap_testbit(struct vm_page *pg, int bit)
{
	pv_entry_t pvl, pvep;
	pt_entry_t *pte;
	int spl;
	boolean_t rv;

	SPLVM(spl);

	pvl = pg_to_pvh(pg);
testbit_Retry:

	if (pvl->pv_flags & bit) {
		/* we've already cached a this flag for this page,
		   no use looking further... */
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_TBIT | CD_NORM)) == (CD_TBIT | CD_NORM))
			printf("(pmap_testbit: %x) already cached a modify flag for this page\n",
			    curproc);
#endif
		SPLX(spl);
		return (TRUE);
	}

	if (pvl->pv_pmap == PMAP_NULL) {
		/* unmapped page - get info from attribute array
		   maintained by pmap_remove_range/pmap_remove_all */
		rv = (boolean_t)(pvl->pv_flags & bit);
#ifdef DEBUG
		if ((pmap_con_dbg & (CD_TBIT | CD_NORM)) == (CD_TBIT | CD_NORM))
			printf("(pmap_testbit: %x) vm page 0x%x not mapped\n",
			    curproc, pg);
#endif
		SPLX(spl);
		return (rv);
	}

	/* for each listed pmap, check modified bit for given page */
	pvep = pvl;
	while (pvep != PV_ENTRY_NULL) {
		if (!simple_lock_try(&pvep->pv_pmap->pm_lock)) {
			goto testbit_Retry;
		}

		pte = pmap_pte(pvep->pv_pmap, pvep->pv_va);
		if (pte == PT_ENTRY_NULL) {
			printf("pmap_testbit: pte from pv_list not in map virt = 0x%x\n", pvep->pv_va);
			panic("pmap_testbit: bad pv list entry");
		}
		if (*pte & bit) {
			simple_unlock(&pvep->pv_pmap->pm_lock);
			pvl->pv_flags |= bit;
#ifdef DEBUG
			if ((pmap_con_dbg & (CD_TBIT | CD_FULL)) == (CD_TBIT | CD_FULL))
				printf("(pmap_testbit: %x) modified page pte@0x%p\n", curproc, pte);
#endif
			SPLX(spl);
			return (TRUE);
		}
		simple_unlock(&pvep->pv_pmap->pm_lock);
		pvep = pvep->pv_next;
	}

	SPLX(spl);
	return (FALSE);
}

/*
 * Routine:	PMAP_IS_MODIFIED
 *
 * Function:
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(struct vm_page *pg)
{
	return pmap_testbit(pg, PG_M);
}

/*
 * Routine:	PMAP_CLEAR_REFERENCE
 *
 * Function:
 *	Clear the reference bit on the specified physical page.
 */
boolean_t
pmap_clear_reference(struct vm_page *pg)
{
	boolean_t rv;

	rv = pmap_testbit(pg, PG_U);
	pmap_changebit(pg, 0, ~PG_U);
	return rv;
}

/*
 * Routine:	PMAP_IS_REFERENCED
 *
 * Function:
 *	Return whether or not the specified physical page is referenced by
 *	any physical maps.
 */
boolean_t
pmap_is_referenced(struct vm_page *pg)
{
	return pmap_testbit(pg, PG_U);
}

/*
 * Routine:	PMAP_PAGE_PROTECT
 *
 * Calls:
 *	pmap_changebit
 *	pmap_remove_all
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	switch (prot) {
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		/* copy on write */
		pmap_changebit(pg, PG_RO, ~0);
		break;
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		break;
	default:
		pmap_remove_all(pg);
		break;
	}
}

void
pmap_virtual_space(vaddr_t *startp, vaddr_t *endp)
{
	*startp = virtual_avail;
	*endp = virtual_end;
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	int spl;
	pt_entry_t template, *pte;
	u_int users;

	CHECK_PAGE_ALIGN(va, "pmap_kenter_pa - VA");
	CHECK_PAGE_ALIGN(pa, "pmap_kenter_pa - PA");

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_ENT | CD_NORM)) == (CD_ENT | CD_NORM)) {
		printf ("(pmap_kenter_pa: %x) va %x pa %x\n", curproc, va, pa);
	}
#endif

	PMAP_LOCK(kernel_pmap, spl);
	users = kernel_pmap->pm_cpus;

	template = m88k_protection(kernel_pmap, prot);

	/*
	 * Expand pmap to include this pte.
	 */
	while ((pte = pmap_pte(kernel_pmap, va)) == PT_ENTRY_NULL) {
		pmap_expand_kmap(va, VM_PROT_READ|VM_PROT_WRITE);
	}

	/*
	 * And count the mapping.
	 */
	kernel_pmap->pm_stats.resident_count++;
	kernel_pmap->pm_stats.wired_count++;

	invalidate_pte(pte);
	if ((unsigned long)pa >= MAXPHYSMEM)
		template |= CACHE_INH | PG_V | PG_W;
	else
		template |= CACHE_GLOBAL | PG_V | PG_W;
	*pte = template | trunc_page(pa);
	flush_atc_entry(users, va, TRUE);

	PMAP_UNLOCK(kernel_pmap, spl);
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	int spl;
	u_int users;

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_RM | CD_NORM)) == (CD_RM | CD_NORM))
		printf("(pmap_kremove: %x) va %x len %x\n", curproc, va, len);
#endif

	CHECK_PAGE_ALIGN(va, "pmap_kremove addr");
	CHECK_PAGE_ALIGN(len, "pmap_kremove len");

	PMAP_LOCK(kernel_pmap, spl);
	users = kernel_pmap->pm_cpus;

	for (len >>= PAGE_SHIFT; len != 0; len--, va += PAGE_SIZE) {
		vaddr_t e = va + PAGE_SIZE;
		sdt_entry_t *sdt;
		pt_entry_t *pte;

		sdt = SDTENT(kernel_pmap, va);

		if (!SDT_VALID(sdt)) {
			va &= SDT_MASK;	/* align to segment */
			if (va <= e - (1<<SDT_SHIFT))
				va += (1<<SDT_SHIFT) - PAGE_SIZE; /* no page table, skip to next seg entry */
			else /* wrap around */
				break;
			continue;
		}

		/*
		 * Update the counts
		 */
		kernel_pmap->pm_stats.resident_count--;
		kernel_pmap->pm_stats.wired_count--;

		pte = pmap_pte(kernel_pmap, va);
		invalidate_pte(pte);
		flush_atc_entry(users, va, TRUE);
	}
	PMAP_UNLOCK(map, spl);
}
