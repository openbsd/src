/*	$OpenBSD: pmap.c,v 1.81 2015/08/11 13:15:36 visa Exp $	*/

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/pool.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#include <sys/atomic.h>

#include <mips64/cache.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/vmparam.h>

#include <uvm/uvm.h>

extern void mem_zero_page(vaddr_t);

struct pool pmap_pmap_pool;
struct pool pmap_pv_pool;
struct pool pmap_pg_pool;

#define pmap_pv_alloc()		(pv_entry_t)pool_get(&pmap_pv_pool, PR_NOWAIT)
#define pmap_pv_free(pv)	pool_put(&pmap_pv_pool, (pv))

#ifndef PMAP_PV_LOWAT
#define PMAP_PV_LOWAT   16
#endif
int	pmap_pv_lowat = PMAP_PV_LOWAT;

uint	 pmap_alloc_tlbpid(struct proc *);
int	 pmap_enter_pv(pmap_t, vaddr_t, vm_page_t, pt_entry_t *);
void	 pmap_remove_pv(pmap_t, vaddr_t, paddr_t);
void	*pmap_pg_alloc(struct pool *, int, int *);
void	 pmap_pg_free(struct pool *, void *);

struct pool_allocator pmap_pg_allocator = {
	pmap_pg_alloc, pmap_pg_free
};

void	pmap_invalidate_user_page(pmap_t, vaddr_t);
#ifdef MULTIPROCESSOR
void	pmap_invalidate_kernel_page(vaddr_t);
void	pmap_invalidate_kernel_page_action(void *);
void	pmap_invalidate_user_page_action(void *);
void	pmap_update_kernel_page_action(void *);
void	pmap_update_user_page_action(void *);
#else
#define pmap_invalidate_kernel_page(va) tlb_flush_addr(va)
#endif

#ifdef PMAPDEBUG
struct {
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
	int cachehit;	/* new entry forced valid entry out */
} enter_stats;
struct {
	int calls;
	int removes;
	int flushes;
	int pidflushes;	/* HW pid stolen */
	int pvfirst;
	int pvsearch;
} remove_stats;

#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_PVENTRY	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_TLBPID	0x0400
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000

#define DPRINTF(flag, printdata)	\
	if (pmapdebug & (flag)) 	\
		printf printdata;

#define stat_count(what)	atomic_inc_int(&(what))
int pmapdebug = PDB_ENTER|PDB_FOLLOW;

#else

#define DPRINTF(flag, printdata)
#define stat_count(what)

#endif	/* PMAPDEBUG */

static struct pmap	kernel_pmap_store
	[(PMAP_SIZEOF(MAXCPUS) + sizeof(struct pmap) - 1)
		/ sizeof(struct pmap)];
struct pmap *const kernel_pmap_ptr = kernel_pmap_store;

vaddr_t	virtual_start;  /* VA of first avail page (after kernel bss)*/
vaddr_t	virtual_end;	/* VA of last avail page (end of kernel AS) */

vaddr_t	pmap_prefer_mask;

static struct pmap_asid_info pmap_asid_info[MAXCPUS];

pt_entry_t	*Sysmap;		/* kernel pte table */
u_int		Sysmapsize;		/* number of pte's in Sysmap */
const vaddr_t	Sysmapbase = VM_MIN_KERNEL_ADDRESS;	/* for libkvm */


#ifdef MULTIPROCESSOR

struct pmap_invalidate_page_arg {
	pmap_t pmap;
	vaddr_t va;
};

void
pmap_invalidate_kernel_page(vaddr_t va)
{
	struct pmap_invalidate_page_arg arg;
	unsigned int cpumask = 0;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) 
		if (cpuset_isset(&cpus_running, ci))
			cpumask |= 1 << ci->ci_cpuid;
	arg.va = va;

	smp_rendezvous_cpus(cpumask, pmap_invalidate_kernel_page_action, &arg);
}

void
pmap_invalidate_kernel_page_action(void *arg)
{
	vaddr_t va = ((struct pmap_invalidate_page_arg *)arg)->va;

	tlb_flush_addr(va);
}

void
pmap_invalidate_user_page(pmap_t pmap, vaddr_t va)
{
	unsigned int cpuid = cpu_number();
	unsigned int cpumask = 0;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) 
		if (cpuset_isset(&cpus_running, ci)) {
			unsigned int i = ci->ci_cpuid;
			unsigned int m = 1 << i;
			if (pmap->pm_asid[i].pma_asidgen !=
			    pmap_asid_info[i].pma_asidgen)
				continue;
			else if (ci->ci_curpmap != pmap) {
				pmap->pm_asid[i].pma_asidgen = 0;
				continue;
			}
			cpumask |= m;
		}

	if (cpumask == 1 << cpuid) {
		u_long asid;

		asid = pmap->pm_asid[cpuid].pma_asid << PG_ASID_SHIFT;
		tlb_flush_addr(va | asid);
	} else if (cpumask) {
		struct pmap_invalidate_page_arg arg;
		arg.pmap = pmap;
		arg.va = va;

		smp_rendezvous_cpus(cpumask, pmap_invalidate_user_page_action,
			&arg);
	}
}

void
pmap_invalidate_user_page_action(void *arg)
{
	pmap_t pmap = ((struct pmap_invalidate_page_arg *)arg)->pmap;
	vaddr_t va = ((struct pmap_invalidate_page_arg *)arg)->va;
	unsigned int cpuid = cpu_number();
	u_long asid;

	asid = pmap->pm_asid[cpuid].pma_asid << PG_ASID_SHIFT;
	tlb_flush_addr(va | asid);
}

struct pmap_update_page_arg {
	pmap_t pmap;
	vaddr_t va;
	pt_entry_t entry;
};

void
pmap_update_kernel_page(vaddr_t va, pt_entry_t entry)
{
	struct pmap_update_page_arg arg;
	unsigned long cpumask = 0;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) 
		if (cpuset_isset(&cpus_running, ci))
			cpumask |= 1 << ci->ci_cpuid;

	arg.va = va;
	arg.entry = entry;
	smp_rendezvous_cpus(cpumask,
			    pmap_update_kernel_page_action, &arg);
}

void
pmap_update_kernel_page_action(void *arg)
{
	vaddr_t va = ((struct pmap_update_page_arg *)arg)->va;
	pt_entry_t entry = ((struct pmap_update_page_arg *)arg)->entry;

	tlb_update(va, entry);
}

