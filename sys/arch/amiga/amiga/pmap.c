/*	$OpenBSD: pmap.c,v 1.12 1999/01/11 05:11:05 millert Exp $	*/
/*	$NetBSD: pmap.c,v 1.39 1997/06/10 18:26:41 veego Exp $	*/

/* 
 * Copyright (c) 1991 Regents of the University of California.
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
 *	@(#)pmap.c	7.5 (Berkeley) 5/10/91
 */

/*
 *	AMIGA physical map management code.
 *	For 68020/68030 machines with 68551, or 68030 MMUs
 *	Don't even pay lip service to multiprocessor support.
 *
 *	will only work for PAGE_SIZE == NBPG
 *	right now because of the assumed one-to-one relationship of PT
 *	pages to STEs.
 */

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <vm/vm.h>
#include <sys/user.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/vmparam.h>
#include <amiga/amiga/memlist.h>
/*
 * Allocate various and sundry SYSMAPs used in the days of old VM
 * and not yet converted.  XXX.
 */

#ifdef DEBUG
struct kpt_stats {
	int collectscans;
	int collectpages;
	int kpttotal;
	int kptinuse;
	int kptmaxuse;
};
struct enter_stats {
	int kernel;	/* entering kernel mapping */
	int user;	/* entering user mapping */
	int ptpneeded;	/* needed to allocate a PT page */
	int pwchange;	/* no mapping change, just wiring or protection */
	int wchange;	/* no mapping change, just wiring */
	int mchange;	/* was mapped but mapping to different page */
	int managed;	/* a managed page */
	int firstpv;	/* first mapping for this PA */
	int secondpv;	/* second mapping for this PA */
	int ci;		/* cache inhibited */
	int unmanaged;	/* not a managed page */
	int flushes;	/* cache flushes */
};
struct remove_stats {
	int calls;
	int removes;
	int pvfirst;
	int pvsearch;
	int ptinvalid;
	int uflushes;
	int sflushes;
};

struct remove_stats remove_stats;
struct enter_stats enter_stats;
struct kpt_stats kpt_stats;

#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_CACHE	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_SEGTAB	0x0400
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000
int debugmap = 0;
int pmapdebug = PDB_PARANOIA;
#endif

/*
 * Get STEs and PTEs for user/kernel address space
 */
#if defined(M68040) || defined(M68060)
#define	pmap_ste(m, v)	(&((m)->pm_stab[(vm_offset_t)(v) >> pmap_ishift]))
#define pmap_ste1(m, v) \
	(&((m)->pm_stab[(vm_offset_t)(v) >> SG4_SHIFT1]))
/* XXX assumes physically contiguous ST pages (if more than one) */
#define pmap_ste2(m, v) \
	(&((m)->pm_stab[(u_int *)(*(u_int *)pmap_ste1(m,v) & SG4_ADDR1) \
			- (m)->pm_stpa + (((v) & SG4_MASK2) >> SG4_SHIFT2)]))
#define pmap_ste_v(m, v) \
	(mmutype == MMU_68040 \
	? ((*pmap_ste1(m, v) & SG_V) && \
	   (*pmap_ste2(m, v) & SG_V)) \
	: (*pmap_ste(m, v) & SG_V))
#else
#define	pmap_ste(m, v)	(&((m)->pm_stab[(vm_offset_t)(v) >> SG_ISHIFT]))
#define pmap_ste_v(m, v)	(*pmap_ste(m, v) & SG_V)
#endif
#define pmap_pte(m, v)	(&((m)->pm_ptab[(vm_offset_t)(v) >> PG_SHIFT]))

#define pmap_pte_pa(pte)	(*(u_int *)(pte) & PG_FRAME)

#define pmap_pte_w(pte)		(*(u_int *)(pte) & PG_W)
#define pmap_pte_ci(pte)	(*(u_int *)(pte) & PG_CI)
#define pmap_pte_m(pte)		(*(u_int *)(pte) & PG_M)
#define pmap_pte_u(pte)		(*(u_int *)(pte) & PG_U)
#define pmap_pte_prot(pte)	(*(u_int *)(pte) & PG_PROT)
#define pmap_pte_v(pte)		(*(u_int *)(pte) & PG_V)

#define pmap_pte_set_w(pte, v) \
    do { if (v) *(u_int *)(pte) |= PG_W; else *(u_int *)(pte) &= ~PG_W; \
    } while (0)
#define pmap_pte_set_prot(pte, v) \
    do { if (v) *(u_int *)(pte) |= PG_PROT; else *(u_int *)(pte) &= ~PG_PROT; \
    } while (0)
#define pmap_pte_w_chg(pte, nw)		((nw) ^ pmap_pte_w(pte))
#define pmap_pte_prot_chg(pte, np)	((np) ^ pmap_pte_prot(pte))

#define active_pmap(pm)	\
    ((pm) == pmap_kernel() || (pm) == curproc->p_vmspace->vm_map.pmap)

/*
 * Given a map and a machine independent protection code,
 * convert to a vax protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
int	protection_codes[8];

/*
 * Kernel page table page management.
 *
 * One additional page of KPT allows for 16 MB of virtual buffer cache.
 * A GENERIC kernel allocates this for 2 MB of real buffer cache,
 * which in turn is allocated for 38 MB of RAM.
 * We add one per 16 MB of RAM to allow for tuning the machine-independent
 * options.
 */
#ifndef NKPTADDSHIFT
#define NKPTADDSHIFT 24
#endif

struct kpt_page {
	struct kpt_page *kpt_next;	/* link on either used or free list */
	vm_offset_t	kpt_va;		/* always valid kernel VA */
	vm_offset_t	kpt_pa;		/* PA of this page (for speed) */
};
struct kpt_page *kpt_free_list, *kpt_used_list;
struct kpt_page *kpt_pages;

/*
 * Kernel segment/page table and page table map.
 * The page table map gives us a level of indirection we need to dynamically
 * expand the page table.  It is essentially a copy of the segment table
 * with PTEs instead of STEs.  All are initialized in locore at boot time.
 * Sysmap will initially contain VM_KERNEL_PT_PAGES pages of PTEs.
 * Segtabzero is an empty segment table which all processes share til they
 * reference something.
 */
u_int	*Sysseg;
u_int	*Sysseg_pa;
u_int	*Sysmap, *Sysptmap;
u_int	*Segtabzero, *Segtabzeropa;
vm_size_t	Sysptsize = VM_KERNEL_PT_PAGES + 4 / NPTEPG;

struct pmap	kernel_pmap_store;
vm_map_t	pt_map;

vm_offset_t    	avail_start;	/* PA of first available physical page */
vm_offset_t	avail_end;	/* PA of last available physical page */
vm_size_t	mem_size;	/* memory size in bytes */
vm_offset_t	virtual_avail;  /* VA of first avail page (after kernel bss)*/
vm_offset_t	virtual_end;	/* VA of last avail page (end of kernel AS) */
vm_offset_t	vm_first_phys;	/* PA of first managed page */
vm_offset_t	vm_last_phys;	/* PA just past last managed page */
boolean_t	pmap_initialized = FALSE;	/* Has pmap_init completed? */
char		*pmap_attributes;	/* reference and modify bits */
TAILQ_HEAD(pv_page_list, pv_page) pv_page_freelist;
int		pv_nfree;
#if defined(M68040) || defined(M68060)
static int	pmap_ishift;	/* segment table index shift */
int		protostfree;	/* prototype (default) free ST map */
#endif

#ifdef MACHINE_NONCONTIG
struct physeg {
	vm_offset_t start;
	vm_offset_t end;
	int first_page;
} phys_segs[16];

static	vm_offset_t avail_next;
static	vm_size_t avail_remaining;
u_long	noncontig_enable;
#endif

extern vm_offset_t z2mem_start;

boolean_t	pmap_testbit __P((register vm_offset_t, int));
void		pmap_enter_ptpage __P((register pmap_t, register vm_offset_t));

#ifdef MACHINE_NONCONTIG
#define pmap_valid_page(pa)	(pmap_initialized && pmap_page_index(pa) >= 0)
#else
#define pmap_valid_page(pa)	(pmap_initialized && pa >= vm_first_phys && \
				pa < vm_last_phys)
#endif

void pmap_activate __P((register pmap_t, struct pcb *));
extern vm_offset_t reserve_dumppages __P((vm_offset_t));
static void amiga_protection_init __P((void));
void pmap_check_wiring __P((char *, vm_offset_t));
static void pmap_changebit __P((register vm_offset_t, int, boolean_t));
struct pv_entry * pmap_alloc_pv __P((void));
void pmap_free_pv __P((struct pv_entry *));

#ifdef DEBUG            
void pmap_pvdump __P((vm_offset_t));  
#endif


