/*	$NetBSD: pmap.c,v 1.7 1995/11/30 00:57:39 jtc Exp $	*/

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
 *	ATARI physical map management code.
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
#define	pmap_ste(m, v)	(&((m)->pm_stab[(vm_offset_t)(v) >> pmap_ishift]))
#define pmap_pte(m, v)	(&((m)->pm_ptab[(vm_offset_t)(v) >> PG_SHIFT]))

#define pmap_pte_pa(pte)	(*(u_int *)(pte) & PG_FRAME)

#define pmap_ste_v(ste)		(*(u_int *)(ste) & SG_V)
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
 */
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
u_int		*Sysseg1;		/* root segment for 68040 */
u_int		*Sysseg;
u_int		*Sysmap, *Sysptmap;
u_int		*Segtabzero;
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
static int	pmap_ishift;	/* segment table index shift */

#ifdef MACHINE_NONCONTIG
static	vm_offset_t	avail_next;
static	vm_size_t	avail_remaining;
#endif

boolean_t		pmap_testbit __P((vm_offset_t, int));
void			pmap_enter_ptpage __P((pmap_t, vm_offset_t));
extern vm_offset_t	reserve_dumppages __P((vm_offset_t));

#ifdef MACHINE_NONCONTIG
#define pmap_valid_page(pa)	(pmap_initialized && pmap_isvalidphys(pa))
#else
#define pmap_valid_page(pa)	(pmap_initialized && pa >= vm_first_phys && \
				pa < vm_last_phys)
#endif

/*
 * All those kernel PT submaps that BSD is so fond of
 */
caddr_t	CADDR1, CADDR2;
u_int	*CMAP1, *CMAP2, *vmpte, *msgbufmap;

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
pmap_bootstrap(kernel_size)
vm_offset_t kernel_size;
{
	vm_offset_t	va;
	u_int		*pte;
	int		i;

	/*
	 * Setup physical address ranges
	 */
	for (i = 0; phys_segs[i+1].start; i++)
		;
	/* XXX: allow for msgbuf */
	phys_segs[i].end -= atari_round_page(sizeof(struct msgbuf));

	avail_start = phys_segs[0].start;
	avail_end   = phys_segs[i].end;

#ifdef MACHINE_NONCONTIG
	/*
	 * Setup var's for pmap_next_page()
	 */
	for (i = 0, avail_remaining = 0; phys_segs[i].start; i++)
		avail_remaining += phys_segs[i].end - phys_segs[i].start;

	avail_remaining >>= PGSHIFT;
	avail_next        = phys_segs[0].start;
#endif

	virtual_avail = VM_MIN_KERNEL_ADDRESS + kernel_size;
	virtual_end   = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 */
	atari_protection_init();

	/*
	 * Kernel page/segment table allocated in locore,
	 * just initialize pointers.
	 */
	pmap_kernel()->pm_stab = Sysseg;
	pmap_kernel()->pm_ptab = Sysmap;
#ifdef M68040
	if (cpu040) {
		pmap_kernel()->pm_rtab = Sysseg1;
		pmap_ishift = SG4_SHIFT2;
	} else
#endif
		pmap_ishift = SG_ISHIFT;

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
		panic("pmap_bootstrap_alloc: called after startup initialized");
	size = round_page(size);
	val  = virtual_avail;

	virtual_avail = pmap_map(virtual_avail, avail_start,
		avail_start + size, VM_PROT_READ|VM_PROT_WRITE);
	avail_start += size;

	bzero((caddr_t) val, size);
	return((void *) val);
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
	vm_offset_t	addr, addr2;
	vm_size_t	npg, s;
	int		rv;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
#ifdef MACHINE_NONCONTIG
		printf("pmap_init(%x, %x)\n", avail_start, avail_end);
#else
		printf("pmap_init(%x, %x)\n", phys_start, phys_end);
#endif
#endif
	addr = (vm_offset_t) Sysmap;
	vm_object_reference(kernel_object);
	(void) vm_map_find(kernel_map, kernel_object, addr, &addr,
	    ATARI_KPTSIZE, FALSE);
	/*
	 * If this fails it is probably because the static portion of
	 * the kernel page table isn't big enough and we overran the
	 * page table map.   Need to adjust pmap_size() in atari_init.c.
	 */
	if (addr != (vm_offset_t)Sysmap)
		panic("pmap_init: bogons in the VM system!\n");
#ifdef DEBUG
	if (pmapdebug & PDB_INIT) {
		printf("pmap_init: Sysseg %x, Sysmap %x, Sysptmap %x\n",
		       Sysseg, Sysmap, Sysptmap);
		printf("  pstart %x, pend %x, vstart %x, vend %x\n",
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
	printf ("pmap_init: avail_start %08x phys_segs[0].start %08x npg %d\n",
		avail_start, phys_segs[0].start, npg);
#endif
#else
	npg = atop(phys_end - phys_start);
#endif
#ifdef M68040
	if (cpu040)
		s = (vm_size_t)ATARI_040STSIZE * 128 + 
		    sizeof(struct pv_entry) * npg + npg;
	else 
#endif
		s = (vm_size_t)ATARI_STSIZE +
		    sizeof(struct pv_entry) * npg + npg;

	s = round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	Segtabzero = (u_int *) addr;
#ifdef M68040
	if (cpu040)
		addr += ATARI_040STSIZE * 128;
	else
#endif
		addr += ATARI_STSIZE;
	pv_table = (pv_entry_t) addr;
	addr += sizeof(struct pv_entry) * npg;
	pmap_attributes = (char *) addr;
#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: %x bytes (%x pgs): seg %x tbl %x attr %x\n",
		       s, npg, Segtabzero, pv_table, pmap_attributes);
#endif

	/*
	 * Allocate physical memory for kernel PT pages and their management.
	 * we need enough pages to map the page tables for each process 
	 * plus some slop.
	 */
	npg = howmany(((maxproc + 16) * ATARI_UPTSIZE / NPTEPG), NBPG);
	npg = min(atop(ATARI_MAX_KPTSIZE), npg);
	s = ptoa(npg) + round_page(npg * sizeof(struct kpt_page));

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
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	s = ptoa(npg);
	addr2 = addr + s;
	kpt_pages = &((struct kpt_page *)addr2)[npg];
	kpt_free_list = (struct kpt_page *) 0;
	do {
		addr2 -= NBPG;
		(--kpt_pages)->kpt_next = kpt_free_list;
		kpt_free_list = kpt_pages;
		kpt_pages->kpt_va = addr2;
		kpt_pages->kpt_pa = pmap_extract(pmap_kernel(), addr2);
	} while (addr != addr2);
#ifdef DEBUG
	kpt_stats.kpttotal = atop(s);
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: KPT: %d pages from %x to %x\n",
		       atop(s), addr, addr + s);
#endif

	/*
	 * Slightly modified version of kmem_suballoc() to get page table
	 * map where we want it.
	 */
	addr = ATARI_UPTBASE;
	s = min(ATARI_UPTMAXSIZE, maxproc * ATARI_UPTSIZE);
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
		printf("pmap_init: pt_map [%x - %x)\n", addr, addr2);
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

	return avail_remaining;
}