void
pmap_update_user_page(pmap_t pmap, vaddr_t va, pt_entry_t entry)
{
	unsigned int cpuid = cpu_number();
	unsigned long cpumask = 0;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) 
		if (cpuset_isset(&cpus_running, ci)) {
			unsigned int i = ci->ci_cpuid;
			unsigned int m = 1 << i;
			if (pmap->pm_asid[i].pma_asidgen != 
			    pmap_asid_info[i].pma_asidgen)
				continue;
			else if (ci->ci_curpmap != pmap) {
				pmap->pm_asid[i].pma_asidgen = 0;
				continue;
			}
			cpumask |= m;
		}

	if (cpumask == 1 << cpuid) {
		u_long asid;

		asid = pmap->pm_asid[cpuid].pma_asid << PG_ASID_SHIFT;
		tlb_update(va | asid, entry);
	} else if (cpumask) {
		struct pmap_update_page_arg arg;
		arg.pmap = pmap;
		arg.va = va;
		arg.entry = entry;
		smp_rendezvous_cpus(cpumask,
				    pmap_update_user_page_action, &arg);
	}
}

void
pmap_update_user_page_action(void *arg)
{
	pmap_t pmap = ((struct pmap_update_page_arg *)arg)->pmap;
	vaddr_t va = ((struct pmap_update_page_arg *)arg)->va;
	pt_entry_t entry = ((struct pmap_update_page_arg *)arg)->entry;
	unsigned int cpuid = cpu_number();
	u_long asid;

	asid = pmap->pm_asid[cpuid].pma_asid << PG_ASID_SHIFT;
	tlb_update(va | asid, entry);
}
#else
void
pmap_invalidate_user_page(pmap_t pmap, vaddr_t va)
{
	u_long cpuid = cpu_number();
	u_long asid = pmap->pm_asid[cpuid].pma_asid << PG_ASID_SHIFT;

	if (pmap->pm_asid[cpuid].pma_asidgen ==
	    pmap_asid_info[cpuid].pma_asidgen) {
#ifdef CPU_R4000
		if (r4000_errata != 0)
			eop_tlb_flush_addr(pmap, va, asid);
		else
#endif
			tlb_flush_addr(va | asid);
	}
}

void
pmap_update_user_page(pmap_t pmap, vaddr_t va, pt_entry_t entry)
{
	u_long cpuid = cpu_number();
	u_long asid = pmap->pm_asid[cpuid].pma_asid << PG_ASID_SHIFT;

	if (pmap->pm_asid[cpuid].pma_asidgen ==
	    pmap_asid_info[cpuid].pma_asidgen)
		tlb_update(va | asid, entry);
}
#endif

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(void)
{
	u_int i;
#ifndef CPU_R8000
	pt_entry_t *spte;
#endif

	/*
	 * Create a mapping table for kernel virtual memory. This
	 * table is a linear table in contrast to the user process
	 * mapping tables which are built with segment/page tables.
	 * Create 1GB of map (this will only use 1MB of memory).
	 */
	virtual_start = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	Sysmapsize = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
	    PAGE_SIZE;
#ifndef CPU_R8000
	if (Sysmapsize & 1)
		Sysmapsize++;	/* force even number of pages */
#endif

	Sysmap = (pt_entry_t *)
	    uvm_pageboot_alloc(sizeof(pt_entry_t) * Sysmapsize);

	pool_init(&pmap_pmap_pool, PMAP_SIZEOF(ncpusfound), 0, 0, 0,"pmappl", NULL);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0,"pvpl", NULL);
	pool_init(&pmap_pg_pool, PMAP_L2SIZE, PMAP_L2SIZE, 0, 0, "pmappgpl",
	    &pmap_pg_allocator);

	pmap_kernel()->pm_count = 1;

#ifndef CPU_R8000
	/*
	 * The 64 bit Mips architecture stores the AND result
	 * of the Global bits in the pte pair in the on chip
	 * translation lookaside buffer. Thus invalid entries
	 * must have the Global bit set so when Entry LO and
	 * Entry HI G bits are ANDed together they will produce
	 * a global bit to store in the tlb.
	 */
	for (i = Sysmapsize, spte = Sysmap; i != 0; i--, spte++)
		*spte = PG_G;
#else
	bzero(Sysmap, sizeof(pt_entry_t) * Sysmapsize);
#endif
	tlb_set_gbase((vaddr_t)Sysmap, Sysmapsize);

	for (i = 0; i < MAXCPUS; i++) {
		pmap_asid_info[i].pma_asidgen = 1;
		pmap_asid_info[i].pma_asid = MIN_USER_ASID + 1;
	}
}

/*
 *  Page steal allocator used during bootup.
 */
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstartp, vaddr_t *vendp)
{
	int i, j;
	uint npg;
	vaddr_t va;
	paddr_t pa;

#ifdef DIAGNOSTIC
	if (uvm.page_init_done) {
		panic("pmap_steal_memory: too late, vm is running!");
	}
#endif

	size = round_page(size);
	npg = atop(size);
	va = 0;

	for(i = 0; i < vm_nphysseg && va == 0; i++) {
		if (vm_physmem[i].avail_start != vm_physmem[i].start ||
		    vm_physmem[i].avail_start >= vm_physmem[i].avail_end) {
			continue;
		}

		if ((vm_physmem[i].avail_end - vm_physmem[i].avail_start) < npg)
			continue;

		pa = ptoa(vm_physmem[i].avail_start);
		vm_physmem[i].avail_start += npg;
		vm_physmem[i].start += npg;

		if (vm_physmem[i].avail_start == vm_physmem[i].end) {
			if (vm_nphysseg == 1)
				panic("pmap_steal_memory: out of memory!");

			vm_nphysseg--;
			for (j = i; j < vm_nphysseg; j++)
				vm_physmem[j] = vm_physmem[j + 1];
		}
		if (vstartp)
			*vstartp = round_page(virtual_start);
		if (vendp)
			*vendp = virtual_end;

#ifdef __sgi__
#ifndef CPU_R8000
		/*
		 * Return a CKSEG0 address whenever possible.
		 */
		if (pa + size < CKSEG_SIZE)
			va = PHYS_TO_CKSEG0(pa);
		else
#endif
			va = PHYS_TO_XKPHYS(pa, CCA_CACHED);
#else
		va = PHYS_TO_XKPHYS(pa, CCA_CACHED);
#endif

		bzero((void *)va, size);
		return (va);
	}

	panic("pmap_steal_memory: no memory to steal");
}

/*
 *	Initialize the pmap module.
 *	Called by uvm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init()
{

	DPRINTF(PDB_FOLLOW|PDB_INIT, ("pmap_init()\n"));

#if 0 /* too early */
	pool_setlowat(&pmap_pv_pool, pmap_pv_lowat);
#endif
}

static pv_entry_t pg_to_pvh(struct vm_page *);
static __inline pv_entry_t
pg_to_pvh(struct vm_page *pg)
{
	return &pg->mdpage.pv_ent;
}

/*
 *	Create and return a physical map.
 */