/*
 * All those kernel PT submaps that BSD is so fond of
 */
caddr_t	CADDR1, CADDR2, vmmap;
u_int	*CMAP1, *CMAP2, *vmpte, *msgbufmap;
struct msgbuf	*msgbufp;

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, and allocate the system page table.
 *
 *	On the HP this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address 0 to the actual (physical)
 *	address of 0xFFxxxxxx.]
 */
void
pmap_bootstrap(firstaddr, loadaddr)
	vm_offset_t firstaddr;
	vm_offset_t loadaddr;
{
	vm_offset_t va;
	u_int *pte;
#ifdef MACHINE_NONCONTIG
	int i;
	struct boot_memseg *sp, *esp;
#endif

	avail_start = firstaddr;
	avail_end = maxmem << PGSHIFT;

	/* XXX: allow for msgbuf */
	avail_end -= m68k_round_page(sizeof(struct msgbuf));
#ifdef MACHINE_NONCONTIG
	/*
	 * first segment of memory is always the one loadbsd found
	 * found for loading the kernel into.
	 */
	avail_next = avail_start;
	avail_remaining = (avail_end - avail_start) >> PGSHIFT;
	phys_segs[0].start = avail_start;
	phys_segs[0].end = avail_end;
	sp = memlist->m_seg;
	esp = sp + memlist->m_nseg;
	i = 1;
	for (; noncontig_enable && sp < esp; sp++) {
		if ((sp->ms_attrib & MEMF_FAST) == 0)
			continue;		/* skip if not FastMem */
		if (avail_start >= sp->ms_start && avail_start <
		    sp->ms_start + sp->ms_size)
			continue;		/* skip kernel segment */
		if (sp->ms_size == 0)
			continue;		/* skip zero size segments */
		phys_segs[i].start = sp->ms_start;
		phys_segs[i].end = sp->ms_start + sp->ms_size;
#ifdef DEBUG_A4000
		/*
		 * My A4000 doesn't seem to like Zorro II memory - this
		 * hack is to skip the motherboard memory and use the
		 * Zorro II memory.  Only for trying to debug the problem.
		 * Michael L. Hitch
		 */
		if (phys_segs[i].end == 0x08000000)
			continue;	/* skip A4000 motherboard mem */
#endif
		/*
		 * Deal with Zorro II memory stolen for DMA bounce buffers.
		 * This needs to be handled better.
		 *
		 * XXX is: disabled. This is handled now in amiga_init.c
		 * by removing the stolen memory from the memlist.
		 *
		 * XXX is: enabled again, but check real size and position.
		 * We check z2mem_start is in this segment, and set its end
		 * to the z2mem_start.
		 * 
		 */
		if ((phys_segs[i].start <= z2mem_start) &&
		    (phys_segs[i].end > z2mem_start))
			phys_segs[i].end = z2mem_start;

		phys_segs[i].first_page = phys_segs[i - 1].first_page +
		    (phys_segs[i - 1].end - phys_segs[i - 1].start) / NBPG;
		avail_remaining +=
		    (phys_segs[i].end - phys_segs[i].start) / NBPG;
		physmem += (phys_segs[i].end - phys_segs[i].start) / NBPG;
		++i;
		if (noncontig_enable == 1)
			break;		/* Only two segments enabled */
	}
#endif

	mem_size = physmem << PGSHIFT;
	virtual_avail = VM_MIN_KERNEL_ADDRESS + (firstaddr - loadaddr);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 */
	amiga_protection_init();

	/*
	 * Kernel page/segment table allocated in locore,
	 * just initialize pointers.
	 */
	pmap_kernel()->pm_stpa = Sysseg_pa;
	pmap_kernel()->pm_stab = Sysseg;
	pmap_kernel()->pm_ptab = Sysmap;
#if defined(M68040) || defined(M68060)
	if (mmutype == MMU_68040) {
		pmap_ishift = SG4_SHIFT1;
		pmap_kernel()->pm_stfree = protostfree;
	} else
		pmap_ishift = SG_ISHIFT;
#endif

	simple_lock_init(&pmap_kernel()->pm_lock);
	pmap_kernel()->pm_count = 1;

	/*
	 * Allocate all the submaps we need
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*NBPG); p = pte; pte += (n);

	va = virtual_avail;
	pte = pmap_pte(pmap_kernel(), va);

	SYSMAP(caddr_t		,CMAP1		,CADDR1	   ,1		)
	SYSMAP(caddr_t		,CMAP2		,CADDR2	   ,1		)
	SYSMAP(caddr_t		,vmpte		,vmmap	   ,1		)
	SYSMAP(struct msgbuf *	,msgbufmap	,msgbufp   ,1		)

	virtual_avail = reserve_dumppages(va);
}

/*
 * Bootstrap memory allocator. This function allows for early dynamic
 * memory allocation until the virtual memory system has been bootstrapped.
 * After that point, either kmem_alloc or malloc should be used. This
 * function works by stealing pages from the (to be) managed page pool,
 * stealing virtual address space, then mapping the pages and zeroing them.
 *
 * It should be used from pmap_bootstrap till vm_page_startup, afterwards
 * it cannot be used, and will generate a panic if tried. Note that this
 * memory will never be freed, and in essence it is wired down.
 */
void *
pmap_bootstrap_alloc(size)
	int size;
{
	extern boolean_t vm_page_startup_initialized;
	vm_offset_t val;
	
	if (vm_page_startup_initialized)
		panic(
		    "pmap_bootstrap_alloc: called after startup initialized");
	size = round_page(size);
	val = virtual_avail;

	virtual_avail = pmap_map(virtual_avail, avail_start,
	    avail_start + size, VM_PROT_READ|VM_PROT_WRITE);
	avail_start += size;

	bzero((caddr_t)val, size);
	return ((void *)val);
}


/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
#ifdef MACHINE_NONCONTIG
pmap_init()
#else
pmap_init(phys_start, phys_end)
	vm_offset_t	phys_start, phys_end;
#endif
{
	extern vm_offset_t amigahwaddr;
	extern u_int	namigahwpg;
	vm_offset_t	addr, addr2;
	vm_size_t	npg, s;
	int		rv;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
#ifdef MACHINE_NONCONTIG
		printf("pmap_init(%lx, %lx)\n", avail_start, avail_end);
#else
		printf("pmap_init(%lx, %lx)\n", phys_start, phys_end);
#endif
#endif
	/*
	 * Now that kernel map has been allocated, we can mark as
	 * unavailable regions which we have mapped in locore.
	 */
	addr = amigahwaddr;
	(void)vm_map_find(kernel_map, NULL, 0, &addr, ptoa(namigahwpg), FALSE);
	if (addr != amigahwaddr)
		panic("pmap_init: bogons in the VM system!");

	addr = (vm_offset_t)Sysmap;
	vm_object_reference(kernel_object);
	(void)vm_map_find(kernel_map, kernel_object, addr, &addr,
	    AMIGA_KPTSIZE, FALSE);
	/*
	 * If this fails it is probably because the static portion of
	 * the kernel page table isn't big enough and we overran the
	 * page table map.   Need to adjust pmap_size() in amiga_init.c.
	 */
	if (addr != (vm_offset_t)Sysmap)
		panic("pmap_init: bogons in the VM system!");
#ifdef DEBUG
	if (pmapdebug & PDB_INIT) {
		printf("pmap_init: Sysseg %p, Sysmap %p, Sysptmap %p\n",
		    Sysseg, Sysmap, Sysptmap);
		printf("  pstart %lx, pend %lx, vstart %lx, vend %lx\n",
		    avail_start, avail_end, virtual_avail, virtual_end);
	}
#endif

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * initial segment table, pv_head_table and pmap_attributes.
	 */
#ifdef MACHINE_NONCONTIG
	{
		int i;
		for (npg = 0, i = 0; phys_segs[i].start; ++i)
			npg += atop(phys_segs[i].end - phys_segs[i].start);
	}
#ifdef DEBUG
	printf("pmap_init: avail_start %lx phys_segs[0].start %lx npg %ld\n",
	    avail_start, phys_segs[0].start, npg);
#endif
#else
	npg = atop(phys_end - phys_start);
#endif
	s = (vm_size_t)AMIGA_STSIZE + sizeof(struct pv_entry) * npg + npg;

	s = round_page(s);
	addr = (vm_offset_t)kmem_alloc(kernel_map, s);
	Segtabzero = (u_int *)addr;
	Segtabzeropa = (u_int *)pmap_extract(pmap_kernel(), addr);
#ifdef M68060
	if (machineid & AMIGA_68060) {
		addr2 = addr;
		while (addr2 < addr + AMIGA_STSIZE) {
			pmap_changebit(addr2, PG_CCB, 0);
			pmap_changebit(addr2, PG_CI, 1);
			addr2 += NBPG;
		}
		DCIS();
	}
#endif
	addr += AMIGA_STSIZE;
	pv_table = (pv_entry_t)addr;
	addr += sizeof (struct pv_entry) * npg;
	pmap_attributes = (char *)addr;
#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf(
		    "pmap_init: %lx bytes (%lx pgs): seg %p tbl %p attr %p\n",
		    s, npg, Segtabzero, pv_table, pmap_attributes);
#endif

	/*
	 * Allocate physical memory for kernel PT pages and their management.
	 * we need enough pages to map the page tables for each process 
	 * plus some slop.
	 */
	npg = howmany(((maxproc + 16) * AMIGA_UPTSIZE / NPTEPG), NBPG);
#ifdef NKPTADD
	npg += NKPTADD;
#else
	npg += mem_size >> NKPTADDSHIFT;
#endif
#if 1/*def DEBUG*/
	printf("Maxproc %d, mem_size %ld MB: allocating %ld KPT pages\n",
	    maxproc, mem_size>>20, npg);
#endif
	s = ptoa(npg) + round_page(npg * sizeof (struct kpt_page));

	/*
	 * Verify that space will be allocated in region for which
	 * we already have kernel PT pages.
	 */
	addr = 0;
	rv = vm_map_find(kernel_map, NULL, 0, &addr, s, TRUE);
	if (rv != KERN_SUCCESS || addr + s >= (vm_offset_t)Sysmap)
		panic("pmap_init: kernel PT too small");
	vm_map_remove(kernel_map, addr, addr + s);

	/*
	 * Now allocate the space and link the pages together to
	 * form the KPT free list.
	 */
	addr = (vm_offset_t)kmem_alloc(kernel_map, s);
	s = ptoa(npg);
	addr2 = addr + s;
	kpt_pages = &((struct kpt_page *)addr2)[npg];
	kpt_free_list = (struct kpt_page *)0;
	do {
		addr2 -= NBPG;
		(--kpt_pages)->kpt_next = kpt_free_list;
		kpt_free_list = kpt_pages;
		kpt_pages->kpt_va = addr2;
		kpt_pages->kpt_pa = pmap_extract(pmap_kernel(), addr2);
#ifdef M68060
		if (machineid & AMIGA_68060) {
			pmap_changebit(kpt_pages->kpt_pa, PG_CCB, 0);
			pmap_changebit(kpt_pages->kpt_pa, PG_CI, 1);
			DCIS();
		}
#endif
	} while (addr != addr2);

#ifdef DEBUG
	kpt_stats.kpttotal = atop(s);
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: KPT: %ld pages from %lx to %lx\n", atop(s),
		    addr, addr + s);