int
pmap_next_page(addrp)
	vm_offset_t *addrp;
{
	static int cur_seg = 0;

#ifdef DEBUG
	if(!cur_seg && (avail_next == phys_segs[cur_seg].start))
		printf ("pmap_next_page: next %08x remain %d\n",
		    avail_next, avail_remaining);
#endif
	if (phys_segs[cur_seg].start == 0)
		return FALSE;
	if (avail_next == phys_segs[cur_seg].end) {
		avail_next = phys_segs[++cur_seg].start;
#ifdef DEBUG
		printf ("pmap_next_page: next %08x remain %d\n",
		    avail_next, avail_remaining);
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
			return (atari_btop(pa - sep->start) + sep->first_page);
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
		printf("pmap_map(%x, %x, %x, %x)\n", virt, start, end, prot);
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
		printf("pmap_create(%x)\n", size);
#endif
	/*
	 * Software use map does not need a pmap
	 */
	if (size)
		return(NULL);

	/* XXX: is it ok to wait here? */
	pmap = (pmap_t) malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
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
		printf("pmap_pinit(%x)\n", pmap);
#endif
	/*
	 * No need to allocate page table space yet but we do need a
	 * valid segment table.  Initially, we point everyone at the
	 * "null" segment table.  On the first pmap_enter, a real
	 * segment table will be allocated.
	 */
#ifdef M68040
	if (cpu040)
		pmap->pm_rtab = Segtabzero;
#endif
	pmap->pm_stab = Segtabzero;
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
		printf("pmap_destroy(%x)\n", pmap);
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
		printf("pmap_release(%x)\n", pmap);
#endif
#ifdef notdef /* DIAGNOSTIC */
	/* count would be 0 from pmap_destroy... */
	simple_lock(&pmap->pm_lock);
	if (pmap->pm_count != 1)
		panic("pmap_release count");
#endif
	if (pmap->pm_ptab)
		kmem_free_wakeup(pt_map, (vm_offset_t)pmap->pm_ptab,
				 ATARI_UPTSIZE);
	if (pmap->pm_stab != Segtabzero)
#ifdef M68040
		if (cpu040) {
			kmem_free(kernel_map, (vm_offset_t)pmap->pm_rtab,
			    ATARI_040RTSIZE);
			kmem_free(kernel_map, (vm_offset_t)pmap->pm_stab,
			    ATARI_040STSIZE*128);
		}
		else
#endif
			kmem_free(kernel_map, (vm_offset_t)pmap->pm_stab, ATARI_STSIZE);
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
		printf("pmap_reference(%x)\n", pmap);
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
	int *ste, i, s, bits;
	boolean_t flushcache = FALSE;
#ifdef DEBUG
	u_int opte;

	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%x, %x, %x)\n", pmap, sva, eva);
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
		if (!pmap_ste_v(pmap_ste(pmap, va))) {
			/* XXX: avoid address wrap around */
			if (va >= atari_trunc_seg((vm_offset_t)-1))
				break;
			va = atari_round_seg(va + PAGE_SIZE) - PAGE_SIZE;
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
			printf("remove: invalidating %x\n", pte);
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
				free((caddr_t)npv, M_VMPVENT);
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
printf ("pmap_remove: PA %08x index %d\n", pa, pa_index(pa));
				panic("pmap_remove: PA not in pv_tab");
			}
#endif
			ste = (int *)npv->pv_ptste;
			ptpmap = npv->pv_ptpmap;
			pv->pv_next = npv->pv_next;
			free((caddr_t)npv, M_VMPVENT);
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
				printf("remove: ste was %x@%x pte was %x@%x\n",
				       *ste, ste,
				       *(int *)&opte, pmap_pte(pmap, va));
			}