pmap_t
pmap_create()
{
	pmap_t pmap;
	int i, s;

extern struct vmspace vmspace0;
extern struct user *proc0paddr;

	DPRINTF(PDB_FOLLOW|PDB_CREATE, ("pmap_create()\n"));

	s = splvm();
	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK | PR_ZERO);
	splx(s);

	pmap->pm_count = 1;

	pmap->pm_segtab = (struct segtab *)pool_get(&pmap_pg_pool,
	    PR_WAITOK | PR_ZERO);

	if (pmap == vmspace0.vm_map.pmap) {
		/*
		 * The initial process has already been allocated a TLBPID
		 * in mips_init().
		 */
		for (i = 0; i < ncpusfound; i++) {
			pmap->pm_asid[i].pma_asid = MIN_USER_ASID;
			pmap->pm_asid[i].pma_asidgen =
				pmap_asid_info[i].pma_asidgen;
		}
		proc0paddr->u_pcb.pcb_segtab = pmap->pm_segtab;
	} else {
		for (i = 0; i < ncpusfound; i++) {
			pmap->pm_asid[i].pma_asid = 0;
			pmap->pm_asid[i].pma_asidgen = 0;
		}
	}

	return (pmap);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap_t pmap)
{
	int s, count;

	DPRINTF(PDB_FOLLOW|PDB_CREATE, ("pmap_destroy(%p)\n", pmap));

	count = --pmap->pm_count;
	if (count > 0)
		return;

	if (pmap->pm_segtab) {
		pt_entry_t *pte;
		int i;
#ifdef PARANOIA
		int j;
#endif

		for (i = 0; i < PMAP_SEGTABSIZE; i++) {
			/* get pointer to segment map */
			pte = pmap->pm_segtab->seg_tab[i];
			if (!pte)
				continue;
#ifdef PARANOIA
			for (j = 0; j < NPTEPG; j++) {
				if (pte[j] != PG_NV)
					panic("pmap_destroy: segmap %p not empty at index %d", pte, j);
			}
#endif
			pool_put(&pmap_pg_pool, pte);
#ifdef PARANOIA
			pmap->pm_segtab->seg_tab[i] = NULL;
#endif
		}
		pool_put(&pmap_pg_pool, pmap->pm_segtab);
#ifdef PARANOIA
		pmap->pm_segtab = NULL;
#endif
	}

	s = splvm();
	pool_put(&pmap_pmap_pool, pmap);
	splx(s);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap_t pmap)
{

	DPRINTF(PDB_FOLLOW, ("pmap_reference(%p)\n", pmap));

	pmap->pm_count++;
}

/*
 *      Make a new pmap (vmspace) active for the given process.
 */
void
pmap_activate(struct proc *p)
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	struct cpu_info *ci = curcpu();
	uint id;

	ci->ci_curpmap = pmap;
	p->p_addr->u_pcb.pcb_segtab = pmap->pm_segtab;
	id = pmap_alloc_tlbpid(p);
	if (p == ci->ci_curproc)
		tlb_set_pid(id);
}

/*
 *      Make a previously active pmap (vmspace) inactive.
 */
void
pmap_deactivate(struct proc *p)
{
	struct cpu_info *ci = curcpu();

	ci->ci_curpmap = NULL;
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{
	vaddr_t nssva;
	pt_entry_t *pte, entry;
	paddr_t pa;
	struct cpu_info *ci = curcpu();

	DPRINTF(PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT,
		("pmap_remove(%p, %p, %p)\n", pmap, (void *)sva, (void *)eva));

	stat_count(remove_stats.calls);

	KERNEL_LOCK();
	if (pmap == pmap_kernel()) {
		/* remove entries from kernel pmap */
#ifdef DIAGNOSTIC
		if (sva < VM_MIN_KERNEL_ADDRESS ||
		    eva >= VM_MAX_KERNEL_ADDRESS || eva < sva)
			panic("pmap_remove(%p, %p): not in range",
			    (void *)sva, (void *)eva);
#endif
		pte = kvtopte(sva);
		for(; sva < eva; sva += PAGE_SIZE, pte++) {
			entry = *pte;
			if (!(entry & PG_V))
				continue;
			if (entry & PG_WIRED)
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;
			pa = pfn_to_pad(entry);
			if ((entry & PG_CACHEMODE) == PG_CACHED)
				Mips_HitSyncDCache(ci, sva, PAGE_SIZE);
			pmap_remove_pv(pmap, sva, pa);
			*pte = PG_NV | PG_G;
			/*
			 * Flush the TLB for the given address.
			 */
			pmap_invalidate_kernel_page(sva);
			stat_count(remove_stats.flushes);
		}
		KERNEL_UNLOCK();
		return;
	}

#ifdef DIAGNOSTIC
	if (eva > VM_MAXUSER_ADDRESS)
		panic("pmap_remove: uva not in range");
#endif
	while (sva < eva) {
		nssva = mips_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!(pte = pmap_segmap(pmap, sva))) {
			sva = nssva;
			continue;
		}
		/*
		 * Invalidate every valid mapping within this segment.
		 */
		pte += uvtopte(sva);
		for (; sva < nssva; sva += PAGE_SIZE, pte++) {
			entry = *pte;
			if (!(entry & PG_V))
				continue;
			if (entry & PG_WIRED)
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;
			pa = pfn_to_pad(entry);
			if ((entry & PG_CACHEMODE) == PG_CACHED)
				Mips_SyncDCachePage(ci, sva, pa);
			pmap_remove_pv(pmap, sva, pa);
			*pte = PG_NV;
			/*
			 * Flush the TLB for the given address.
			 */
			pmap_invalidate_user_page(pmap, sva);
			stat_count(remove_stats.flushes);
		}
	}
	KERNEL_UNLOCK();
}