#endif

	/*
	 * Slightly modified version of kmem_suballoc() to get page table
	 * map where we want it.
	 */
	addr = AMIGA_UPTBASE;
	s = AMIGA_UPTMAXSIZE / AMIGA_UPTSIZE < maxproc ?
	    AMIGA_UPTMAXSIZE : maxproc * AMIGA_UPTSIZE;
	addr2 = addr + s;
	rv = vm_map_find(kernel_map, NULL, 0, &addr, s, TRUE);
	if (rv != KERN_SUCCESS)
		panic("pmap_init: cannot allocate space for PT map");
	pmap_reference(vm_map_pmap(kernel_map));
	pt_map = vm_map_create(vm_map_pmap(kernel_map), addr, addr2, TRUE);
	if (pt_map == NULL)
		panic("pmap_init: cannot create pt_map");
	rv = vm_map_submap(kernel_map, addr, addr2, pt_map);
	if (rv != KERN_SUCCESS)
		panic("pmap_init: cannot map range to pt_map");
#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: pt_map [%lx - %lx)\n", addr, addr2);
#endif

#if defined(M68040) || defined(M68060)
	if (mmutype == MMU_68040) {
		protostfree = ~1 & ~(-1 << MAXUL2SIZE);
	}
#endif

	/*
	 * Now it is safe to enable pv_table recording.
	 */
#ifdef MACHINE_NONCONTIG
	vm_first_phys = avail_start;
	vm_last_phys = avail_end;
#else
	vm_first_phys = phys_start;
	vm_last_phys = phys_end;
#endif
	pmap_initialized = TRUE;
}

#ifdef MACHINE_NONCONTIG
unsigned int
pmap_free_pages()
{
	return (avail_remaining);
}

int
pmap_next_page(addrp)
	vm_offset_t *addrp;
{
	static int cur_seg = 0;

	if (phys_segs[cur_seg].start == 0)
		return FALSE;
	if (avail_next == phys_segs[cur_seg].end) {
		avail_next = phys_segs[++cur_seg].start;
#ifdef DEBUG
		printf("pmap_next_page: next %lx remain %ld\n", avail_next,
		    avail_remaining);
#endif
	}

	if (avail_next == 0)
		return FALSE;
	*addrp = avail_next;
	avail_next += NBPG;
	avail_remaining--;
	return TRUE;
}

int
pmap_page_index(pa)
	vm_offset_t pa;
{

	struct physeg *sep = &phys_segs[0];

	while (sep->start) {
		if (pa >= sep->start && pa < sep->end)
			return (m68k_btop(pa - sep->start) + sep->first_page);
		++sep;
	}
	return -1;
}

void
pmap_virtual_space(startp, endp)
	vm_offset_t	*startp;
	vm_offset_t	*endp;
{
	*startp = virtual_avail;
	*endp = virtual_end;
}
#else
#define pmap_page_index(pa) (pa_index(pa))
#endif	/* MACHINE_NONCONTIG */

struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page *pvp;
	struct pv_entry *pv;
	int i;

	if (pv_nfree == 0) {
		pvp = (struct pv_page *)kmem_alloc(kernel_map, NBPG);
		if (pvp == 0)
			panic("pmap_alloc_pv: kmem_alloc() failed");
		pvp->pvp_pgi.pgi_freelist = pv = &pvp->pvp_pv[1];
		for (i = NPVPPG - 2; i; i--, pv++)
			pv->pv_next = pv + 1;
		pv->pv_next = 0;
		pv_nfree += pvp->pvp_pgi.pgi_nfree = NPVPPG - 1;
		TAILQ_INSERT_HEAD(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		pv = &pvp->pvp_pv[0];
	} else {
		--pv_nfree;
		pvp = pv_page_freelist.tqh_first;
		if (--pvp->pvp_pgi.pgi_nfree == 0) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		}
		pv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
		if (pv == 0)
			panic("pmap_alloc_pv: pgi_nfree inconsistent");
#endif
		pvp->pvp_pgi.pgi_freelist = pv->pv_next;
	}
	return pv;
}

void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	register struct pv_page *pvp;

	pvp = (struct pv_page *)trunc_page(pv);
	switch (++pvp->pvp_pgi.pgi_nfree) {
	case 1:
		TAILQ_INSERT_TAIL(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
	default:
		pv->pv_next = pvp->pvp_pgi.pgi_freelist;
		pvp->pvp_pgi.pgi_freelist = pv;
		++pv_nfree;
		break;
	case NPVPPG:
		pv_nfree -= NPVPPG - 1;
		TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		kmem_free(kernel_map, (vm_offset_t)pvp, NBPG);
		break;
	}
}

#ifdef not_used		/* ?? */
void
pmap_collect_pv()
{
	struct pv_page_list pv_page_collectlist;
	struct pv_page *pvp, *npvp;
	struct pv_entry *ph, *ppv, *pv, *npv;
	int s;

	TAILQ_INIT(&pv_page_collectlist);

	for (pvp = pv_page_freelist.tqh_first; pvp; pvp = npvp) {
		if (pv_nfree < NPVPPG)
			break;
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		if (pvp->pvp_pgi.pgi_nfree > NPVPPG / 3) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
			TAILQ_INSERT_TAIL(&pv_page_collectlist, pvp,
			    pvp_pgi.pgi_list);
			pv_nfree -= pvp->pvp_pgi.pgi_nfree;
			pvp->pvp_pgi.pgi_nfree = -1;
		}
	}

	if (pv_page_collectlist.tqh_first == 0)
		return;

	for (ph = &pv_table[npages - 1]; ph >= &pv_table[0]; ph--) {
		if (ph->pv_pmap == 0)
			continue;
		s = splimp();
		for (ppv = ph; (pv = ppv->pv_next) != 0; ) {
			pvp = (struct pv_page *)trunc_page(pv);
			if (pvp->pvp_pgi.pgi_nfree == -1) {
				pvp = pv_page_freelist.tqh_first;
				if (--pvp->pvp_pgi.pgi_nfree == 0) {
					TAILQ_REMOVE(&pv_page_freelist, pvp,
					    pvp_pgi.pgi_list);
				}
				npv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
				if (npv == 0)
					panic("pmap_collect_pv: "
					    "pgi_nfree inconsistent");
#endif
				pvp->pvp_pgi.pgi_freelist = npv->pv_next;
				*npv = *pv;
				ppv->pv_next = npv;
				ppv = npv;
			} else
				ppv = pv;
		}
		splx(s);
	}

	for (pvp = pv_page_collectlist.tqh_first; pvp; pvp = npvp) {
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		kmem_free(kernel_map, (vm_offset_t)pvp, NBPG);
	}
}
#endif

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(virt, start, end, prot)
	vm_offset_t	virt;
	vm_offset_t	start;
	vm_offset_t	end;
	int		prot;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_map(%lx, %lx, %lx, %x)\n", virt, start, end,
		    prot);