#endif
#ifdef M68040
			if (cpu040) {
			/*
			 * On the 68040, the PT page contains 64 page tables,
			 * so we need to remove all the associated segment
			 * table entries
			 * (This may be incorrect:  if a single page table is
			 * being removed, the whole page should not be
			 * removed.)
			 */
				for (i = 0; i < 64; ++i)
					*ste++ = SG_NV;
				ste -= 64;
#ifdef DEBUG
				if (pmapdebug &(PDB_REMOVE|PDB_SEGTAB|0x10000))
					printf("pmap_remove:PT at %x remved\n",
					    va);
#endif
			}
			else
#endif /* M68040 */
				*ste = SG_NV;
			/*
			 * If it was a user PT page, we decrement the
			 * reference count on the segment table as well,
			 * freeing it if it is now empty.
			 */
			if (ptpmap != pmap_kernel()) {
#ifdef DEBUG
				if (pmapdebug & (PDB_REMOVE|PDB_SEGTAB))
					printf("remove: stab %x, refcnt %d\n",
					       ptpmap->pm_stab,
					       ptpmap->pm_sref - 1);
				if ((pmapdebug & PDB_PARANOIA) &&
				    ptpmap->pm_stab != (u_int *)trunc_page(ste))
					panic("remove: bogus ste");
#endif
				if (--(ptpmap->pm_sref) == 0) {
#ifdef DEBUG
					if (pmapdebug&(PDB_REMOVE|PDB_SEGTAB))
					printf("remove: free stab %x\n",
					       ptpmap->pm_stab);
#endif
#ifdef M68040
					if (cpu040) {
						kmem_free(kernel_map,
							(vm_offset_t)ptpmap->pm_rtab,
							ATARI_040RTSIZE);
						kmem_free(kernel_map,
							(vm_offset_t)ptpmap->pm_stab,
							ATARI_040STSIZE*128);
						ptpmap->pm_rtab = Segtabzero;
					}
					else
#endif
						kmem_free(kernel_map,
							  (vm_offset_t)ptpmap->pm_stab,
							  ATARI_STSIZE);
					ptpmap->pm_stab = Segtabzero;
					ptpmap->pm_stchanged = TRUE;
					/*
					 * XXX may have changed segment table
					 * pointer for current process so
					 * update now to reload hardware.
					 */
					if (ptpmap == curproc->p_vmspace->vm_map.pmap)
						PMAP_ACTIVATE(ptpmap,
							(struct pcb *)curproc->p_addr, 1);
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
	    prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE))
		printf("pmap_page_protect(%x, %x)\n", pa, prot);
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
			if (!pmap_ste_v(pmap_ste(pv->pv_pmap,pv->pv_va)) ||
			    pmap_pte_pa(pmap_pte(pv->pv_pmap,pv->pv_va)) != pa)
{
  printf ("pmap_page_protect: va %08x, pmap_ste_v %d pmap_pte_pa %08x/%08x\n",
    pv->pv_va, pmap_ste_v(pmap_ste(pv->pv_pmap,pv->pv_va)),
    pmap_pte_pa(pmap_pte(pv->pv_pmap,pv->pv_va)),pa);
  printf (" pvh %08x pv %08x pv_next %08x\n", pa_to_pvh(pa), pv, pv->pv_next);
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
		printf("pmap_protect(%x, %x, %x, %x)\n", pmap, sva, eva, prot);
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
		if (!pmap_ste_v(pmap_ste(pmap, va))) {
			/* XXX: avoid address wrap around */
			if (va >= atari_trunc_seg((vm_offset_t)-1))
				break;
			va = atari_round_seg(va + PAGE_SIZE) - PAGE_SIZE;
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
#ifdef M68040
		/*
		 * Clear caches if making RO (see section
		 * "7.3 Cache Coherency" in the manual).
		 */
		if (isro && cpu040) {
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
		printf("pmap_enter(%x, %x, %x, %x, %x)\n",
		       pmap, va, pa, prot, wired);
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
			kmem_alloc_wait(pt_map, ATARI_UPTSIZE);

	/*
	 * Segment table entry not valid, we need a new PT page
	 */
	if (!pmap_ste_v(pmap_ste(pmap, va)))
		pmap_enter_ptpage(pmap, va);

	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: pte %x, *pte %x\n", pte, *(int *)pte);
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
		if (wired && !pmap_pte_w(pte) || !wired && pmap_pte_w(pte)) {
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
			printf("enter: removing old mapping %x\n", va);
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
			printf("enter: pv at %x: %x/%x/%x\n",
			       pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
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
			npv = (pv_entry_t)
				malloc(sizeof *npv, M_VMPVENT, M_NOWAIT);
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
	 * ATARI pages in a MACH page.
	 */
#ifdef M68040
	if (cpu040 && pmap == pmap_kernel() && va >= ATARI_UPTBASE && 
	    va < (ATARI_UPTBASE + ATARI_UPTMAXSIZE))
		cacheable = FALSE;	/* don't cache user page tables */
#endif
	npte = (pa & PG_FRAME) | pte_prot(pmap, prot) | PG_V;
	npte |= (*(int *)pte & (PG_M|PG_U));
	if (wired)
		npte |= PG_W;
	if (!checkpv && !cacheable)
		npte |= PG_CI;
#ifdef M68040
	else if (cpu040 && (npte & PG_PROT) == PG_RW)
		npte |= PG_CCB;		/* cache copyback */
#endif
	/*
	 * Remember if this was a wiring-only change.
	 * If so, we need not flush the TLB and caches.
	 */
	wired = ((*(int *)pte ^ npte) == PG_W);
#ifdef M68040
	if (cpu040 && !wired) {
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
		printf("pmap_change_wiring(%x, %x, %x)\n", pmap, va, wired);
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
	if (!pmap_ste_v(pmap_ste(pmap, va))) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid STE for %x\n", va);
		return;
	}
	/*
	 * Page not valid.  Should this ever happen?
	 * Just continue and change wiring anyway.
	 */
	if (!pmap_pte_v(pte)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid PTE for %x\n", va);
	}
#endif
	if (wired && !pmap_pte_w(pte) || !wired && pmap_pte_w(pte)) {
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
		printf("pmap_extract(%x, %x) -> ", pmap, va);
#endif
	pa = 0;
	if (pmap && pmap_ste_v(pmap_ste(pmap, va)))
		pa = *(int *)pmap_pte(pmap, va);
	if (pa)
		pa = (pa & PG_FRAME) | (va & ~PG_FRAME);
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("%x\n", pa);
#endif
	return(pa);
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
		printf("pmap_copy(%x, %x, %x, %x, %x)\n",
		       dst_pmap, src_pmap, dst_addr, len, src_addr);
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
	int opmapdebug;
#endif
	if (pmap != pmap_kernel())
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_collect(%x)\n", pmap);
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
		if (pv->pv_pmap != pmap_kernel() || !(pv->pv_flags & PV_PTPAGE))
			continue;
		do {
			if (pv->pv_ptste && pv->pv_ptpmap == pmap_kernel())
				break;
		} while (pv = pv->pv_next);
		if (pv == NULL)
			continue;
#ifdef DEBUG
		if (pv->pv_va < (vm_offset_t)Sysmap ||
		    pv->pv_va >= (vm_offset_t)Sysmap + ATARI_KPTSIZE)
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
			printf("collect: freeing KPT page at %x (ste %x@%x)\n",
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
			printf("collect: %x (%x) to free list\n",
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
			printf("collect: kernel STE at %x still valid (%x)\n",
			       ste, *ste);
		ste = (int *)&Sysptmap[(u_int *)ste-pmap_ste(pmap_kernel(), 0)];
		if (*ste)
			printf("collect: kernel PTmap at %x still valid (%x)\n",
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
		printf("pmap_activate(%x, %x)\n", pmap, pcbp);
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
		printf("pmap_zero_page(%x)\n", phys);
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
		printf("pmap_copy_page(%x, %x)\n", src, dst);
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
		printf("pmap_pageable(%x, %x, %x, %x)\n",
		       pmap, sva, eva, pageable);
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
			printf("pmap_pageable(%x, %x, %x, %x)\n",
			       pmap, sva, eva, pageable);
#endif
		if (!pmap_ste_v(pmap_ste(pmap, sva)))
			return;
		pa = pmap_pte_pa(pmap_pte(pmap, sva));
		if (!pmap_valid_page(pa))
			return;
		pv = pa_to_pvh(pa);
		if (pv->pv_ptste == NULL)
			return;
#ifdef DEBUG
		if (pv->pv_va != sva || pv->pv_next) {
			printf("pmap_pageable: bad PT page va %x next %x\n",
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
			printf("pmap_pageable: PT page %x(%x) unmodified\n",
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
		printf("pmap_clear_modify(%x)\n", pa);
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
		printf("pmap_clear_reference(%x)\n", pa);
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
		printf("pmap_is_referenced(%x) -> %c\n", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_U));
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
		printf("pmap_is_modified(%x) -> %c\n", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_M));
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return(atari_ptob(ppn));
}

/*
 * Miscellaneous support routines follow
 */

/* static */
atari_protection_init()
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
		return(FALSE);

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Check saved info first
	 */
	if (pmap_attributes[pa_index(pa)] & bit) {
		splx(s);
		return(TRUE);
	}
	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			pte = (int *) pmap_pte(pv->pv_pmap, pv->pv_va);
			if (*pte & bit) {
				splx(s);
				return(TRUE);
			}
		}
	}
	splx(s);
	return(FALSE);
}

/* static */
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
		printf("pmap_changebit(%x, %x, %s)\n",
		       pa, bit, setem ? "set" : "clear");
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

		pte = (int *) pmap_pte(pv->pv_pmap, va);
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
#ifdef M68040
			if (firstpage && cpu040 &&
			    (bit == PG_RO && setem || (bit & PG_CMASK))) {
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
	u_int *ste;
	int s;
	u_int sg_proto, *sg;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER|PDB_PTPAGE))
		printf("pmap_enter_ptpage: pmap %x, va %x\n", pmap, va);
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
#ifdef M68040
		if (cpu040) {
			pmap->pm_rtab = (u_int *)
				kmem_alloc(kernel_map, ATARI_040RTSIZE);
			pmap->pm_stab = (u_int *)
				kmem_alloc(kernel_map, ATARI_040STSIZE*128);
			/* intialize root table entries */
			sg = (u_int *) pmap->pm_rtab;
			sg_proto = pmap_extract(pmap_kernel(), 
			    (vm_offset_t) pmap->pm_stab) | SG_RW | SG_V;
#ifdef DEBUG
			if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
				printf ("pmap_enter_ptpage: ROOT TABLE SETUP %x %x\n",
				    pmap->pm_rtab, sg_proto);
#endif
			while (sg < (u_int *) ((u_int) pmap->pm_rtab + ATARI_040RTSIZE)) {
				*sg++ = sg_proto;
				sg_proto += ATARI_040STSIZE;
			}
		}
		else
#endif
			pmap->pm_stab = (u_int *)
				kmem_alloc(kernel_map, ATARI_STSIZE);
		pmap->pm_stchanged = TRUE;
		/*
		 * XXX may have changed segment table pointer for current
		 * process so update now to reload hardware.
		 */
		if (pmap == curproc->p_vmspace->vm_map.pmap)
			PMAP_ACTIVATE(pmap, (struct pcb *)curproc->p_addr, 1);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter: pmap %x stab %x\n",
			       pmap, pmap->pm_stab);
#endif
	}

	/*
	 * On the 68040, a page will hold 64 page tables, so the segment
	 * table will have to have 64 entries set up.  First get the ste
	 * for the page mapped by the first PT entry.
	 */
#ifdef M68040
	if (cpu040) {
		ste = pmap_ste(pmap, va & ((SG4_MASK1 | SG4_MASK2) << 6));
		va = trunc_page((vm_offset_t)pmap_pte(pmap,
		    va & ((SG4_MASK1|SG4_MASK2) << 6)));
	} else
#endif
	{
		ste = pmap_ste(pmap, va);
		va = trunc_page((vm_offset_t)pmap_pte(pmap, va));
	}

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
				printf("enter: no KPT pages, collecting...\n");
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
		bzero(kpt->kpt_va, NBPG);
		pmap_enter(pmap, va, ptpa, VM_PROT_DEFAULT, TRUE);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
			printf("enter: add &Sysptmap[%d]: %x (KPT page %x)\n",
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
			printf("enter: about to fault UPT pg at %x\n", va);
#endif
		if (vm_fault(pt_map, va, VM_PROT_READ|VM_PROT_WRITE, FALSE)
		    != KERN_SUCCESS)
			panic("pmap_enter: vm_fault failed");
		ptpa = pmap_extract(pmap_kernel(), va);
#ifdef DEBUG
		PHYS_TO_VM_PAGE(ptpa)->flags |=  PG_PTPAGE;
#endif
	}

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
		} while (pv = pv->pv_next);
	}
#ifdef DEBUG
	if (pv == NULL)
		panic("pmap_enter_ptpage: PT page not entered");
#endif
	pv->pv_ptste = ste;
	pv->pv_ptpmap = pmap;
#ifdef DEBUG
	if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
		printf("enter: new PT page at PA %x, ste at %x\n", ptpa, ste);
#endif

	/*
	 * Map the new PT page into the segment table.
	 * Also increment the reference count on the segment table if this
	 * was a user page table page.  Note that we don't use vm_map_pageable
	 * to keep the count like we do for PT pages, this is mostly because
	 * it would be difficult to identify ST pages in pmap_pageable to
	 * release them.  We also avoid the overhead of vm_map_pageable.
	 */
#ifdef M68040
	if (cpu040) {
		/* 68040 has 64 page tables, so we have to map all 64 */
		sg = (u_int *) ste;
		sg_proto = (ptpa & SG_FRAME) | SG_RW | SG_V;
		while (sg < (u_int *) (ste + 64)) {
			*sg++ = sg_proto;
			sg_proto += ATARI_040PTSIZE;
		}
	}
	else
#endif
		*(int *)ste = (ptpa & SG_FRAME) | SG_RW | SG_V;
	if (pmap != pmap_kernel()) {
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter: stab %x refcnt %d\n",
			       pmap->pm_stab, pmap->pm_sref);
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

int
pmap_isvalidphys(pa)
vm_offset_t pa;
{
    struct physeg *sep = &phys_segs[0];

    while (sep->start) {
	if (pa >= sep->start && pa < sep->end)
		return(TRUE);
	++sep;
    }
    return(FALSE);
}

#ifdef DEBUG
pmap_pvdump(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv;

	printf("pa %x", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next)
		printf(" -> pmap %x, va %x, ptste %x, ptpmap %x, flags %x",
		       pv->pv_pmap, pv->pv_va, pv->pv_ptste, pv->pv_ptpmap,
		       pv->pv_flags);
	printf("\n");
}

pmap_check_wiring(str, va)
	char *str;
	vm_offset_t va;
{
	vm_map_entry_t entry;
	register int count, *pte;

	va = trunc_page(va);
	if (!pmap_ste_v(pmap_ste(pmap_kernel(), va)) ||
	    !pmap_pte_v(pmap_pte(pmap_kernel(), va)))
		return;

	if (!vm_map_lookup_entry(pt_map, va, &entry)) {
		printf("wired_check: entry for %x not found\n", va);
		return;
	}
	count = 0;
	for (pte = (int *)va; pte < (int *)(va+PAGE_SIZE); pte++)
		if (*pte)
			count++;
	if (entry->wired_count != count)
		printf("*%s*: %x: w%d/a%d\n",
		       str, va, entry->wired_count, count);
}
#endif