/*
 *	pmap_page_protect:
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	pv_entry_t pv;
	vaddr_t va;
	int s;

	if (prot == PROT_NONE) {
		DPRINTF(PDB_REMOVE, ("pmap_page_protect(%p, 0x%x)\n", pg, prot));
	} else {
		DPRINTF(PDB_FOLLOW|PDB_PROTECT,
			("pmap_page_protect(%p, 0x%x)\n", pg, prot));
	}

	switch (prot) {
	case PROT_READ | PROT_WRITE:
	case PROT_MASK:
		break;

	/* copy_on_write */
	case PROT_READ:
	case PROT_READ | PROT_EXEC:
		pv = pg_to_pvh(pg);
		s = splvm();
		/*
		 * Loop over all current mappings setting/clearing as apropos.
		 */
		if (pv->pv_pmap != NULL) {
			for (; pv; pv = pv->pv_next) {
				va = pv->pv_va;
				pmap_protect(pv->pv_pmap, va, va + PAGE_SIZE,
				    prot);
			}
		}
		splx(s);
		break;

	/* remove_all */
	default:
		pv = pg_to_pvh(pg);
		s = splvm();
		while (pv->pv_pmap != NULL) {
			va = pv->pv_va;
			pmap_remove(pv->pv_pmap, va, va + PAGE_SIZE);
		}
		splx(s);
	}
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	vaddr_t nssva;
	pt_entry_t *pte, entry, p;
	struct cpu_info *ci = curcpu();

	DPRINTF(PDB_FOLLOW|PDB_PROTECT,
		("pmap_protect(%p, %p, %p, 0x%x)\n",
		    pmap, (void *)sva, (void *)eva, prot));

	if ((prot & PROT_READ) == PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	p = (prot & PROT_WRITE) ? PG_M : PG_RO;

	if (pmap == pmap_kernel()) {
		/*
		 * Change entries in kernel pmap.
		 * This will trap if the page is writeable (in order to set
		 * the dirty bit) even if the dirty bit is already set. The
		 * optimization isn't worth the effort since this code isn't
		 * executed much. The common case is to make a user page
		 * read-only.
		 */
#ifdef DIAGNOSTIC
		if (sva < VM_MIN_KERNEL_ADDRESS ||
		    eva >= VM_MAX_KERNEL_ADDRESS || eva < sva)
			panic("pmap_protect(%p, %p): not in range",
			    (void *)sva, (void *)eva);
#endif
		pte = kvtopte(sva);
		for (; sva < eva; sva += PAGE_SIZE, pte++) {
			entry = *pte;
			if (!(entry & PG_V))
				continue;
			if ((entry & PG_M) != 0 /* && p != PG_M */)
				if ((entry & PG_CACHEMODE) == PG_CACHED)
					Mips_HitSyncDCache(ci, sva, PAGE_SIZE);
			entry = (entry & ~(PG_M | PG_RO)) | p;
			*pte = entry;
			/*
			 * Update the TLB if the given address is in the cache.
			 */
			pmap_update_kernel_page(sva, entry);
		}
		return;
	}

#ifdef DIAGNOSTIC
	if (eva > VM_MAXUSER_ADDRESS)
		panic("pmap_protect: uva not in range");
#endif
	while (sva < eva) {
		nssva = mips_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!(pte = pmap_segmap(pmap, sva))) {
			sva = nssva;
			continue;
		}
		/*
		 * Change protection on every valid mapping within this segment.
		 */
		pte += uvtopte(sva);
		for (; sva < nssva; sva += PAGE_SIZE, pte++) {
			entry = *pte;
			if (!(entry & PG_V))
				continue;
			if ((entry & PG_M) != 0 /* && p != PG_M */)
				if ((entry & PG_CACHEMODE) == PG_CACHED)
					Mips_SyncDCachePage(ci, sva,
					    pfn_to_pad(entry));
			entry = (entry & ~(PG_M | PG_RO)) | p;
			*pte = entry;
			pmap_update_user_page(pmap, sva, entry);
		}
	}
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	pt_entry_t *pte, npte;
	vm_page_t pg;
	struct cpu_info *ci = curcpu();
	u_long cpuid = ci->ci_cpuid;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

	DPRINTF(PDB_FOLLOW|PDB_ENTER,
		("pmap_enter(%p, %p, %p, 0x%x, 0x%x)\n",
		    pmap, (void *)va, (void *)pa, prot, flags));

#ifdef DIAGNOSTIC
	if (pmap == pmap_kernel()) {
		stat_count(enter_stats.kernel);
		if (va < VM_MIN_KERNEL_ADDRESS ||
		    va >= VM_MAX_KERNEL_ADDRESS)
			panic("pmap_enter: kva %p", (void *)va);
	} else {
		stat_count(enter_stats.user);
		if (va >= VM_MAXUSER_ADDRESS)
			panic("pmap_enter: uva %p", (void *)va);
	}
#endif

	pg = PHYS_TO_VM_PAGE(pa);

	if (pg != NULL) {
		if (!(prot & PROT_WRITE)) {
			npte = PG_ROPAGE;
		} else {
			if (pmap == pmap_kernel()) {
				/*
				 * Don't bother to trap on kernel writes,
				 * just record page as dirty.
				 */
				npte = PG_RWPAGE;
			} else {
				if (pg->pg_flags & PGF_ATTR_MOD) {
					npte = PG_RWPAGE;
				} else {
					npte = PG_CWPAGE;
				}
			}
		}
		if (flags & PMAP_NOCACHE) {
			npte &= ~PG_CACHED;
			npte |= PG_UNCACHED;
		}

		/* Set page referenced/modified status based on flags */
		if (flags & PROT_WRITE)
			atomic_setbits_int(&pg->pg_flags,
			    PGF_ATTR_MOD | PGF_ATTR_REF);
		else if (flags & PROT_MASK)
			atomic_setbits_int(&pg->pg_flags, PGF_ATTR_REF);

		stat_count(enter_stats.managed);
	} else {
		/*
		 * Assumption: if it is not part of our managed memory
		 * then it must be device memory which may be volatile.
		 */
		stat_count(enter_stats.unmanaged);
		if (prot & PROT_WRITE) {
			npte = PG_IOPAGE & ~PG_G;
		} else {
			npte = (PG_IOPAGE | PG_RO) & ~(PG_G | PG_M);
		}
	}

	if (pmap == pmap_kernel()) {
		if (pg != NULL) {
			if (pmap_enter_pv(pmap, va, pg, &npte) != 0) {
				if (flags & PMAP_CANFAIL)
					return ENOMEM;
				panic("pmap_enter: pmap_enter_pv() failed");
			}
		}

		pte = kvtopte(va);
		if ((*pte & PG_V) && pa != pfn_to_pad(*pte)) {
			pmap_remove(pmap, va, va + PAGE_SIZE);
			stat_count(enter_stats.mchange);
		}
		if ((*pte & PG_V) == 0) {
			pmap->pm_stats.resident_count++;
			if (wired)
				pmap->pm_stats.wired_count++;
		} else {
			if ((*pte & PG_WIRED) != 0 && wired == 0)
				pmap->pm_stats.wired_count--;
			else if ((*pte & PG_WIRED) == 0 && wired != 0)
				pmap->pm_stats.wired_count++;
		}
		npte |= vad_to_pfn(pa) | PG_G;
		if (wired)
			npte |= PG_WIRED;

		/*
		 * Update the same virtual address entry.
		 */
		*pte = npte;
		pmap_update_kernel_page(va, npte);
		return 0;
	}

	/*
	 *  User space mapping. Do table build.
	 */
	if ((pte = pmap_segmap(pmap, va)) == NULL) {
		unsigned int wflags = PR_WAITOK | PR_ZERO;

		if (flags & PMAP_CANFAIL)
			wflags |= PR_LIMITFAIL;
	
		pte = (pt_entry_t *)pool_get(&pmap_pg_pool, wflags);
		if (pte == NULL)
			return ENOMEM;	/* can only happen if PMAP_CANFAIL */

		pmap_segmap(pmap, va) = pte;
	}

	if (pg != NULL) {
		if (pmap_enter_pv(pmap, va, pg, &npte) != 0) {
			if (flags & PMAP_CANFAIL)
				return ENOMEM;
			panic("pmap_enter: pmap_enter_pv() failed");
		}
	}

	pte += uvtopte(va);

	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * MIPS pages in a OpenBSD page.
	 */
	if ((*pte & PG_V) && pa != pfn_to_pad(*pte)) {
		pmap_remove(pmap, va, va + PAGE_SIZE);
		stat_count(enter_stats.mchange);
	}
	if ((*pte & PG_V) == 0) {
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;
	} else {
		if ((*pte & PG_WIRED) != 0 && wired == 0)
			pmap->pm_stats.wired_count--;
		else if ((*pte & PG_WIRED) == 0 && wired != 0)
			pmap->pm_stats.wired_count++;
	}

	npte |= vad_to_pfn(pa);
	if (wired)
		npte |= PG_WIRED;

	if (pmap->pm_asid[cpuid].pma_asidgen == 
	    pmap_asid_info[cpuid].pma_asidgen) {
		DPRINTF(PDB_ENTER, ("pmap_enter: new pte 0x%08x tlbpid %u\n",
			npte, pmap->pm_asid[cpuid].pma_asid));
	} else {
		DPRINTF(PDB_ENTER, ("pmap_enter: new pte 0x%08x\n", npte));
	}