#endif
	while (start < end) {
		pmap_enter(pmap_kernel(), virt, start, prot, FALSE);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	return(virt);
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
pmap_t
pmap_create(size)
	vm_size_t	size;
{
	register pmap_t pmap;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create(%lx)\n", size);
#endif
	/*
	 * Software use map does not need a pmap
	 */
	if (size)
		return(NULL);

	/* XXX: is it ok to wait here? */
	pmap = (pmap_t)malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
#ifdef notifwewait
	if (pmap == NULL)
		panic("pmap_create: cannot allocate a pmap");
#endif
	bzero(pmap, sizeof(*pmap));
	pmap_pinit(pmap);
	return (pmap);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_pinit(%p)\n", pmap);
#endif
	/*
	 * No need to allocate page table space yet but we do need a
	 * valid segment table.  Initially, we point everyone at the
	 * "null" segment table.  On the first pmap_enter, a real
	 * segment table will be allocated.
	 */
	pmap->pm_stab = Segtabzero;
	pmap->pm_stpa = Segtabzeropa;
#if defined(M68040) || defined(M68060)
	if (mmutype == MMU_68040)
		pmap->pm_stfree = protostfree;
#endif
	pmap->pm_stchanged = TRUE;
	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	register pmap_t pmap;
{
	int count;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_destroy(%p)\n", pmap);
#endif
	if (pmap == NULL)
		return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	register struct pmap *pmap;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_release(%p)\n", pmap);
#endif
#ifdef notdef /* DIAGNOSTIC */
	/* count would be 0 from pmap_destroy... */
	simple_lock(&pmap->pm_lock);
	if (pmap->pm_count != 1)
		panic("pmap_release count");
#endif
	if (pmap->pm_ptab)
		kmem_free_wakeup(pt_map, (vm_offset_t)pmap->pm_ptab,
				 AMIGA_UPTSIZE);
	if (pmap->pm_stab != Segtabzero)
		kmem_free_wakeup(kernel_map, (vm_offset_t)pmap->pm_stab,
				 AMIGA_STSIZE);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%p)\n", pmap);
#endif
	if (pmap != NULL) {
		simple_lock(&pmap->pm_lock);
		pmap->pm_count++;
		simple_unlock(&pmap->pm_lock);
	}
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	register pmap_t pmap;
	vm_offset_t sva, eva;
{
	register vm_offset_t pa, va;
	register u_int *pte;
	register pv_entry_t pv, npv;
	pmap_t ptpmap;
	int *ste, s, bits;
	boolean_t flushcache = FALSE;
#if defined(M68040) || defined(M68060)
	int i;
#endif
#ifdef DEBUG
	u_int opte;

	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%p, %lx, %lx)\n", pmap, sva, eva);
#endif

	if (pmap == NULL)
		return;

#ifdef DEBUG
	remove_stats.calls++;
#endif
	for (va = sva; va < eva; va += PAGE_SIZE) {
		/*
		 * Weed out invalid mappings.
		 * Note: we assume that the segment table is always allocated.
		 */
		if (!pmap_ste_v(pmap, va)) {
			/* XXX: avoid address wrap around */
			if (va >= m68k_trunc_seg((vm_offset_t)-1))
				break;
			va = m68k_round_seg(va + PAGE_SIZE) - PAGE_SIZE;
			continue;
		}
		pte = pmap_pte(pmap, va);
		pa = pmap_pte_pa(pte);
		if (pa == 0)
			continue;
#ifdef DEBUG
		opte = *pte;
		remove_stats.removes++;
#endif
		/*
		 * Update statistics
		 */
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_REMOVE)
			printf("remove: invalidating %p\n", pte);
#endif
		bits = *(int *)pte & (PG_U|PG_M);
		*(int *)pte = PG_NV;
		if (active_pmap(pmap))
			TBIS(va);

		/*
		 * For user mappings decrement the wiring count on
		 * the PT page.  We do this after the PTE has been
		 * invalidated because vm_map_pageable winds up in
		 * pmap_pageable which clears the modify bit for the
		 * PT page.
		 */
		if (pmap != pmap_kernel()) {
			pte = pmap_pte(pmap, va);
			vm_map_pageable(pt_map, trunc_page(pte),
					round_page(pte+1), TRUE);
#ifdef DEBUG
			if (pmapdebug & PDB_WIRING)
				pmap_check_wiring("remove", trunc_page(pte));
#endif
		}
		/*
		 * Remove from the PV table (raise IPL since we
		 * may be called at interrupt time).
		 */
		if (!pmap_valid_page(pa))
			continue;
		pv = pa_to_pvh(pa);
		ste = (int *)0;
		s = splimp();
		/*
		 * If it is the first entry on the list, it is actually
		 * in the header and we must copy the following entry up
		 * to the header.  Otherwise we must search the list for
		 * the entry.  In either case we free the now unused entry.
		 */
		if (pmap == pv->pv_pmap && va == pv->pv_va) {
			ste = (int *)pv->pv_ptste;
			ptpmap = pv->pv_ptpmap;
			npv = pv->pv_next;
			if (npv) {
				*pv = *npv;
				pmap_free_pv(npv);
			} else
				pv->pv_pmap = NULL;
#ifdef DEBUG
			remove_stats.pvfirst++;
#endif
		} else {
			for (npv = pv->pv_next; npv; npv = npv->pv_next) {
#ifdef DEBUG
				remove_stats.pvsearch++;
#endif
				if (pmap == npv->pv_pmap && va == npv->pv_va)
					break;
				pv = npv;
			}
#ifdef DEBUG
			if (npv == NULL) {
#ifdef MACHINE_NONCONTIG	/* XXX this need to be fixed */
				printf("pmap_remove: PA %lx index %d\n", pa,
				    pa_index(pa));
#else
				printf("pmap_remove: PA %lx index %ld\n", pa,
				    pa_index(pa));
#endif
				panic("pmap_remove: PA not in pv_tab");
			}
#endif
			ste = (int *)npv->pv_ptste;
			ptpmap = npv->pv_ptpmap;
			pv->pv_next = npv->pv_next;
			pmap_free_pv(npv);
			pv = pa_to_pvh(pa);
		}
		/*
		 * If this was a PT page we must also remove the
		 * mapping from the associated segment table.
		 */
		if (ste) {
#ifdef DEBUG
			remove_stats.ptinvalid++;
			if (pmapdebug & (PDB_REMOVE|PDB_PTPAGE)) {
				printf("remove: ste was %x@%p pte was %x@%p\n",
				    *ste, ste, *(int *)&opte,
				    pmap_pte(pmap, va));
			}
#endif
#if defined(M68040) || defined(M68060)
			if (mmutype == MMU_68040) {
			/*
			 * On the 68040, the PT page contains
			 * NPTEPG/SG4_LEV3SIZE page tables, so we need to
			 * remove all the associated segment table entries
			 * (This may be incorrect:  if a single page table is
			 * being removed, the whole page should not be
			 * removed.)
			 */
				for (i = 0; i < NPTEPG / SG4_LEV3SIZE; ++i)
					*ste++ = SG_NV;
				ste -= NPTEPG / SG4_LEV3SIZE;
#ifdef DEBUG
				if (pmapdebug &
				    (PDB_REMOVE|PDB_SEGTAB|0x10000))
					printf(
					    "pmap_remove:PT at %lx removed\n",
					    va);
#endif
			}
			else
#endif /* M68040 || M68060 */
				*ste = SG_NV;
			/*
			 * If it was a user PT page, we decrement the
			 * reference count on the segment table as well,
			 * freeing it if it is now empty.
			 */
			if (ptpmap != pmap_kernel()) {
#ifdef DEBUG
				if (pmapdebug & (PDB_REMOVE|PDB_SEGTAB))
					printf("remove: stab %p, refcnt %d\n",
					    ptpmap->pm_stab,
					    ptpmap->pm_sref - 1);
				if ((pmapdebug & PDB_PARANOIA) &&
				    ptpmap->pm_stab !=
				    (u_int *)trunc_page(ste))
					panic("remove: bogus ste");
#endif
				if (--(ptpmap->pm_sref) == 0) {
#ifdef DEBUG
					if (pmapdebug&(PDB_REMOVE|PDB_SEGTAB))
					printf("remove: free stab %p\n",
					     ptpmap->pm_stab);
#endif
					kmem_free_wakeup(kernel_map,
					    (vm_offset_t)ptpmap->pm_stab,
					    AMIGA_STSIZE);
					ptpmap->pm_stab = Segtabzero;
					ptpmap->pm_stpa = Segtabzeropa;
#if defined(M68040) || defined(M68060)
					if (mmutype == MMU_68040)
						ptpmap->pm_stfree =
						    protostfree;
#endif
					ptpmap->pm_stchanged = TRUE;
					/*
					 * XXX may have changed segment table
					 * pointer for current process so
					 * update now to reload hardware.
					 */
					if (curproc &&
					    ptpmap ==
					    curproc->p_vmspace->vm_map.pmap)
						PMAP_ACTIVATE(ptpmap,
						    (struct pcb *)
						    curproc->p_addr, 1);
				}
			}
			if (ptpmap == pmap_kernel())
				TBIAS();
			else
				TBIAU();
			pv->pv_flags &= ~PV_PTPAGE;
			ptpmap->pm_ptpages--;
		}
		/*
		 * Update saved attributes for managed page
		 */
		pmap_attributes[pa_index(pa)] |= bits;
		splx(s);
	}
	if (flushcache) {
		if (pmap == pmap_kernel()) {
			DCIS();
#ifdef DEBUG
			remove_stats.sflushes++;
#endif
		} else {
			DCIU();
#ifdef DEBUG
			remove_stats.uflushes++;
#endif
		}
	}
}

/*
 *	pmap_page_protect:
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pa, prot)
	vm_offset_t	pa;
	vm_prot_t	prot;
{
	register pv_entry_t pv;
	int s;

#ifdef DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    (prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE)))
		printf("pmap_page_protect(%lx, %x)\n", pa, prot);
#endif
	if (!pmap_valid_page(pa))
		return;

	switch (prot) {
	case VM_PROT_ALL:
		break;
	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pmap_changebit(pa, PG_RO, TRUE);
		break;
	/* remove_all */
	default:
		pv = pa_to_pvh(pa);
		s = splimp();
		while (pv->pv_pmap != NULL) {
#ifdef DEBUG
			if (!pmap_ste_v(pv->pv_pmap,pv->pv_va) ||
			    pmap_pte_pa(pmap_pte(pv->pv_pmap,pv->pv_va)) != pa)
{
    printf("pmap_page_protect: va %lx, pmap_ste_v %d pmap_pte_pa %08x/%lx\n",
	pv->pv_va, pmap_ste_v(pv->pv_pmap,pv->pv_va),
	pmap_pte_pa(pmap_pte(pv->pv_pmap,pv->pv_va)), pa);
    printf(" pvh %p pv %p pv_next %p\n", pa_to_pvh(pa), pv, pv->pv_next);
    panic("pmap_page_protect: bad mapping");
}
#endif
			pmap_remove(pv->pv_pmap, pv->pv_va,
			    pv->pv_va + PAGE_SIZE);
		}
		splx(s);
		break;
	}
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	register pmap_t	pmap;
	vm_offset_t	sva, eva;
	vm_prot_t	prot;
{
	register u_int *pte;
	register vm_offset_t va;
	boolean_t needtflush;
	int isro;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%p, %lx, %lx, %x)\n", pmap, sva, eva,
		    prot);
#endif
	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	pte = pmap_pte(pmap, sva);
	isro = pte_prot(pmap, prot) == PG_RO ? 1 : 0;
	needtflush = active_pmap(pmap);
	for (va = sva; va < eva; va += PAGE_SIZE) {
		/*
		 * Page table page is not allocated.
		 * Skip it, we don't want to force allocation
		 * of unnecessary PTE pages just to set the protection.
		 */
		if (!pmap_ste_v(pmap, va)) {
			/* XXX: avoid address wrap around */
			if (va >= m68k_trunc_seg((vm_offset_t)-1))
				break;
			va = m68k_round_seg(va + PAGE_SIZE) - PAGE_SIZE;
			pte = pmap_pte(pmap, va);
			pte++;
			continue;
		}
		/*
		 * skip if page not valid or protection is same
		 */
		if (!pmap_pte_v(pte) || !pmap_pte_prot_chg(pte, isro)) {
			pte++;
			continue;
		}
#if defined(M68040) || defined(M68060)
		/*
		 * Clear caches if making RO (see section
		 * "7.3 Cache Coherency" in the manual).
		 */
		if (isro && mmutype == MMU_68040) {
			vm_offset_t pa = pmap_pte_pa(pte);

			DCFP(pa);
			ICPP(pa);
		}
#endif
		pmap_pte_set_prot(pte, isro);
		if (needtflush)
			TBIS(va);
		pte++;
	}
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
extern int kernel_copyback;

void
pmap_enter(pmap, va, pa, prot, wired)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	register u_int *pte;
	register int npte;
	vm_offset_t opa;
	boolean_t cacheable = TRUE;
	boolean_t checkpv = TRUE;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%p, %lx, %lx, %x, %x)\n", pmap, va, pa,
		    prot, wired);
#endif
	if (pmap == NULL)
		return;

#ifdef DEBUG
	if (pmap == pmap_kernel())
		enter_stats.kernel++;
	else
		enter_stats.user++;
#endif
	/*
	 * For user mapping, allocate kernel VM resources if necessary.
	 */
	if (pmap->pm_ptab == NULL)
		pmap->pm_ptab = (u_int *)
			kmem_alloc_wait(pt_map, AMIGA_UPTSIZE);

	/*
	 * Segment table entry not valid, we need a new PT page
	 */
	if (!pmap_ste_v(pmap, va))
		pmap_enter_ptpage(pmap, va);

	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: pte %p, *pte %x\n", pte, *(int *)pte);
#endif

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
#ifdef DEBUG
		enter_stats.pwchange++;
#endif
		/*
		 * Wiring change, just update stats.
		 * We don't worry about wiring PT pages as they remain
		 * resident as long as there are valid mappings in them.
		 * Hence, if a user page is wired, the PT page will be also.
		 */
		if ((wired && !pmap_pte_w(pte)) ||
		    (!wired && pmap_pte_w(pte))) {
#ifdef DEBUG
			if (pmapdebug & PDB_ENTER)
				printf("enter: wiring change -> %x\n", wired);
#endif
			if (wired)
				pmap->pm_stats.wired_count++;
			else
				pmap->pm_stats.wired_count--;
#ifdef DEBUG
			enter_stats.wchange++;
#endif
		}
		/*
		 * Retain cache inhibition status
		 */
		checkpv = FALSE;
		if (pmap_pte_ci(pte))
			cacheable = FALSE;
		goto validate;
	}

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: removing old mapping %lx\n", va);
#endif
		pmap_remove(pmap, va, va + PAGE_SIZE);
#ifdef DEBUG
		enter_stats.mchange++;
#endif
	}

	/*
	 * If this is a new user mapping, increment the wiring count
	 * on this PT page.  PT pages are wired down as long as there
	 * is a valid mapping in the page.
	 */
	if (pmap != pmap_kernel())
		vm_map_pageable(pt_map, trunc_page(pte),
				round_page(pte+1), FALSE);

	/*
	 * Enter on the PV list if part of our managed memory
	 * Note that we raise IPL while manipulating pv_table
	 * since pmap_enter can be called at interrupt time.
	 */
	if (pmap_valid_page(pa)) {
		register pv_entry_t pv, npv;
		int s;

#ifdef DEBUG
		enter_stats.managed++;
#endif
		pv = pa_to_pvh(pa);
		s = splimp();
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: pv at %p: %lx/%p/%p\n", pv, pv->pv_va,
			    pv->pv_pmap, pv->pv_next);