#ifdef CPU_R4000
	/*
	 * If mapping an executable page, check for the R4000 EOP bug, and
	 * flag it in the pte.
	 */
	if (r4000_errata != 0) {
		if (pg != NULL && (prot & PROT_EXEC)) {
			if ((pg->pg_flags & PGF_EOP_CHECKED) == 0)
				atomic_setbits_int(&pg->pg_flags,
				     PGF_EOP_CHECKED |
				     eop_page_check(pa));

			if (pg->pg_flags & PGF_EOP_VULN)
				npte |= PG_SP;
		}
	}
#endif

	*pte = npte;
	pmap_update_user_page(pmap, va, npte);

	/*
	 * If mapping an executable page, invalidate ICache.
	 */
	if (pg != NULL && (prot & PROT_EXEC))
		Mips_InvalidateICache(ci, va, PAGE_SIZE);

	return 0;
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t *pte, npte;

	DPRINTF(PDB_FOLLOW|PDB_ENTER,
		("pmap_kenter_pa(%p, %p, 0x%x)\n", (void *)va, (void *)pa, prot));

#ifdef DIAGNOSTIC
	if (va < VM_MIN_KERNEL_ADDRESS ||
	    va >= VM_MAX_KERNEL_ADDRESS)
		panic("pmap_kenter_pa: kva %p", (void *)va);
#endif

	npte = vad_to_pfn(pa) | PG_G | PG_WIRED;
	if (prot & PROT_WRITE)
		npte |= PG_RWPAGE;
	else
		npte |= PG_ROPAGE;
	pte = kvtopte(va);
	if ((*pte & PG_V) == 0) {
		pmap_kernel()->pm_stats.resident_count++;
		pmap_kernel()->pm_stats.wired_count++;
	} else {
		if ((*pte & PG_WIRED) == 0)
			pmap_kernel()->pm_stats.wired_count++;
	}
	*pte = npte;
	pmap_update_kernel_page(va, npte);
}

/*
 *  Remove a mapping from the kernel map table. When doing this
 *  the cache must be synced for the VA mapped since we mapped
 *  pages behind the back of the VP tracking system. 
 */
void
pmap_kremove(vaddr_t va, vsize_t len)
{
	pt_entry_t *pte, entry;
	vaddr_t eva;
	struct cpu_info *ci = curcpu();

	DPRINTF(PDB_FOLLOW|PDB_REMOVE,
		("pmap_kremove(%p, 0x%lx)\n", (void *)va, len));

	eva = va + len;
#ifdef DIAGNOSTIC
	if (va < VM_MIN_KERNEL_ADDRESS ||
	    eva >= VM_MAX_KERNEL_ADDRESS || eva < va)
		panic("pmap_kremove: va %p len %lx", (void *)va, len);
#endif
	pte = kvtopte(va);
	for (; va < eva; va += PAGE_SIZE, pte++) {
		entry = *pte;
		if (!(entry & PG_V))
			continue;
		if ((entry & PG_CACHEMODE) == PG_CACHED)
			Mips_HitSyncDCache(ci, va, PAGE_SIZE);
		*pte = PG_NV | PG_G;
		pmap_invalidate_kernel_page(va);
		pmap_kernel()->pm_stats.wired_count--;
		pmap_kernel()->pm_stats.resident_count--;
	}
}

void
pmap_unwire(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte;

	if (pmap == pmap_kernel())
		pte = kvtopte(va);
	else {
		if ((pte = pmap_segmap(pmap, va)) == NULL) 
			return;
		pte += uvtopte(va);
	}

	if (*pte & PG_V) {
		if (*pte & PG_WIRED) {
			*pte &= ~PG_WIRED;
			pmap->pm_stats.wired_count--;
		}
	}
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
boolean_t
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
	boolean_t rv = TRUE;
	paddr_t pa;
	pt_entry_t *pte;

	if (pmap == pmap_kernel()) {
		if (IS_XKPHYS(va))
			pa = XKPHYS_TO_PHYS(va);
#ifndef CPU_R8000
		else if (va >= (vaddr_t)CKSEG0_BASE &&
		    va < (vaddr_t)CKSEG0_BASE + CKSEG_SIZE)
			pa = CKSEG0_TO_PHYS(va);
		else if (va >= (vaddr_t)CKSEG1_BASE &&
		    va < (vaddr_t)CKSEG1_BASE + CKSEG_SIZE)
			pa = CKSEG1_TO_PHYS(va);
#endif
		else {
#ifdef DIAGNOSTIC
			if (va < VM_MIN_KERNEL_ADDRESS ||
			    va >= VM_MAX_KERNEL_ADDRESS)
				panic("pmap_extract(%p, %p)", pmap, (void *)va);
#endif
			pte = kvtopte(va);
			if (*pte & PG_V)
				pa = pfn_to_pad(*pte) | (va & PAGE_MASK);
			else
				rv = FALSE;
		}
	} else {
		if (!(pte = pmap_segmap(pmap, va)))
			rv = FALSE;
		else {
			pte += uvtopte(va);
			if (*pte & PG_V)
				pa = pfn_to_pad(*pte) | (va & PAGE_MASK);
			else
				rv = FALSE;
		}
	}
	if (rv != FALSE)
		*pap = pa;

	DPRINTF(PDB_FOLLOW, ("pmap_extract(%p, %p)=%p(%d)",
		pmap, (void *)va, (void *)pa, rv));

	return (rv);
}