#endif
		/*
		 * No entries yet, use header as the first entry
		 */
		if (pv->pv_pmap == NULL) {
#ifdef DEBUG
			enter_stats.firstpv++;
#endif
			pv->pv_va = va;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
			pv->pv_ptste = NULL;
			pv->pv_ptpmap = NULL;
			pv->pv_flags = 0;
		}
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		else {
#ifdef DEBUG
			for (npv = pv; npv; npv = npv->pv_next)
				if (pmap == npv->pv_pmap && va == npv->pv_va)
					panic("pmap_enter: already in pv_tab");
#endif
			npv = pmap_alloc_pv();
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			npv->pv_ptste = NULL;
			npv->pv_ptpmap = NULL;
			pv->pv_next = npv;
#ifdef DEBUG
			if (!npv->pv_next)
				enter_stats.secondpv++;
#endif
		}
		splx(s);
	}
	/*
	 * Assumption: if it is not part of our managed memory
	 * then it must be device memory which may be volitile.
	 */
	else if (pmap_initialized) {
		checkpv = cacheable = FALSE;
#ifdef DEBUG
		enter_stats.unmanaged++;
#endif
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * AMIGA pages in a MACH page.
	 */
#if defined(M68040) || defined(M68060)
#if DEBUG
	if (pmapdebug & 0x10000 && mmutype == MMU_68040 && 
	    pmap == pmap_kernel()) {
		char *s;
		if (va >= AMIGA_UPTBASE && 
		    va < (AMIGA_UPTBASE + AMIGA_UPTMAXSIZE))
			s = "UPT";
		else if (va >= (u_int)Sysmap && 
		    va < ((u_int)Sysmap + AMIGA_KPTSIZE))
			s = "KPT";
		else if (va >= (u_int)pmap->pm_stab && 
		    va < ((u_int)pmap->pm_stab + AMIGA_STSIZE))
			s = "KST";
		else if (curproc && 
		    va >= (u_int)curproc->p_vmspace->vm_map.pmap->pm_stab &&
		    va < ((u_int)curproc->p_vmspace->vm_map.pmap->pm_stab +
		    AMIGA_STSIZE))
			s = "UST";
		else
			s = "other";
		printf("pmap_init: validating %s kernel page at %lx -> %lx\n",
		    s, va, pa);

	}
#endif
	if (mmutype == MMU_68040 && pmap == pmap_kernel() &&
	    ((va >= AMIGA_UPTBASE &&
	    va < (AMIGA_UPTBASE + AMIGA_UPTMAXSIZE)) ||
	    (va >= (u_int)Sysmap && va < ((u_int)Sysmap + AMIGA_KPTSIZE))))
		cacheable = FALSE;	/* don't cache user page tables */

	/* Don't cache if process can't take it, like SunOS ones.  */
	if (mmutype == MMU_68040 && pmap != pmap_kernel() &&
	    (curproc->p_md.md_flags & MDP_UNCACHE_WX) &&
	    (prot & VM_PROT_EXECUTE) && (prot & VM_PROT_WRITE))
		checkpv = cacheable = FALSE;
#endif
	npte = (pa & PG_FRAME) | pte_prot(pmap, prot) | PG_V;
	npte |= (*(int *)pte & (PG_M|PG_U));
	if (wired)
		npte |= PG_W;
	if (!checkpv && !cacheable)
		npte |= PG_CI;
#if defined(M68040) || defined(M68060)
	else if (mmutype == MMU_68040 && (npte & PG_PROT) == PG_RW &&
	    (kernel_copyback || pmap != pmap_kernel()))
		npte |= PG_CCB;		/* cache copyback */
#endif
	/*
	 * Remember if this was a wiring-only change.
	 * If so, we need not flush the TLB and caches.
	 */
	wired = ((*(int *)pte ^ npte) == PG_W);
#if defined(M68040) || defined(M68060)
	if (mmutype == MMU_68040 && !wired) {
		DCFP(pa);
		ICPP(pa);
	}
#endif
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: new pte value %x\n", npte);
#endif
	*(int *)pte++ = npte;
	if (!wired && active_pmap(pmap))
		TBIS(va);
#ifdef DEBUG
	if ((pmapdebug & PDB_WIRING) && pmap != pmap_kernel()) {
		va -= PAGE_SIZE;
		pmap_check_wiring("enter", trunc_page(pmap_pte(pmap, va)));
	}
#endif
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap, va, wired)
	register pmap_t	pmap;
	vm_offset_t	va;
	boolean_t	wired;
{
	register u_int *pte;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_change_wiring(%p, %lx, %x)\n", pmap, va, wired);
#endif
	if (pmap == NULL)
		return;

	pte = pmap_pte(pmap, va);
#ifdef DEBUG
	/*
	 * Page table page is not allocated.
	 * Should this ever happen?  Ignore it for now,
	 * we don't want to force allocation of unnecessary PTE pages.
	 */
	if (!pmap_ste_v(pmap, va)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid STE for %lx\n",
			    va);
		return;
	}
	/*
	 * Page not valid.  Should this ever happen?
	 * Just continue and change wiring anyway.
	 */
	if (!pmap_pte_v(pte)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid PTE for %lx\n",
			    va);
	}
#endif
	if ((wired && !pmap_pte_w(pte)) || (!wired && pmap_pte_w(pte))) {
		if (wired)
			pmap->pm_stats.wired_count++;
		else
			pmap->pm_stats.wired_count--;
	}
	/*
	 * Wiring is not a hardware characteristic so there is no need
	 * to invalidate TLB.
	 */
	pmap_pte_set_w(pte, wired);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */

vm_offset_t
pmap_extract(pmap, va)
	register pmap_t	pmap;
	vm_offset_t va;
{
	register vm_offset_t pa;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%p, %lx) -> ", pmap, va);
#endif
	pa = 0;
	if (pmap && pmap_ste_v(pmap, va))
		pa = *(int *)pmap_pte(pmap, va);
	if (pa)
		pa = (pa & PG_FRAME) | (va & ~PG_FRAME);
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("%lx\n", pa);
#endif
	return (pa);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
	vm_offset_t	dst_addr;
	vm_size_t	len;
	vm_offset_t	src_addr;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy(%p, %p, %lx, %lx, %lx)\n", dst_pmap,
		    src_pmap, dst_addr, len, src_addr);
#endif
}

/*
 *	Require that all active physical maps contain no
 *	incorrect entries NOW.  [This update includes
 *	forcing updates of any address map caching.]
 *
 *	Generally used to insure that a thread about
 *	to run will see a semantically correct world.
 */
void pmap_update()
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_update()\n");
#endif
#if defined(M68060)
	if (machineid & AMIGA_68060)
		DCIA();
#endif
	TBIA();
}

/*
 *	Routine:	pmap_collect
 *	Function:
 *		Garbage collects the physical map system for
 *		pages which are no longer used.
 *		Success need not be guaranteed -- that is, there
 *		may well be pages which are not referenced, but
 *		others may be collected.
 *	Usage:
 *		Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap)
	pmap_t		pmap;
{
	register vm_offset_t pa;
	register pv_entry_t pv;
	register int *pte;
	vm_offset_t kpa;
	int s;

#ifdef DEBUG
	int *ste;
	int opmapdebug = 0;
#endif
	if (pmap != pmap_kernel())
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_collect(%p)\n", pmap);
	kpt_stats.collectscans++;
#endif
	s = splimp();
	for (pa = vm_first_phys; pa < vm_last_phys; pa += PAGE_SIZE) {
		register struct kpt_page *kpt, **pkpt;

		/*
		 * Locate physical pages which are being used as kernel
		 * page table pages.
		 */
		pv = pa_to_pvh(pa);
		if (pv->pv_pmap != pmap_kernel() ||
		    !(pv->pv_flags & PV_PTPAGE))
			continue;
		do {
			if (pv->pv_ptste && pv->pv_ptpmap == pmap_kernel())
				break;
		} while ((pv = pv->pv_next) > 0);
		if (pv == NULL)
			continue;
#ifdef DEBUG
		if (pv->pv_va < (vm_offset_t)Sysmap ||
		    pv->pv_va >= (vm_offset_t)Sysmap + AMIGA_KPTSIZE)
			printf("collect: kernel PT VA out of range\n");
		else
			goto ok;
		pmap_pvdump(pa);
		continue;
ok:
#endif
		pte = (int *)(pv->pv_va + NBPG);
		while (--pte >= (int *)pv->pv_va && *pte == PG_NV)
			;
		if (pte >= (int *)pv->pv_va)
			continue;

#ifdef DEBUG
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT)) {
			printf(
			    "collect: freeing KPT page at %lx (ste %x@%p)\n",
			    pv->pv_va, *(int *)pv->pv_ptste, pv->pv_ptste);
			opmapdebug = pmapdebug;
			pmapdebug |= PDB_PTPAGE;
		}

		ste = (int *)pv->pv_ptste;
#endif
		/*
		 * If all entries were invalid we can remove the page.
		 * We call pmap_remove to take care of invalidating ST
		 * and Sysptmap entries.
		 */
		kpa = pmap_extract(pmap, pv->pv_va);
		pmap_remove(pmap, pv->pv_va, pv->pv_va + NBPG);

		/*
		 * Use the physical address to locate the original
		 * (kmem_alloc assigned) address for the page and put
		 * that page back on the free list.
		 */
		for (pkpt = &kpt_used_list, kpt = *pkpt;
		     kpt != (struct kpt_page *)0;
		     pkpt = &kpt->kpt_next, kpt = *pkpt)
			if (kpt->kpt_pa == kpa)
				break;
#ifdef DEBUG
		if (kpt == (struct kpt_page *)0)
			panic("pmap_collect: lost a KPT page");
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			printf("collect: %lx (%lx) to free list\n",
			    kpt->kpt_va, kpa);
#endif
		*pkpt = kpt->kpt_next;
		kpt->kpt_next = kpt_free_list;
		kpt_free_list = kpt;
#ifdef DEBUG
		kpt_stats.kptinuse--;
		kpt_stats.collectpages++;
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			pmapdebug = opmapdebug;

		if (*ste)
			printf("collect: kernel STE at %p still valid (%x)\n",
			    ste, *ste);
		ste =
		    (int *)&Sysptmap[(u_int *)ste-pmap_ste(pmap_kernel(), 0)];
		if (*ste)
			printf(
			    "collect: kernel PTmap at %p still valid (%x)\n",
			    ste, *ste);
#endif
	}
	splx(s);
}

void
pmap_activate(pmap, pcbp)
	register pmap_t pmap;
	struct pcb *pcbp;
{
#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_SEGTAB))
		printf("pmap_activate(%p, %p)\n", pmap, pcbp);
#endif
	PMAP_ACTIVATE(pmap, pcbp, pmap == curproc->p_vmspace->vm_map.pmap);
}

/*
 *	pmap_zero_page zeros the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bzero to clear its contents, one machine dependent page
 *	at a time.
 */
void
pmap_zero_page(phys)
	register vm_offset_t	phys;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%lx)\n", phys);
#endif
	phys >>= PG_SHIFT;
	clearseg(phys);
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(src, dst)
	register vm_offset_t	src, dst;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%lx, %lx)\n", src, dst);
#endif
	src >>= PG_SHIFT;
	dst >>= PG_SHIFT;
	physcopyseg(src, dst);
}


/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(pmap, sva, eva, pageable)
	pmap_t		pmap;
	vm_offset_t	sva, eva;
	boolean_t	pageable;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_pageable(%p, %lx, %lx, %x)\n", pmap, sva, eva,
		    pageable);
#endif
	/*
	 * If we are making a PT page pageable then all valid
	 * mappings must be gone from that page.  Hence it should
	 * be all zeros and there is no need to clean it.
	 * Assumptions:
	 *	- we are called with only one page at a time
	 *	- PT pages have only one pv_table entry
	 */
	if (pmap == pmap_kernel() && pageable && sva + PAGE_SIZE == eva) {
		register pv_entry_t pv;
		register vm_offset_t pa;

#ifdef DEBUG
		if ((pmapdebug & (PDB_FOLLOW|PDB_PTPAGE)) == PDB_PTPAGE)
			printf("pmap_pageable(%p, %lx, %lx, %x)\n", pmap, sva,
			    eva, pageable);
#endif
		if (!pmap_ste_v(pmap, sva))
			return;
		pa = pmap_pte_pa(pmap_pte(pmap, sva));
		if (!pmap_valid_page(pa))
			return;
		pv = pa_to_pvh(pa);
		if (pv->pv_ptste == NULL)
			return;
#ifdef DEBUG
		if (pv->pv_va != sva || pv->pv_next) {
			printf("pmap_pageable: bad PT page va %lx next %p\n",
			    pv->pv_va, pv->pv_next);
			return;
		}
#endif
		/*
		 * Mark it unmodified to avoid pageout
		 */
		pmap_changebit(pa, PG_M, FALSE);
#ifdef DEBUG
		if (pmapdebug & PDB_PTPAGE)
			printf("pmap_pageable: PT page %lx(%x) unmodified\n",
			    sva, *(int *)pmap_pte(pmap, sva));
		if (pmapdebug & PDB_WIRING)
			pmap_check_wiring("pageable", sva);
#endif
	}
}

/*
 *	Clear the modify bits on the specified physical page.
 */

void
pmap_clear_modify(pa)
	vm_offset_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_modify(%lx)\n", pa);
#endif
	pmap_changebit(pa, PG_M, FALSE);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */

void pmap_clear_reference(pa)
	vm_offset_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_reference(%lx)\n", pa);
#endif
	pmap_changebit(pa, PG_U, FALSE);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */

boolean_t
pmap_is_referenced(pa)
	vm_offset_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_U);
		printf("pmap_is_referenced(%lx) -> %c\n", pa, "FT"[rv]);
		return (rv);
	}
#endif
	return (pmap_testbit(pa, PG_U));
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */

boolean_t
pmap_is_modified(pa)
	vm_offset_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_M);
		printf("pmap_is_modified(%lx) -> %c\n", pa, "FT"[rv]);
		return (rv);
	}
#endif
	return (pmap_testbit(pa, PG_M));
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return(m68k_ptob(ppn));
}

/*
 * Miscellaneous support routines follow
 */

static void
amiga_protection_init()
{
	register int *kp, prot;

	kp = protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = 0;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = PG_RO;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_RW;
			break;
		}
	}
}

/* static */
boolean_t
pmap_testbit(pa, bit)
	register vm_offset_t pa;
	int bit;
{
	register pv_entry_t pv;
	register int *pte;
	int s;

	if (!pmap_valid_page(pa))
		return (FALSE);

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Check saved info first
	 */
	if (pmap_attributes[pa_index(pa)] & bit) {
		splx(s);
		return (TRUE);
	}
	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			pte = (int *)pmap_pte(pv->pv_pmap, pv->pv_va);
			if (*pte & bit) {
				splx(s);
				return (TRUE);
			}
		}
	}
	splx(s);
	return (FALSE);
}

static void
pmap_changebit(pa, bit, setem)
	register vm_offset_t pa;
	int bit;
	boolean_t setem;
{
	register pv_entry_t pv;
	register int *pte, npte;
	vm_offset_t va;
	boolean_t firstpage;
	int s;

	firstpage = TRUE;

#ifdef DEBUG
	if (pmapdebug & PDB_BITS)
		printf("pmap_changebit(%lx, %x, %s)\n", pa, bit,
		    setem ? "set" : "clear");
#endif
	if (!pmap_valid_page(pa))
		return;

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Clear saved attributes (modify, reference)
	 */
	if (!setem)
		pmap_attributes[pa_index(pa)] &= ~bit;
	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap == NULL) {
		splx(s);
		return;
	}
	for (; pv; pv = pv->pv_next) {
		va = pv->pv_va;

		/*
		 * XXX don't write protect pager mappings
		 */
		if (bit == PG_RO) {
			extern vm_offset_t pager_sva, pager_eva;

			if (va >= pager_sva && va < pager_eva)
				continue;
		}

		pte = (int *)pmap_pte(pv->pv_pmap, va);
		if (setem)
			npte = *pte | bit;
		else
			npte = *pte & ~bit;
		if (*pte != npte) {
			/*
			 * If we are changing caching status or
			 * protection make sure the caches are
			 * flushed (but only once).
			 */
#if defined(M68040) || defined(M68060)
			if (firstpage && mmutype == MMU_68040 &&
			    ((bit == PG_RO && setem) || (bit & PG_CMASK))) {
				firstpage = FALSE;
				DCFP(pa);
				ICPP(pa);
			}
#endif
			*pte = npte;
			if (active_pmap(pv->pv_pmap))
				TBIS(va);
		}
	}
	splx(s);
}

/* static */
void
pmap_enter_ptpage(pmap, va)
	register pmap_t pmap;
	register vm_offset_t va;
{
	register vm_offset_t ptpa;
	register pv_entry_t pv;
#ifdef M68060
	u_int stpa;
#endif
	u_int *ste;
	int s;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER|PDB_PTPAGE))
		printf("pmap_enter_ptpage: pmap %p, va %lx\n", pmap, va);
	enter_stats.ptpneeded++;
#endif
	/*
	 * Allocate a segment table if necessary.  Note that it is allocated
	 * from kernel_map and not pt_map.  This keeps user page tables
	 * aligned on segment boundaries in the kernel address space.
	 * The segment table is wired down.  It will be freed whenever the
	 * reference count drops to zero.
	 */
	if (pmap->pm_stab == Segtabzero) {
		pmap->pm_stab = (u_int *)
			kmem_alloc(kernel_map, AMIGA_STSIZE);
		pmap->pm_stpa = (u_int *)pmap_extract(
		    pmap_kernel(), (vm_offset_t)pmap->pm_stab);
#if defined(M68040) || defined(M68060)
		if (mmutype == MMU_68040) {
#if defined(M68060)
			stpa = (u_int)pmap->pm_stpa;
			if (machineid & AMIGA_68060) {
				while (stpa < (u_int)pmap->pm_stpa + 
				    AMIGA_STSIZE) {
					pmap_changebit(stpa, PG_CCB, 0);
					pmap_changebit(stpa, PG_CI, 1);
					stpa += NBPG;
				}
				DCIS(); /* XXX */
	 		}
#endif
			pmap->pm_stfree = protostfree;
		}
#endif
		pmap->pm_stchanged = TRUE;
		/*
		 * XXX may have changed segment table pointer for current
		 * process so update now to reload hardware.
		 */
		if (pmap == curproc->p_vmspace->vm_map.pmap)
			PMAP_ACTIVATE(pmap, (struct pcb *)curproc->p_addr, 1);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter_pt: pmap %p stab %p(%p)\n", pmap,
			    pmap->pm_stab, pmap->pm_stpa);