/*
 * Find first virtual address >= *vap that
 * will not cause cache aliases.
 */
vaddr_t
pmap_prefer(paddr_t foff, vaddr_t va)
{
	if (pmap_prefer_mask != 0)
		va += (foff - va) & pmap_prefer_mask;

	return va;
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t dst_pmap;
	pmap_t src_pmap;
	vaddr_t dst_addr;
	vsize_t len;
	vaddr_t src_addr;
{

	DPRINTF(PDB_FOLLOW,("pmap_copy(%p, %p, %p, 0x%lx, %p)\n",
	       dst_pmap, src_pmap, (void *)dst_addr, len, (void *)src_addr));
}

/*
 *	pmap_zero_page zeros the specified (machine independent) page.
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t phys = VM_PAGE_TO_PHYS(pg);
	vaddr_t va;
	pv_entry_t pv;
	struct cpu_info *ci = curcpu();
	int df = 0;

	DPRINTF(PDB_FOLLOW, ("pmap_zero_page(%p)\n", (void *)phys));

	va = (vaddr_t)PHYS_TO_XKPHYS(phys, CCA_CACHED);
	pv = pg_to_pvh(pg);
	if (pg->pg_flags & PGF_UNCACHED)
		df = 1;
	else if (pg->pg_flags & PGF_CACHED) {
		df = ((pv->pv_va ^ va) & cache_valias_mask) != 0;
		if (df)
			Mips_SyncDCachePage(ci, pv->pv_va, phys);
	}
	mem_zero_page(va);
	if (df || cache_valias_mask != 0)
		Mips_HitSyncDCache(ci, va, PAGE_SIZE);

#ifdef CPU_R4000
	atomic_clearbits_int(&pg->pg_flags, PGF_EOP_CHECKED | PGF_EOP_VULN);
#endif
}

/*
 *	pmap_copy_page copies the specified (machine independent) page.
 *
 *	We do the copy phys to phys and need to check if there may be
 *	a virtual coherence problem. If so flush the cache for the
 *	areas before copying, and flush afterwards.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src, dst;
	vaddr_t s, d;
	int sf, df;
	pv_entry_t pv;
	struct cpu_info *ci = curcpu();

	sf = df = 0;
	src = VM_PAGE_TO_PHYS(srcpg);
	dst = VM_PAGE_TO_PHYS(dstpg);
	s = (vaddr_t)PHYS_TO_XKPHYS(src, CCA_CACHED);
	d = (vaddr_t)PHYS_TO_XKPHYS(dst, CCA_CACHED);

	DPRINTF(PDB_FOLLOW,
		("pmap_copy_page(%p, %p)\n", (void *)src, (void *)dst));

	pv = pg_to_pvh(srcpg);
	if (srcpg->pg_flags & PGF_UNCACHED)
		sf = 1;
	else if (srcpg->pg_flags & PGF_CACHED) {
		sf = ((pv->pv_va ^ s) & cache_valias_mask) != 0;
		if (sf)
			Mips_SyncDCachePage(ci, pv->pv_va, src);
	}
	pv = pg_to_pvh(dstpg);
	if (dstpg->pg_flags & PGF_UNCACHED)
		df = 1;
	else if (dstpg->pg_flags & PGF_CACHED) {
		df = ((pv->pv_va ^ s) & cache_valias_mask) != 0;
		if (df)
			Mips_SyncDCachePage(ci, pv->pv_va, dst);
	}

	memcpy((void *)d, (void *)s, PAGE_SIZE);

	if (sf)
		Mips_HitInvalidateDCache(ci, s, PAGE_SIZE);
	if (df || cache_valias_mask != 0)
		Mips_HitSyncDCache(ci, d, PAGE_SIZE);

#ifdef CPU_R4000
	atomic_clearbits_int(&dstpg->pg_flags, PGF_EOP_CHECKED | PGF_EOP_VULN);
	atomic_setbits_int(&dstpg->pg_flags,
	    srcpg->pg_flags & (PGF_EOP_CHECKED | PGF_EOP_VULN));
#endif
}

/*
 *  Clear the modify bits on the specified physical page.
 *  Also sync the cache so it reflects the new clean state of the page.
 */
boolean_t
pmap_clear_modify(struct vm_page *pg)
{
	pv_entry_t pv;
	pt_entry_t *pte, entry;
	boolean_t rv = FALSE;
	int s;
	paddr_t pa;
	struct cpu_info *ci = curcpu();

	DPRINTF(PDB_FOLLOW,
		("pmap_clear_modify(%p)\n", (void *)VM_PAGE_TO_PHYS(pg)));

	pa = VM_PAGE_TO_PHYS(pg);
	pv = pg_to_pvh(pg);
	s = splvm();
	if (pg->pg_flags & PGF_ATTR_MOD) {
		atomic_clearbits_int(&pg->pg_flags, PGF_ATTR_MOD);
		rv = TRUE;
	}

	for (; pv != NULL; pv = pv->pv_next) {
		if (pv->pv_pmap == pmap_kernel()) {
#ifdef DIAGNOSTIC
			if (pv->pv_va < VM_MIN_KERNEL_ADDRESS ||
			    pv->pv_va >= VM_MAX_KERNEL_ADDRESS)
				panic("pmap_clear_modify(%p)",
				    (void *)pv->pv_va);
#endif
			pte = kvtopte(pv->pv_va);
			entry = *pte;
			if ((entry & PG_V) != 0 && (entry & PG_M) != 0) {
				if (pg->pg_flags & PGF_CACHED)
					Mips_HitSyncDCache(ci, pv->pv_va,
					    PAGE_SIZE);
				rv = TRUE;
				entry &= ~PG_M;
				*pte = entry;
				pmap_update_kernel_page(pv->pv_va, entry);
			}
		} else if (pv->pv_pmap != NULL) {
			if ((pte = pmap_segmap(pv->pv_pmap, pv->pv_va)) == NULL)
				continue;
			pte += uvtopte(pv->pv_va);
			entry = *pte;
			if ((entry & PG_V) != 0 && (entry & PG_M) != 0) {
				if (pg->pg_flags & PGF_CACHED)
					Mips_SyncDCachePage(ci, pv->pv_va, pa);
				rv = TRUE;
				entry &= ~PG_M;
				*pte = entry;
				pmap_update_user_page(pv->pv_pmap, pv->pv_va, entry);
			}
		}
	}
	splx(s);

	return rv;
}

void
pmap_set_modify(struct vm_page *pg)
{
	atomic_setbits_int(&pg->pg_flags, PGF_ATTR_MOD | PGF_ATTR_REF);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
boolean_t
pmap_clear_reference(struct vm_page *pg)
{
	boolean_t rv;

	DPRINTF(PDB_FOLLOW, ("pmap_clear_reference(%p)\n",
	    (void *)VM_PAGE_TO_PHYS(pg)));

	rv = (pg->pg_flags & PGF_ATTR_REF) != 0;
	atomic_clearbits_int(&pg->pg_flags, PGF_ATTR_REF);
	return rv;
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
boolean_t
pmap_is_referenced(struct vm_page *pg)
{
	return (pg->pg_flags & PGF_ATTR_REF) != 0;
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(struct vm_page *pg)
{
	return (pg->pg_flags & PGF_ATTR_MOD) != 0;
}

/*
 * Miscellaneous support routines not part of the pmap API
 */

/*
 *	Return RO protection of page.
 */
int
pmap_is_page_ro(pmap_t pmap, vaddr_t va, pt_entry_t entry)
{
	return ((entry & PG_RO) != 0);
}


/*
 *  Walk the PV tree for a physical page and change all its
 *  mappings to cached or uncached.
 */
void
pmap_page_cache(vm_page_t pg, u_int mode)
{
	pv_entry_t pv;
	pt_entry_t *pte, entry;
	pt_entry_t newmode;
	int s;

	DPRINTF(PDB_FOLLOW|PDB_ENTER, ("pmap_page_cache(%p)\n", pg));

	newmode = mode & PGF_CACHED ? PG_CACHED : PG_UNCACHED;
	pv = pg_to_pvh(pg);

	s = splvm();
	for (; pv != NULL; pv = pv->pv_next) {
		if (pv->pv_pmap == pmap_kernel()) {
#ifdef DIAGNOSTIC
			if (pv->pv_va < VM_MIN_KERNEL_ADDRESS ||
			    pv->pv_va >= VM_MAX_KERNEL_ADDRESS)
				panic("pmap_page_cache(%p)", (void *)pv->pv_va);
#endif
			pte = kvtopte(pv->pv_va);
			entry = *pte;
			if (entry & PG_V) {
				entry = (entry & ~PG_CACHEMODE) | newmode;
				*pte = entry;
				pmap_update_kernel_page(pv->pv_va, entry);
			}
		} else if (pv->pv_pmap != NULL) {
			if ((pte = pmap_segmap(pv->pv_pmap, pv->pv_va))) {
				pte += uvtopte(pv->pv_va);
				entry = *pte;
				if (entry & PG_V) {
					entry = (entry & ~PG_CACHEMODE) | newmode;
					*pte = entry;
					pmap_update_user_page(pv->pv_pmap, pv->pv_va, entry);
				}
			}
		}
	}
	atomic_clearbits_int(&pg->pg_flags, PGF_CACHED | PGF_UNCACHED);
	atomic_setbits_int(&pg->pg_flags, mode);
	splx(s);
}

/*
 * Allocate a hardware PID and return it.
 * It takes almost as much or more time to search the TLB for a
 * specific PID and flush those entries as it does to flush the entire TLB.
 * Therefore, when we allocate a new PID, we just take the next number. When
 * we run out of numbers, we flush the TLB, increment the generation count
 * and start over. PID zero is reserved for kernel use.
 * This is called only by switch().
 */
uint
pmap_alloc_tlbpid(struct proc *p)
{
	pmap_t pmap;
	uint id;
	struct cpu_info *ci = curcpu();
	u_long cpuid = ci->ci_cpuid;

	pmap = p->p_vmspace->vm_map.pmap;
	if (pmap->pm_asid[cpuid].pma_asidgen != 
	    pmap_asid_info[cpuid].pma_asidgen) {
		id = pmap_asid_info[cpuid].pma_asid;
		if (id >= PG_ASID_COUNT) {
			tlb_asid_wrap(ci);
			/* reserve tlbpid_gen == 0 to alway mean invalid */
			if (++pmap_asid_info[cpuid].pma_asidgen == 0)
				pmap_asid_info[cpuid].pma_asidgen = 1;
			id = MIN_USER_ASID;
		}
		pmap_asid_info[cpuid].pma_asid = id + 1;
		pmap->pm_asid[cpuid].pma_asid = id;
		pmap->pm_asid[cpuid].pma_asidgen = 
			pmap_asid_info[cpuid].pma_asidgen;
	} else {
		id = pmap->pm_asid[cpuid].pma_asid;
	}

	if (curproc) {
		DPRINTF(PDB_FOLLOW|PDB_TLBPID, 
			("pmap_alloc_tlbpid: curproc %d '%s' ",
				curproc->p_pid, curproc->p_comm));
	} else {
		DPRINTF(PDB_FOLLOW|PDB_TLBPID, 
			("pmap_alloc_tlbpid: curproc <none> "));
	}
	DPRINTF(PDB_FOLLOW|PDB_TLBPID, ("segtab %p tlbpid %u pid %d '%s'\n",
			pmap->pm_segtab, id, p->p_pid, p->p_comm));

	return (id);
}

/*
 * Enter the pmap and virtual address into the physical to virtual map table.
 */
int
pmap_enter_pv(pmap_t pmap, vaddr_t va, vm_page_t pg, pt_entry_t *npte)
{
	pv_entry_t pv, npv;
	int s;

	pv = pg_to_pvh(pg);

	s = splvm();
	if (pv->pv_pmap == NULL) {
		/*
		 * No entries yet, use header as the first entry
		 */

		DPRINTF(PDB_PVENTRY,
			("pmap_enter: first pv: pmap %p va %p pa %p\n",
				pmap, (void *)va, (void *)VM_PAGE_TO_PHYS(pg)));

		stat_count(enter_stats.firstpv);

		pv->pv_va = va;
		if (*npte & PG_CACHED)
			atomic_setbits_int(&pg->pg_flags, PGF_CACHED);
		if (*npte & PG_UNCACHED)
			atomic_setbits_int(&pg->pg_flags, PGF_UNCACHED);
		pv->pv_pmap = pmap;
		pv->pv_next = NULL;
	} else {
		/*
		 * There is at least one other VA mapping this page.
		 * We'll place this entry after the header.
		 */

		if ((pg->pg_flags & PGF_CACHED) == 0) {
			/*
			 * If page is not mapped cached it's either because
			 * an uncached mapping was explicitely requested or
			 * we have a VAC situation.
			 * Map this page uncached as well.
			 */
			*npte = (*npte & ~PG_CACHEMODE) | PG_UNCACHED;
		}

		/*
		 * The entry may already be in the list if
		 * we are only changing the protection bits.
		 */
		for (npv = pv; npv; npv = npv->pv_next) {
			if (pmap == npv->pv_pmap && va == npv->pv_va) {
				splx(s);
				return 0;
			}
		}

		DPRINTF(PDB_PVENTRY,
			("pmap_enter: new pv: pmap %p va %p pg %p\n",
			    pmap, (void *)va, (void *)VM_PAGE_TO_PHYS(pg)));

		npv = pmap_pv_alloc();
		if (npv == NULL) {
			splx(s);
			return ENOMEM;
		}

		if ((*npte & PG_CACHED) != 0 &&
		    (pg->pg_flags & PGF_CACHED) != 0 && cache_valias_mask != 0) {
			/*
			 * We have a VAC possibility.  Check if virtual
			 * address of current mappings are compatible
			 * with this new mapping. Only need to check first
			 * since all others have been checked compatible
			 * when added. If they are incompatible, update
			 * all mappings to be mapped uncached, flush the
			 * cache and set page to not be mapped cached.
			 */
			if (((pv->pv_va ^ va) & cache_valias_mask) != 0) {
#ifdef PMAPDEBUG
				printf("%s: uncaching page pa %p, va %p/%p\n",
				    __func__, (void *)VM_PAGE_TO_PHYS(pg),
				    (void *)pv->pv_va, (void *)va);
#endif
				pmap_page_cache(pg, 0);
				Mips_SyncDCachePage(curcpu(), pv->pv_va,
				    VM_PAGE_TO_PHYS(pg));
				*npte = (*npte & ~PG_CACHEMODE) | PG_UNCACHED;
			}
		}

		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;

		if (!npv->pv_next)
			stat_count(enter_stats.secondpv);
	}

	splx(s);
	return 0;
}

/*
 * Remove a physical to virtual address translation from the PV table.
 */
void
pmap_remove_pv(pmap_t pmap, vaddr_t va, paddr_t pa)
{
	pv_entry_t pv, npv;
	vm_page_t pg;
	int s;

	DPRINTF(PDB_FOLLOW|PDB_PVENTRY,
		("pmap_remove_pv(%p, %p, %p)\n", pmap, (void *)va, (void *)pa));

	/*
	 * Remove page from the PV table
	 */
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		return;

	pv = pg_to_pvh(pg);
	s = splvm();
	/*
	 * If we are removing the first entry on the list, copy up
	 * the next entry, if any, and free that pv item since the
	 * first root item can't be freed. Else walk the list.
	 */
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			pmap_pv_free(npv);
		} else {
			pv->pv_pmap = NULL;
			atomic_clearbits_int(&pg->pg_flags,
			    PG_PMAPMASK & ~PGF_PRESERVE);
		}
		stat_count(remove_stats.pvfirst);
	} else {
		for (npv = pv->pv_next; npv; pv = npv, npv = npv->pv_next) {
			stat_count(remove_stats.pvsearch);
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				break;
		}
		if (npv != NULL) {
			pv->pv_next = npv->pv_next;
			pmap_pv_free(npv);
		} else {
#ifdef DIAGNOSTIC
			panic("pmap_remove_pv(%p, %p, %p) not found",
			    pmap, (void *)va, (void *)pa);
#endif
		}
	}

	if ((pg->pg_flags & (PGF_CACHED | PGF_UNCACHED)) == 0 &&
	    cache_valias_mask != 0 && pv->pv_pmap != NULL) {
		/*
		 * If this page had been mapped uncached due to aliasing,
		 * check if it can be mapped cached again after the current
		 * entry's removal.
		 */
		pv = pg_to_pvh(pg);
		va = pv->pv_va;
		for (pv = pv->pv_next; pv != NULL; pv = pv->pv_next) {
			if (((pv->pv_va ^ va) & cache_valias_mask) != 0)
				break;
		}

		if (pv == NULL) {
#ifdef PMAPDEBUG
			printf("%s: caching page pa %p, va %p again\n",
			    __func__, (void *)VM_PAGE_TO_PHYS(pg), (void *)va);
#endif
			pmap_page_cache(pg, PGF_CACHED);
		}
	}

	splx(s);
}

/*
 * Allocator for smaller-than-a-page structures pool (pm_segtab, and
 * second level page tables).  Pages backing this poll are mapped in
 * XKPHYS to avoid additional page faults when servicing a TLB miss.
 */

void *
pmap_pg_alloc(struct pool *pp, int flags, int *slowdown)
{
	vm_page_t pg;

	*slowdown = 0;
	for (;;) {
		pg = uvm_pagealloc(NULL, 0, NULL,
		    UVM_PGA_USERESERVE | UVM_PGA_ZERO);
		if (pg != NULL)
			break;

		*slowdown = 1;
		if (flags & PR_WAITOK)
			uvm_wait(__func__);
		else
			break;
	}

	if (pg != NULL)
		return (void *)PHYS_TO_XKPHYS(VM_PAGE_TO_PHYS(pg), CCA_CACHED);
	else
		return NULL;
}

void
pmap_pg_free(struct pool *pp, void *item)
{
	vaddr_t va = (vaddr_t)item;
	paddr_t pa = XKPHYS_TO_PHYS(va);
	vm_page_t pg = PHYS_TO_VM_PAGE(pa);

	if (cache_valias_mask)
		Mips_HitSyncDCache(curcpu(), va, PAGE_SIZE);
	uvm_pagefree(pg);
}

void
pmap_proc_iflush(struct proc *p, vaddr_t va, vsize_t len)
{
#ifdef MULTIPROCESSOR
	struct pmap *pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	CPU_INFO_FOREACH(cii, ci) {
		if (ci->ci_curpmap == pmap) {
			Mips_InvalidateICache(ci, va, len);
			break;
		}
	}
#else
	Mips_InvalidateICache(curcpu(), va, len);
#endif
}

#ifdef __HAVE_PMAP_DIRECT
vaddr_t
pmap_map_direct(vm_page_t pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	vaddr_t va;

#ifdef __sgi__
#ifndef CPU_R8000
	/*
	 * Return a CKSEG0 address whenever possible.
	 */
	if (pa < CKSEG_SIZE)
		va = PHYS_TO_CKSEG0(pa);
	else
#endif
		va = PHYS_TO_XKPHYS(pa, CCA_CACHED);
#else
	va = PHYS_TO_XKPHYS(pa, CCA_CACHED);
#endif

	return va;
}

vm_page_t
pmap_unmap_direct(vaddr_t va)
{
	paddr_t pa;
	vm_page_t pg;

#ifdef __sgi__
#ifndef CPU_R8000
	if (va >= CKSEG0_BASE)
		pa = CKSEG0_TO_PHYS(va);
	else
#endif
		pa = XKPHYS_TO_PHYS(va);
#else
	pa = XKPHYS_TO_PHYS(va);
#endif

	pg = PHYS_TO_VM_PAGE(pa);
	if (cache_valias_mask)
		Mips_HitSyncDCache(curcpu(), va, PAGE_SIZE);

	return pg;
}
#endif

void
pmap_update(struct pmap *pmap)
{
	Mips_SyncICache(curcpu());
}