#endif
	}

	ste = pmap_ste(pmap, va);

#if defined(M68040) || defined(M68060)
	/*
	 * Allocate level 2 descriptor block if necessary
	 */
	if (mmutype == MMU_68040) {
		if (*ste == SG_NV) {
			int ix;
			caddr_t addr;

			ix = bmtol2(pmap->pm_stfree);
			if (ix == -1)
				panic("enter_pt: out of address space");
			pmap->pm_stfree &= ~l2tobm(ix);
			addr = (caddr_t)&pmap->pm_stab[ix * SG4_LEV2SIZE];
			bzero(addr, SG4_LEV2SIZE * sizeof(st_entry_t));
			addr = (caddr_t)&pmap->pm_stpa[ix * SG4_LEV2SIZE];
			*ste = (u_int) addr | SG_RW | SG_U | SG_V;
#if 0 /* XXX should be superfluous here: defined(M68060) */
			if (machineid & AMIGA_68060) {
				pmap_changebit(addr, PG_CCB, 0);
				pmap_changebit(addr, PG_CI, 1);
				DCIS(); /* XXX */
			}
#endif
#ifdef DEBUG
			if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
				printf("enter_pt: alloc ste2 %d(%p)\n", ix,
				    addr);
#endif
		}
		ste = pmap_ste2(pmap, va);
		/*
		 * Since a level 2 descriptor maps a block of SG4_LEV3SIZE
		 * level 3 descriptors, we need a chunk of NPTEPG/SEG4_LEV3SIZE
		 * (64) such descriptors (NBPG/SG4_LEV3SIZE bytes) to map a
		 * PT page -- the unit of allocation.  We set 'ste' to point
		 * to the first entry of that chunk which is validated in its
		 * entirety below.
		 */
		ste = (u_int *)((int)ste & ~(NBPG / SG4_LEV3SIZE - 1));
#ifdef DEBUG
		if (pmapdebug &  (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter_pt: ste2 %p (%p)\n", pmap_ste2(pmap, va),
			    ste);
#endif
	}
#endif
	va = trunc_page((vm_offset_t)pmap_pte(pmap, va));

	/*
	 * In the kernel we allocate a page from the kernel PT page
	 * free list and map it into the kernel page table map (via
	 * pmap_enter).
	 */
	if (pmap == pmap_kernel()) {
		register struct kpt_page *kpt;

		s = splimp();
		if ((kpt = kpt_free_list) == (struct kpt_page *)0) {
			/*
			 * No PT pages available.
			 * Try once to free up unused ones.
			 */
#ifdef DEBUG
			if (pmapdebug & PDB_COLLECT)
				printf(
				    "enter_pt: no KPT pages, collecting...\n");
#endif
			pmap_collect(pmap_kernel());
			if ((kpt = kpt_free_list) == (struct kpt_page *)0)
				panic("pmap_enter_ptpage: can't get KPT page");
		}
#ifdef DEBUG
		if (++kpt_stats.kptinuse > kpt_stats.kptmaxuse)
			kpt_stats.kptmaxuse = kpt_stats.kptinuse;
#endif
		kpt_free_list = kpt->kpt_next;
		kpt->kpt_next = kpt_used_list;
		kpt_used_list = kpt;
		ptpa = kpt->kpt_pa;
		bzero((char *)kpt->kpt_va, NBPG);
		pmap_enter(pmap, va, ptpa, VM_PROT_DEFAULT, TRUE);
#if defined(M68060)
		if (machineid & AMIGA_68060) {
			pmap_changebit(ptpa, PG_CCB, 0);
			pmap_changebit(ptpa, PG_CI, 1);
			DCIS();
	 	}
#endif
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
			printf(
			    "enter_pt: add &Sysptmap[%d]: %x (KPT page %lx)\n",
			    ste - pmap_ste(pmap, 0),
			    *(int *)&Sysptmap[ste - pmap_ste(pmap, 0)],
			    kpt->kpt_va);
#endif
		splx(s);
	}
	/*
	 * For user processes we just simulate a fault on that location
	 * letting the VM system allocate a zero-filled page.
	 */
	else {
		/*
		 * Count the segment table reference now so that we won't
		 * lose the segment table when low on memory.
		 */
		pmap->pm_sref++;
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
			printf("enter_pt: about to fault UPT pg at %lx\n", va);
#endif
		if (vm_fault(pt_map, va, VM_PROT_READ|VM_PROT_WRITE, FALSE)
		    != KERN_SUCCESS)
			panic("pmap_enter: vm_fault failed");
		ptpa = pmap_extract(pmap_kernel(), va);
#ifdef DEBUG
		PHYS_TO_VM_PAGE(ptpa)->flags |=  PG_PTPAGE;
#endif
	}

#ifdef M68060
	if (machineid & M68060) {
		pmap_changebit(ptpa, PG_CCB, 0);
		pmap_changebit(ptpa, PG_CI, 1);
		DCIS();
	}
#endif
	/*
	 * Locate the PV entry in the kernel for this PT page and
	 * record the STE address.  This is so that we can invalidate
	 * the STE when we remove the mapping for the page.
	 */
	pv = pa_to_pvh(ptpa);
	s = splimp();
	if (pv) {
		pv->pv_flags |= PV_PTPAGE;
		do {
			if (pv->pv_pmap == pmap_kernel() && pv->pv_va == va)
				break;
		} while ((pv = pv->pv_next) > 0);
	}
#ifdef DEBUG
	if (pv == NULL) {
		printf("enter_pt: PV entry for PT page %lx not found\n", ptpa);
		panic("pmap_enter_ptpage: PT page not entered");
	}
#endif
	pv->pv_ptste = ste;
	pv->pv_ptpmap = pmap;
#ifdef DEBUG
	if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
		printf("enter_pt: new PT page at PA %lx, ste at %p\n", ptpa,
		    ste);
#endif

	/*
	 * Map the new PT page into the segment table.
	 * Also increment the reference count on the segment table if this
	 * was a user page table page.  Note that we don't use vm_map_pageable
	 * to keep the count like we do for PT pages, this is mostly because
	 * it would be difficult to identify ST pages in pmap_pageable to
	 * release them.  We also avoid the overhead of vm_map_pageable.
	 */
#if defined(M68040) || defined(M68060)
	if (mmutype == MMU_68040) {
		u_int *este;

		for (este = &ste[NPTEPG / SG4_LEV3SIZE]; ste < este; ++ste) {
			*ste = ptpa | SG_U | SG_RW | SG_V;
			ptpa += SG4_LEV3SIZE * sizeof(st_entry_t);
		}
	}
	else
#endif
		*(int *)ste = (ptpa & SG_FRAME) | SG_RW | SG_V;
	if (pmap != pmap_kernel()) {
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter_pt: stab %p refcnt %d\n", pmap->pm_stab,
			    pmap->pm_sref);
#endif
	}
	/*
	 * Flush stale TLB info.
	 */
	if (pmap == pmap_kernel())
		TBIAS();
	else
		TBIAU();
	pmap->pm_ptpages++;
	splx(s);
}

#ifdef DEBUG
void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv;

	printf("pa %lx", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next)
		printf(" -> pmap %p, va %lx, ptste %p, ptpmap %p, flags %x",
		    pv->pv_pmap, pv->pv_va, pv->pv_ptste, pv->pv_ptpmap,
		    pv->pv_flags);
	printf("\n");
}

void
pmap_check_wiring(str, va)
	char *str;
	vm_offset_t va;
{
	vm_map_entry_t entry;
	register int count, *pte;

	va = trunc_page(va);
	if (!pmap_ste_v(pmap_kernel(), va) ||
	    !pmap_pte_v(pmap_pte(pmap_kernel(), va)))
		return;

	if (!vm_map_lookup_entry(pt_map, va, &entry)) {
		printf("wired_check: entry for %lx not found\n", va);
		return;
	}
	count = 0;
	for (pte = (int *)va; pte < (int *)(va+PAGE_SIZE); pte++)
		if (*pte)
			count++;
	if (entry->wired_count != count)
		printf("*%s*: %lx: w%d/a%d\n", str, va, entry->wired_count,
		    count);
}
#endif
