/*	$OpenBSD: pmap.c,v 1.64 2011/10/09 17:01:34 miod Exp $	*/

/*
 * Copyright (c) 2001-2004, 2010, Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pool.h>

#include <uvm/uvm.h>

#include <machine/asm_macro.h>
#include <machine/mmu.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/pmap_table.h>
#ifdef M88100
#include <machine/m8820x.h>
#endif
#ifdef M88110
#include <machine/m88110.h>
#endif

/*
 * VM externals
 */
extern paddr_t last_addr;
vaddr_t avail_end;
vaddr_t virtual_avail = VM_MIN_KERNEL_ADDRESS;
vaddr_t virtual_end = VM_MAX_KERNEL_ADDRESS;


#ifdef	PMAPDEBUG
/*
 * conditional debugging
 */
#define CD_ACTIVATE	0x00000001	/* pmap_activate */
#define CD_KMAP		0x00000002	/* pmap_expand_kmap */
#define CD_MAP		0x00000004	/* pmap_map */
#define CD_CACHE	0x00000008	/* pmap_cache_ctrl */
#define CD_INIT		0x00000010	/* pmap_init */
#define CD_CREAT	0x00000020	/* pmap_create */
#define CD_DESTR	0x00000040	/* pmap_destroy */
#define CD_RM		0x00000080	/* pmap_remove / pmap_kremove */
#define CD_RMPG		0x00000100	/* pmap_remove_page */
#define CD_EXP		0x00000200	/* pmap_expand */
#define CD_ENT		0x00000400	/* pmap_enter / pmap_kenter_pa */
#define CD_COL		0x00000800	/* pmap_collect */
#define CD_CBIT		0x00001000	/* pmap_changebit */
#define CD_TBIT		0x00002000	/* pmap_testbit */
#define CD_USBIT	0x00004000	/* pmap_unsetbit */
#define	CD_COPY		0x00008000	/* pmap_copy_page */
#define	CD_ZERO		0x00010000	/* pmap_zero_page */
#define	CD_BOOT		0x00020000	/* pmap_bootstrap */
#define CD_ALL		0xffffffff

int pmap_debug = CD_BOOT | CD_KMAP | CD_MAP;

#define	DPRINTF(flg, stmt) \
do { \
	if (pmap_debug & (flg)) \
		printf stmt; \
} while (0)

#else

#define	DPRINTF(flg, stmt) do { } while (0)

#endif	/* PMAPDEBUG */

struct pool pmappool, pvpool;
struct pmap kernel_pmap_store;

apr_t	kernel_apr_cmode = CACHE_WT;	/* XXX CACHE_DFL does not work yet */
apr_t	userland_apr_cmode = CACHE_DFL;
apr_t	default_apr = CACHE_GLOBAL | APR_V;

/*
 * Internal routines
 */
void		 pmap_changebit(struct vm_page *, int, int);
pt_entry_t	*pmap_expand(pmap_t, vaddr_t, int);
pt_entry_t	*pmap_expand_kmap(vaddr_t, int);
void		 pmap_map(paddr_t, psize_t, vm_prot_t, u_int);
pt_entry_t	*pmap_pte(pmap_t, vaddr_t);
void		 pmap_remove_page(struct vm_page *);
void		 pmap_remove_pte(pmap_t, vaddr_t, pt_entry_t *,
		    struct vm_page *, boolean_t);
void		 pmap_remove_range(pmap_t, vaddr_t, vaddr_t);
boolean_t	 pmap_testbit(struct vm_page *, int);
void		 tlb_flush(pmap_t, vaddr_t);
void		 tlb_kflush(vaddr_t);

static __inline pv_entry_t
pg_to_pvh(struct vm_page *pg)
{
	return &pg->mdpage.pvent;
}

/*
 * PTE routines
 */

#define	m88k_protection(prot)	((prot) & VM_PROT_WRITE ? PG_RW : PG_RO)
#define	pmap_pte_w(pte)		(*(pte) & PG_W)

#define SDTENT(pm, va)		((pm)->pm_stab + SDTIDX(va))

/*
 * [INTERNAL]
 * Return the address of the pte for `va' within the page table pointed
 * to by the segment table entry `sdt'. Assumes *sdt is a valid segment
 * table entry.
 */
static __inline__
pt_entry_t *
sdt_pte(sdt_entry_t *sdt, vaddr_t va)
{
	return (pt_entry_t *)(*sdt & PG_FRAME) + PDTIDX(va);
}

/*
 * [INTERNAL]
 * Return the address of the pte for `va' in `pmap'. NULL if there is no
 * page table for `va'.
 */
pt_entry_t *
pmap_pte(pmap_t pmap, vaddr_t va)
{
	sdt_entry_t *sdt;

	sdt = SDTENT(pmap, va);
	if (!SDT_VALID(sdt))
		return NULL;

	return sdt_pte(sdt, va);
}

/*
 * [MD PUBLIC]
 * Change the cache control bits of the address range `sva'..`eva' in
 * pmap_kernel to `mode'.
 */
void
pmap_cache_ctrl(vaddr_t sva, vaddr_t eva, u_int mode)
{
	int s;
	pt_entry_t opte, *pte;
	vaddr_t va;
	paddr_t pa;
	cpuid_t cpu;

	DPRINTF(CD_CACHE, ("pmap_cache_ctrl(%p, %p, %x)\n",
	    sva, eva, mode));

	s = splvm();
	for (va = sva; va != eva; va += PAGE_SIZE) {
		if ((pte = pmap_pte(pmap_kernel(), va)) == NULL)
			continue;
		DPRINTF(CD_CACHE, ("cache_ctrl: pte@%p\n", pte));

		/*
		 * Data cache should be copied back and invalidated if
		 * the old mapping was cached and the new isn't, or if
		 * we are downgrading from writeback to writethrough.
		 */
		if (((*pte & CACHE_INH) == 0 && (mode & CACHE_INH) != 0) ||
		    ((*pte & CACHE_WT) == 0 && (mode & CACHE_WT) != 0)) {
			pa = ptoa(PG_PFNUM(*pte));
#ifdef MULTIPROCESSOR
			for (cpu = 0; cpu < MAX_CPUS; cpu++)
				if (ISSET(m88k_cpus[cpu].ci_flags, CIF_ALIVE)) {
#else
			cpu = cpu_number();
#endif
					if (mode & CACHE_INH)
						cmmu_cache_wbinv(cpu,
						    pa, PAGE_SIZE);
					else
						cmmu_dcache_wb(cpu,
						    pa, PAGE_SIZE);
#ifdef MULTIPROCESSOR
				}
#endif
		}

		/*
		 * Invalidate pte temporarily to avoid being written back
		 * the modified bit and/or the reference bit by any other cpu.
		 */

		opte = invalidate_pte(pte);
		*pte = (opte & ~CACHE_MASK) | mode;
		tlb_kflush(va);
	}
	splx(s);
}

/*
 * [MI]
 * Checks how virtual address `va' would translate with `pmap' as the active
 * pmap. Returns TRUE and matching physical address in `pap' (if not NULL) if
 * translation is possible, FAILS otherwise.
 */
boolean_t
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
	paddr_t pa;
	uint32_t ti;
	int rv;

	rv = pmap_translation_info(pmap, va, &pa, &ti);
	if (rv == PTI_INVALID)
		return FALSE;
	else {
		if (pap != NULL)
			*pap = pa;
		return TRUE;
	}
}

/*
 * [MD PUBLIC]
 * Checks how virtual address `va' would translate with `pmap' as the active
 * pmap. Returns a PTI_xxx constant indicating which translation hardware
 * would perform the translation; if not PTI_INVALID, the matching physical
 * address is returned into `pap', and cacheability of the mapping is
 * returned into `ti'.
 */
int
pmap_translation_info(pmap_t pmap, vaddr_t va, paddr_t *pap, uint32_t *ti)
{
	pt_entry_t *pte;
	int s;
	int rv;

	/*
	 * Check for a BATC translation first.
	 * Even though we do not use BATC yet, 88100-based designs (with
	 * 8820x CMMUs) have two hardwired BATC entries which map the
	 * upper 1MB (so-called `utility space') 1:1 in supervisor space.
	 */
#ifdef M88100
	if (CPU_IS88100 && pmap == pmap_kernel()) {
		if (va >= BATC9_VA) {
			*pap = va;
			*ti = BATC9 & CACHE_MASK;
			return PTI_BATC;
		}
		if (va >= BATC8_VA) {
			*pap = va;
			*ti = BATC8 & CACHE_MASK;
			return PTI_BATC;
		}
	}
#endif

	/*
	 * Check for a regular PTE translation.
	 */

	s = splvm();
	pte = pmap_pte(pmap, va);
	if (pte != NULL && PDT_VALID(pte)) {
		*pap = ptoa(PG_PFNUM(*pte)) | (va & PAGE_MASK);
		*ti = (*pte | pmap->pm_apr) & CACHE_MASK;
		rv = PTI_PTE;
	} else
		rv = PTI_INVALID;

	splx(s);

	return rv;
}

/*
 * TLB (ATC) routines
 */

/*
 * [INTERNAL]
 * Flush translation cache entry for `va' in `pmap'. May act lazily.
 */
void
tlb_flush(pmap_t pmap, vaddr_t va)
{
	struct cpu_info *ci;
	boolean_t kernel = pmap == pmap_kernel();

#ifdef MULTIPROCESSOR	/* { */
	CPU_INFO_ITERATOR cpu;

	/*
	 * On 88100, we take action immediately.
	 */
	if (CPU_IS88100) {
		CPU_INFO_FOREACH(cpu, ci) {
			if (kernel || pmap == ci->ci_curpmap)
				cmmu_tlb_inv(ci->ci_cpuid, kernel, va);
		}
	}

	/*
	 * On 88110, we only remember which tlb need to be invalidated,
	 * and wait for pmap_update() to do it.
	 */
	if (CPU_IS88110) {
		CPU_INFO_FOREACH(cpu, ci) {
			if (kernel)
				ci->ci_pmap_ipi |= CI_IPI_TLB_FLUSH_KERNEL;
			else if (pmap == ci->ci_curpmap)
				ci->ci_pmap_ipi |= CI_IPI_TLB_FLUSH_USER;
		}
	}
#else	/* MULTIPROCESSOR */	/* } { */
	ci = curcpu();

	if (kernel || pmap == ci->ci_curpmap) {
		if (CPU_IS88100)
			cmmu_tlb_inv(ci->ci_cpuid, kernel, va);
		if (CPU_IS88110)
			ci->ci_pmap_ipi |= kernel ?
			    CI_IPI_TLB_FLUSH_KERNEL : CI_IPI_TLB_FLUSH_USER;
	}
#endif	/* MULTIPROCESSOR */	/* } */
}

/*
 * [INTERNAL]
 * Flush translation cache entry for `va' in pmap_kernel(). Acts immediately.
 */
void
tlb_kflush(vaddr_t va)
{
	struct cpu_info *ci;

#ifdef MULTIPROCESSOR	/* { */
	CPU_INFO_ITERATOR cpu;

	if (CPU_IS88100)
		CPU_INFO_FOREACH(cpu, ci)
			cmmu_tlb_inv(ci->ci_cpuid, TRUE, va);
	if (CPU_IS88110)
		CPU_INFO_FOREACH(cpu, ci)
			cmmu_tlb_inv(ci->ci_cpuid, TRUE, 0);
#else	/* MULTIPROCESSOR */	/* } { */
	ci = curcpu();

	if (CPU_IS88100)
		cmmu_tlb_inv(ci->ci_cpuid, TRUE, va);
	if (CPU_IS88110)
		cmmu_tlb_inv(ci->ci_cpuid, TRUE, 0);
#endif	/* MULTIPROCESSOR */	/* } */
}

#ifdef M88110
/*
 * [MI]
 * Perform pending lazy tlb invalidates.
 */
void
pmap_update(pmap_t pm)
{
	/*
	 * Time to perform all necessary TLB invalidations.
	 */
#ifdef M88100
	if (CPU_IS88110) {
#endif
		u_int ipi;
#ifdef MULTIPROCESSOR
		struct cpu_info *ci;
		CPU_INFO_ITERATOR cpu;

		CPU_INFO_FOREACH(cpu, ci)
#else
		struct cpu_info *ci = curcpu();
#endif
		/* CPU_INFO_FOREACH(cpu, ci) */ {
			ipi = atomic_clear_int(&ci->ci_pmap_ipi);
			if (ipi & CI_IPI_TLB_FLUSH_KERNEL)
				cmmu_tlb_inv(ci->ci_cpuid, TRUE, 0);
			if (ipi & CI_IPI_TLB_FLUSH_USER)
				cmmu_tlb_inv(ci->ci_cpuid, FALSE, 0);
		}
#ifdef M88100
	}
#endif
}
#endif

/*
 * [MI]
 * Activate the pmap of process `p'.
 */
void
pmap_activate(struct proc *p)
{
	pmap_t pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	struct cpu_info *ci = curcpu();

	DPRINTF(CD_ACTIVATE, ("pmap_activate(%p) pmap %p\n", p, pmap));

	if (pmap == pmap_kernel()) {
		ci->ci_curpmap = NULL;
	} else {
		if (pmap != ci->ci_curpmap) {
			cmmu_set_uapr(pmap->pm_apr);
			cmmu_tlb_inv_all(ci->ci_cpuid);
			ci->ci_curpmap = pmap;
		}
	}
}

/*
 * [MI]
 * Deactivates the pmap of process `p'.
 */
void
pmap_deactivate(struct proc *p)
{
	struct cpu_info *ci = curcpu();

	ci->ci_curpmap = NULL;
}

/*
 * Segment and page table management routines
 */

/*
 * [INTERNAL]
 * Expand pmap_kernel() to be able to map a page at `va', by allocating
 * a page table. Returns a pointer to the pte of this page, or NULL
 * if allocation failed and `canfail' is nonzero. Panics if allocation
 * fails and `canfail' is zero.
 * Caller is supposed to only invoke this function if
 * pmap_pte(pmap_kernel(), va) returns NULL.
 */
pt_entry_t *
pmap_expand_kmap(vaddr_t va, int canfail)
{
	sdt_entry_t *sdt;
	struct vm_page *pg;
	paddr_t pa;

	DPRINTF(CD_KMAP, ("pmap_expand_kmap(%p, %d)\n", va, canfail));

	if (__predict_true(uvm.page_init_done)) {
		pg = uvm_pagealloc(NULL, 0, NULL,
		    (canfail ? 0 : UVM_PGA_USERESERVE) | UVM_PGA_ZERO);
		if (pg == NULL) {
			if (canfail)
				return NULL;
			panic("pmap_expand_kmap(%p): uvm_pagealloc() failed",
			    va);
		}
		pa = VM_PAGE_TO_PHYS(pg);
	} else {
		pa = (paddr_t)uvm_pageboot_alloc(PAGE_SIZE);
		if (pa == 0)
			panic("pmap_expand_kmap(%p): uvm_pageboot_alloc() failed",
			    va);
		bzero((void *)pa, PAGE_SIZE);
	}

	/* memory for page tables should not be writeback */
	pmap_cache_ctrl(pa, pa + PAGE_SIZE, CPU_IS88100 ? CACHE_INH : CACHE_WT);
	sdt = SDTENT(pmap_kernel(), va);
	*sdt = pa | SG_SO | SG_RW | PG_M | SG_V;
	return sdt_pte(sdt, va);
}

/*
 * [INTERNAL]
 * Expand `pmap' to be able to map a page at `va', by allocating
 * a page table. Returns a pointer to the pte of this page, or NULL
 * if allocation failed and `canfail' is nonzero. Waits until memory is
 * available if allocation fails and `canfail' is zero.
 * Caller is supposed to only invoke this function if
 * pmap_pte(pmap, va) returns NULL.
 */
pt_entry_t *
pmap_expand(pmap_t pmap, vaddr_t va, int canfail)
{
	struct vm_page *pg;
	paddr_t pa;
	sdt_entry_t *sdt;

	DPRINTF(CD_EXP, ("pmap_expand(%p, %p, %d)\n", pmap, va, canfail));

	sdt = SDTENT(pmap, va);
	for (;;) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg != NULL)
			break;
		if (canfail)
			return NULL;
		uvm_wait(__func__);
	}

	pa = VM_PAGE_TO_PHYS(pg);
	/* memory for page tables should not be writeback */
	pmap_cache_ctrl(pa, pa + PAGE_SIZE, CPU_IS88100 ? CACHE_INH : CACHE_WT);

	*sdt = pa | SG_RW | PG_M | SG_V;

	return sdt_pte(sdt, va);
}

/*
 * Bootstrap routines
 */

/*
 * [MI]
 * Early allocation, directly from the vm_physseg ranges of managed pages
 * passed to UVM. Pages ``stolen'' by this routine will never be seen as
 * managed pages and will not have vm_page structs created for them,
 */
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstartp, vaddr_t *vendp)
{
	vaddr_t va;
	u_int npg;

	size = round_page(size);
	npg = atop(size);

	/* m88k systems only have one segment. */
#ifdef DIAGNOSTIC
	if (vm_physmem[0].avail_end - vm_physmem[0].avail_start < npg)
		panic("pmap_steal_memory(%x): out of memory", size);
#endif

	va = ptoa(vm_physmem[0].avail_start);
	vm_physmem[0].avail_start += npg;
	vm_physmem[0].start += npg;

	if (vstartp != NULL)
		*vstartp = virtual_avail;
	if (vendp != NULL)
		*vendp = virtual_end;
	
	bzero((void *)va, size);
	return (va);
}

/*
 * [INTERNAL]
 * Setup a wired mapping in pmap_kernel(). Similar to pmap_kenter_pa(),
 * but allows explicit cacheability control.
 */
void
pmap_map(paddr_t pa, psize_t sz, vm_prot_t prot, u_int cmode)
{
	pt_entry_t template, *pte;

	DPRINTF(CD_MAP, ("pmap_map(%p, %p, %x, %x)\n",
	    pa, sz, prot, cmode));
#ifdef DIAGNOSTIC
	if (pa != 0 && pa < VM_MAX_KERNEL_ADDRESS)
		panic("pmap_map: virtual range %p-%p overlaps KVM",
		    pa, pa + sz);
#endif

	template = m88k_protection(prot) | cmode | PG_W | PG_V;
#ifdef M88110
	if (CPU_IS88110 && m88k_protection(prot) != PG_RO)
		template |= PG_M;
#endif

	sz = atop(round_page(pa + sz) - trunc_page(pa));
	pa = trunc_page(pa);
	while (sz-- != 0) {
		if ((pte = pmap_pte(pmap_kernel(), pa)) == NULL)
			pte = pmap_expand_kmap(pa, 0);

		*pte = template | pa;
		pa += PAGE_SIZE;
		pmap_kernel()->pm_stats.resident_count++;
		pmap_kernel()->pm_stats.wired_count++;
	}
}

/*
 * [MD]
 * Initialize kernel translation tables.
 */
void
pmap_bootstrap(paddr_t s_rom, paddr_t e_rom)
{
	paddr_t s_text, e_text;
	unsigned int nsdt, npdt;
	unsigned int i;
	sdt_entry_t *sdt;
	pt_entry_t *pdt, template;
	paddr_t pa, sptpa, eptpa;
	const struct pmap_table *ptable;
	extern void *kernelstart;
	extern void *etext;

	virtual_avail = (vaddr_t)avail_end;

	s_text = trunc_page((vaddr_t)&kernelstart);
	e_text = round_page((vaddr_t)&etext);

	/*
	 * Reserve space for 1:1 memory mapping in supervisor space.
	 * We need:
	 * - roundup(avail_end, SDT_SIZE) / SDT_SIZE segment tables;
	 *   these will fit in one page.
	 * - roundup(avail_end, PDT_SIZE) / PDT_SIZE page tables;
	 *   these will span several pages.
	 */

	nsdt = roundup(avail_end, (1 << SDT_SHIFT)) >> SDT_SHIFT;
	npdt = roundup(avail_end, (1 << PDT_SHIFT)) >> PDT_SHIFT;
	DPRINTF(CD_BOOT, ("avail_end %08x pages %08x nsdt %08x npdt %08x\n",
	    avail_end, atop(avail_end), nsdt, npdt));

	sdt = (sdt_entry_t *)uvm_pageboot_alloc(PAGE_SIZE);
	pdt = (pt_entry_t *)
	    uvm_pageboot_alloc(round_page(npdt * sizeof(pt_entry_t)));
	DPRINTF(CD_BOOT, ("kernel sdt %p", sdt));
	sptpa = (paddr_t)sdt;
	pmap_kernel()->pm_stab = sdt;
	pa = (paddr_t)pdt;
	eptpa = pa + round_page(npdt * sizeof(pt_entry_t));
	for (i = nsdt; i != 0; i--) {
		*sdt++ = pa | SG_SO | SG_RW | PG_M | SG_V;
		pa += PAGE_SIZE;
	}
	DPRINTF(CD_BOOT, ("-%p\n", sdt));
	for (i = (PAGE_SIZE / sizeof(sdt_entry_t)) - nsdt; i != 0; i--)
		*sdt++ = SG_NV;
	KDASSERT((vaddr_t)sdt == (vaddr_t)pdt);
	DPRINTF(CD_BOOT, ("kernel pdt %p", pdt));

	/* memory below the kernel image */
	for (i = atop(s_text); i != 0; i--)
		*pdt++ = PG_NV;
	/* kernel text */
	pa = s_text;
	for (i = atop(e_text) - atop(pa); i != 0; i--) {
		*pdt++ = pa | PG_SO | PG_RO | PG_W | PG_V;
		pa += PAGE_SIZE;
	}
	/* kernel data */
	for (i = atop(sptpa) - atop(pa); i != 0; i--) {
		*pdt++ = pa | PG_SO | PG_RW | PG_M_U | PG_W | PG_V;
		pa += PAGE_SIZE;
	}
	/* kernel page tables */
	template = PG_SO | PG_RW | PG_M_U | PG_W | PG_V |
	    (CPU_IS88100 ? CACHE_INH : CACHE_WT);
	for (i = atop(eptpa) - atop(pa); i != 0; i--) {
		*pdt++ = pa | template;
		pa += PAGE_SIZE;
	}
	/* regular memory */
	for (i = atop(avail_end) - atop(pa); i != 0; i--) {
		*pdt++ = pa | PG_SO | PG_RW | PG_M_U | PG_V;
		pa += PAGE_SIZE;
	}
	DPRINTF(CD_BOOT, ("-%p, pa %08x\n", pdt, pa));
	for (i = (pt_entry_t *)round_page((vaddr_t)pdt) - pdt; i != 0; i--)
		*pdt++ = PG_NV;

	/*
	 * Create all the machine-specific mappings.
	 * XXX This should eventually get done in machdep.c instead of here;
	 * XXX and on a driver basis on luna88k... If only to be able to grow
	 * XXX VM_MAX_KERNEL_ADDRESS.
	 */

	if (e_rom != s_rom)
		pmap_map(s_rom, e_rom - s_rom, UVM_PROT_RW, CACHE_INH);
	for (ptable = pmap_table_build(); ptable->size != (vsize_t)-1; ptable++)
		if (ptable->size != 0)
			pmap_map(ptable->start, ptable->size,
			    ptable->prot, ptable->cacheability);

	/*
	 * Switch to using new page tables
	 */

#if !defined(MULTIPROCESSOR) && defined(M88110)
	if (CPU_IS88110)
		default_apr &= ~CACHE_GLOBAL;
#endif
	pmap_kernel()->pm_count = 1;
	pmap_kernel()->pm_apr = sptpa | default_apr | kernel_apr_cmode;

	DPRINTF(CD_BOOT, ("default apr %08x kernel apr %08x\n",
	    default_apr, sptpa));

	pmap_bootstrap_cpu(cpu_number());
}

/*
 * [MD]
 * Enable address translation on the current processor.
 */
void
pmap_bootstrap_cpu(cpuid_t cpu)
{
	/* Load supervisor pointer to segment table. */
	cmmu_set_sapr(pmap_kernel()->pm_apr);
#ifdef PMAPDEBUG
	printf("cpu%d: running virtual\n", cpu);
#endif
	curcpu()->ci_curpmap = NULL;
}

/*
 * [MI]
 * Complete the pmap layer initialization, to be able to manage userland
 * pmaps.
 */
void
pmap_init(void)
{
	DPRINTF(CD_INIT, ("pmap_init()\n"));
	pool_init(&pmappool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pvpool, sizeof(pv_entry_t), 0, 0, 0, "pvpl", NULL);
}

/*
 * Pmap structure management
 */

/*
 * [MI]
 * Create a new pmap.
 */
pmap_t
pmap_create(void)
{
	pmap_t pmap;
	struct vm_page *pg;
	paddr_t pa;

	pmap = pool_get(&pmappool, PR_WAITOK | PR_ZERO);

	/* Allocate the segment table page immediately. */
	for (;;) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg != NULL)
			break;
		uvm_wait(__func__);
	}

	pa = VM_PAGE_TO_PHYS(pg);
	/* memory for page tables should not be writeback */
	pmap_cache_ctrl(pa, pa + PAGE_SIZE, CPU_IS88100 ? CACHE_INH : CACHE_WT);

	pmap->pm_stab = (sdt_entry_t *)pa;
	pmap->pm_apr = pa | default_apr | userland_apr_cmode;
	pmap->pm_count = 1;

	DPRINTF(CD_CREAT, ("pmap_create() -> pmap %p, pm_stab %p\n", pmap, pa));

	return pmap;
}

/*
 * [MI]
 * Decreased the pmap reference count, and destroy it when it reaches zero.
 */
void
pmap_destroy(pmap_t pmap)
{
	u_int u;
	sdt_entry_t *sdt;
	paddr_t pa;

	DPRINTF(CD_DESTR, ("pmap_destroy(%p)\n", pmap));
	if (--pmap->pm_count == 0) {
		for (u = SDT_ENTRIES, sdt = pmap->pm_stab; u != 0; sdt++, u--) {
			if (SDT_VALID(sdt)) {
				pa = *sdt & PG_FRAME;
				pmap_cache_ctrl(pa, pa + PAGE_SIZE, CACHE_DFL);
				uvm_pagefree(PHYS_TO_VM_PAGE(pa));
			}
		}
		pa = (paddr_t)pmap->pm_stab;
		pmap_cache_ctrl(pa, pa + PAGE_SIZE, CACHE_DFL);
		uvm_pagefree(PHYS_TO_VM_PAGE(pa));
		pool_put(&pmappool, pmap);
	}
}

/*
 * [MI]
 * Increase the pmap reference count.
 */
void
pmap_reference(pmap_t pmap)
{
	pmap->pm_count++;
}

/*
 * [MI]
 * Attempt to regain memory by freeing disposable page tables.
 */
void
pmap_collect(pmap_t pmap)
{
	u_int u, v;
	sdt_entry_t *sdt;
	pt_entry_t *pte;
	vaddr_t va;
	paddr_t pa;
	int s;

	DPRINTF(CD_COL, ("pmap_collect(%p)\n", pmap));

	s = splvm();
	for (sdt = pmap->pm_stab, va = 0, u = SDT_ENTRIES; u != 0;
	    sdt++, va += (1 << SDT_SHIFT), u--) {
		if (!SDT_VALID(sdt))
			continue;
		pte = sdt_pte(sdt, 0);
		for (v = PDT_ENTRIES; v != 0; pte++, v--)
			if (pmap_pte_w(pte)) /* wired mappings can't go */
				break;
		if (v != 0)
			continue;
		/* found a suitable pte page to reclaim */
		pmap_remove_range(pmap, va, va + (1 << SDT_SHIFT));

		pa = *sdt & PG_FRAME;
		*sdt = SG_NV;
		pmap_cache_ctrl(pa, pa + PAGE_SIZE, CACHE_DFL);
		uvm_pagefree(PHYS_TO_VM_PAGE(pa));
	}
	splx(s);

	DPRINTF(CD_COL, ("pmap_collect(%p) done\n", pmap));
}

/*
 * Virtual mapping/unmapping routines
 */

/*
 * [MI]
 * Establish a `va' to `pa' translation with protection `prot' in `pmap'.
 * The `flags' argument contains the expected usage protection of the
 * mapping (and may differ from the currently requested protection), as
 * well as a possible PMAP_WIRED flag.
 */
int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	int s;
	pt_entry_t *pte, template;
	paddr_t old_pa;
	pv_entry_t pv_e, pvl;
	boolean_t wired = (flags & PMAP_WIRED) != 0;
	struct vm_page *pg;

	DPRINTF(CD_ENT, ("pmap_enter(%p, %p, %p, %x, %x)\n",
	    pmap, va, pa, prot, flags));

	template = m88k_protection(prot);

	/*
	 * Expand pmap to include this pte.
	 */
	if ((pte = pmap_pte(pmap, va)) == NULL) {
		if (pmap == pmap_kernel())
			pte = pmap_expand_kmap(va, flags & PMAP_CANFAIL);
		else
			pte = pmap_expand(pmap, va, flags & PMAP_CANFAIL);

		/* will only return NULL if PMAP_CANFAIL is set */
		if (pte == NULL) {
			DPRINTF(CD_ENT, ("failed (ENOMEM)\n"));
			return (ENOMEM);
		}
	}

	/*
	 * Special case if the physical page is already mapped at this address.
	 */
	old_pa = ptoa(PG_PFNUM(*pte));
	DPRINTF(CD_ENT, ("pmap_enter: old_pa %p pte %p\n", old_pa, *pte));

	pg = PHYS_TO_VM_PAGE(pa);
	s = splvm();
	if (pg != NULL)
		pvl = pg_to_pvh(pg);
	else
		pvl = NULL;

	if (old_pa == pa) {
		/* May be changing its wired attributes or protection */
		if (wired && !(pmap_pte_w(pte)))
			pmap->pm_stats.wired_count++;
		else if (!wired && pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
	} else {
		/* Remove old mapping from the PV list if necessary. */
		if (PDT_VALID(pte))
			pmap_remove_pte(pmap, va, pte, NULL, FALSE);

		if (pvl != NULL) {
			/*
			 * Enter the mapping in the PV list for this
			 * managed page.
			 */
			if (pvl->pv_pmap == NULL) {
				/*
				 * No mappings yet.
				 */
				pvl->pv_va = va;
				pvl->pv_pmap = pmap;
				pvl->pv_next = NULL;
				pvl->pv_flags = 0;
			} else {
				/*
				 * Add new pv_entry after header.
				 */
				pv_e = pool_get(&pvpool, PR_NOWAIT);
				if (pv_e == NULL) {
					/* Invalidate the old pte anyway */
					tlb_flush(pmap, va);

					if (flags & PMAP_CANFAIL) {
						splx(s);
						return (ENOMEM);
					} else
						panic("pmap_enter: "
						    "pvpool exhausted");
				}
				pv_e->pv_va = va;
				pv_e->pv_pmap = pmap;
				pv_e->pv_next = pvl->pv_next;
				pv_e->pv_flags = 0;
				pvl->pv_next = pv_e;
			}
		}

		/*
		 * And count the mapping.
		 */
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;
	} /* if (pa == old_pa) ... else */

	template |= PG_V;
	if (wired)
		template |= PG_W;

	if (prot & VM_PROT_WRITE) {
		/*
		 * On 88110, do not mark writable mappings as dirty unless we
		 * know the page is dirty, or we are using the kernel pmap.
		 */
		if (CPU_IS88110 && pmap != pmap_kernel() &&
		    pg != NULL && (pvl->pv_flags & PG_M) == 0)
			template |= PG_U;
		else
			template |= PG_M_U;
	} else if (prot & VM_PROT_ALL)
		template |= PG_U;

	/*
	 * If outside physical memory, disable cache on this (device) page.
	 */
	if (pa >= last_addr)
		template |= CACHE_INH;

	/*
	 * Invalidate pte temporarily to avoid being written
	 * back the modified bit and/or the reference bit by
	 * any other cpu.
	 */
	template |= invalidate_pte(pte) & PG_M_U;
	*pte = template | pa;
	tlb_flush(pmap, va);
	DPRINTF(CD_ENT, ("pmap_enter: new pte %p\n", *pte));

	/*
	 * Cache attribute flags
	 */
	if (pvl != NULL) {
		if (flags & VM_PROT_WRITE) {
			if (CPU_IS88110 && pmap != pmap_kernel())
				pvl->pv_flags |= PG_U;
			else
				pvl->pv_flags |= PG_M_U;
		} else if (flags & VM_PROT_ALL)
			pvl->pv_flags |= PG_U;
	}

	splx(s);

	return 0;
}

/*
 * [MI]
 * Fast pmap_enter() version for pmap_kernel() and unmanaged pages.
 */
void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t template, *pte;

	DPRINTF(CD_ENT, ("pmap_kenter_pa(%p, %p, %x)\n", va, pa, prot));

	template = m88k_protection(prot) | PG_W | PG_V;
#ifdef M88110
	if (CPU_IS88110 && m88k_protection(prot) != PG_RO)
		template |= PG_M;
#endif
	/*
	 * If outside physical memory, disable cache on this (device) page.
	 */
	if (pa >= last_addr)
		template |= CACHE_INH;

	/*
	 * Expand pmap to include this pte.
	 */
	if ((pte = pmap_pte(pmap_kernel(), va)) == NULL)
		pte = pmap_expand_kmap(va, 0);

	/*
	 * And count the mapping.
	 */
	pmap_kernel()->pm_stats.resident_count++;
	pmap_kernel()->pm_stats.wired_count++;

	invalidate_pte(pte);
	*pte = template | pa;
	tlb_kflush(va);
}

/*
 * [INTERNAL]
 * Remove the page at `va' in `pmap', which pte is pointed to by `pte', and
 * update the status of the vm_page matching this translation (if this is
 * indeed a managed page). Flushe the tlb entry if `flush' is nonzero.
 */
void
pmap_remove_pte(pmap_t pmap, vaddr_t va, pt_entry_t *pte, struct vm_page *pg,
   boolean_t flush)
{
	pt_entry_t opte;
	pv_entry_t prev, cur, pvl;
	paddr_t pa;

	splassert(IPL_VM);
	DPRINTF(CD_RM, ("pmap_remove_pte(%p, %p, %d)\n", pmap, va, flush));

	/*
	 * Update statistics.
	 */
	pmap->pm_stats.resident_count--;
	if (pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;

	pa = ptoa(PG_PFNUM(*pte));

	/*
	 * Invalidate the pte.
	 */

	opte = invalidate_pte(pte) & PG_M_U;
	if (flush)
		tlb_flush(pmap, va);

	if (pg == NULL) {
		pg = PHYS_TO_VM_PAGE(pa);
		/* If this isn't a managed page, just return. */
		if (pg == NULL)
			return;
	}

	/*
	 * Remove the mapping from the pvlist for
	 * this physical page.
	 */
	pvl = pg_to_pvh(pg);

#ifdef DIAGNOSTIC
	if (pvl->pv_pmap == NULL)
		panic("pmap_remove_pte(%p, %p, %p, %p/%p, %d): null pv_list",
		   pmap, va, pte, pa, pg, flush);
#endif

	prev = NULL;
	for (cur = pvl; cur != NULL; cur = cur->pv_next) {
		if (cur->pv_va == va && cur->pv_pmap == pmap)
			break;
		prev = cur;
	}
	if (cur == NULL) {
		panic("pmap_remove_pte(%p, %p, %p, %p, %d): mapping for va "
		    "(pa %p) not in pv list at %p",
		    pmap, va, pte, pg, flush, pa, pvl);
	}

	if (prev == NULL) {
		/*
		 * Handler is the pv_entry. Copy the next one
		 * to handler and free the next one (we can't
		 * free the handler)
		 */
		cur = cur->pv_next;
		if (cur != NULL) {
			cur->pv_flags = pvl->pv_flags;
			*pvl = *cur;
			pool_put(&pvpool, cur);
		} else {
			pvl->pv_pmap = NULL;
#ifdef M88100
			if (CPU_IS88100 &&
			    kernel_apr_cmode != userland_apr_cmode) {
				/* XXX Why isn't cmmu_dcache_wb() enough? */
				if (0)
					cmmu_dcache_wb(cpu_number(),
					    pa, PAGE_SIZE);
				else
					cmmu_cache_wbinv(cpu_number(),
					    pa, PAGE_SIZE);
			}
#endif
		}
	} else {
		prev->pv_next = cur->pv_next;
		pool_put(&pvpool, cur);
	}

	/* Update saved attributes for managed page */
	pvl->pv_flags |= opte;
}

/*
 * [INTERNAL]
 * Removes all mappings within the `sva'..`eva' range in `pmap'.
 */
void
pmap_remove_range(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{
	vaddr_t va, eseg;
	pt_entry_t *pte;

	DPRINTF(CD_RM, ("pmap_remove_range(%p, %p, %p)\n", pmap, sva, eva));

	/*
	 * Loop through the range in PAGE_SIZE increments.
	 */
	va = sva;
	while (va != eva) {
		sdt_entry_t *sdt;

		eseg = (va & SDT_MASK) + (1 << SDT_SHIFT);
		if (eseg > eva || eseg == 0)
			eseg = eva;

		sdt = SDTENT(pmap, va);
		/* If no segment table, skip a whole segment */
		if (!SDT_VALID(sdt))
			va = eseg;
		else {
			pte = sdt_pte(sdt, va);
			while (va != eseg) {
				if (PDT_VALID(pte))
					pmap_remove_pte(pmap, va, pte, NULL,
					    TRUE);
				va += PAGE_SIZE;
				pte++;
			}
		}
	}
}

/*
 * [MI]
 * Removes all mappings within the `sva'..`eva' range in `pmap'.
 */
void
pmap_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{
	int s;

	s = splvm();
	pmap_remove_range(pmap, sva, eva);
	splx(s);
}

/*
 * [MI]
 * Fast pmap_remove() version for pmap_kernel() and unmanaged pages.
 */
void
pmap_kremove(vaddr_t va, vsize_t len)
{
	vaddr_t e, eseg;

	DPRINTF(CD_RM, ("pmap_kremove(%p, %x)\n", va, len));

	e = va + len;
	while (va != e) {
		sdt_entry_t *sdt;
		pt_entry_t *pte;

		eseg = (va & SDT_MASK) + (1 << SDT_SHIFT);
		if (eseg > e || eseg == 0)
			eseg = e;

		sdt = SDTENT(pmap_kernel(), va);

		/* If no segment table, skip a whole segment */
		if (!SDT_VALID(sdt))
			va = eseg;
		else {
			pte = sdt_pte(sdt, va);
			while (va != eseg) {
				if (PDT_VALID(pte)) {
					/* Update the counts */
					pmap_kernel()->pm_stats.resident_count--;
					pmap_kernel()->pm_stats.wired_count--;

					invalidate_pte(pte);
					tlb_kflush(va);
				}
				va += PAGE_SIZE;
				pte++;
			}
		}
	}
}

/*
 * [INTERNAL]
 * Removes all mappings of managed page `pg'.
 */
void
pmap_remove_page(struct vm_page *pg)
{
	pt_entry_t *pte;
	pv_entry_t pvl;
	vaddr_t va;
	pmap_t pmap;
	int s;

	DPRINTF(CD_RMPG, ("pmap_remove_page(%p)\n", pg));

	s = splvm();
	/*
	 * Walk down PV list, removing all mappings.
	 */
	pvl = pg_to_pvh(pg);
	while (pvl != NULL && (pmap = pvl->pv_pmap) != NULL) {
		va = pvl->pv_va;
		pte = pmap_pte(pmap, va);

		if (pte == NULL || !PDT_VALID(pte)) {
			pvl = pvl->pv_next;
			continue;	/* no page mapping */
		}
		if (pmap_pte_w(pte)) {
			DPRINTF(CD_RMPG, ("pmap_remove_page(%p): wired mapping not removed\n",
			    pg));
			pvl = pvl->pv_next;
			continue;
		}

		pmap_remove_pte(pmap, va, pte, pg, TRUE);
		/*
		 * Do not free any empty page tables,
		 * leave that for when VM calls pmap_collect().
		 */
	}
	splx(s);
}

/*
 * [MI]
 * Strengthens the protection of the `sva'..`eva' range within `pmap' to `prot'.
 */
void
pmap_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	int s;
	pt_entry_t *pte, ap;
	vaddr_t va, eseg;

	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	ap = m88k_protection(prot);

	s = splvm();
	/*
	 * Loop through the range in PAGE_SIZE increments.
	 */
	va = sva;
	while (va != eva) {
		sdt_entry_t *sdt;

		eseg = (va & SDT_MASK) + (1 << SDT_SHIFT);
		if (eseg > eva || eseg == 0)
			eseg = eva;

		sdt = SDTENT(pmap, va);
		/* If no segment table, skip a whole segment */
		if (!SDT_VALID(sdt))
			va = eseg;
		else {
			pte = sdt_pte(sdt, va);
			while (va != eseg) {
				if (PDT_VALID(pte)) {
					/*
					 * Invalidate pte temporarily to avoid
					 * the modified bit and/or the
					 * reference bit being written back by
					 * any other cpu.
					 */
					*pte = ap |
					    (invalidate_pte(pte) & ~PG_PROT);
					tlb_flush(pmap, va);
				}
				va += PAGE_SIZE;
				pte++;
			}
		}
	}
	splx(s);
}

/*
 * [MI]
 * Removes the wired state of the page at `va' in `pmap'.
 */
void
pmap_unwire(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte;

	pte = pmap_pte(pmap, va);
	if (pmap_pte_w(pte)) {
		pmap->pm_stats.wired_count--;
		*pte &= ~PG_W;
	}
}

/*
 * vm_page management routines
 */

/*
 * [MI]
 * Copies vm_page `srcpg' to `dstpg'.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);

	DPRINTF(CD_COPY, ("pmap_copy_page(%p,%p) pa %p %p\n",
	    srcpg, dstpg, src, dst));
#ifdef M88100
	if (CPU_IS88100 &&
	    kernel_apr_cmode != userland_apr_cmode)
		cmmu_dcache_wb(cpu_number(), src, PAGE_SIZE);
#endif
	curcpu()->ci_copypage((vaddr_t)src, (vaddr_t)dst);
}

/*
 * [MI]
 * Clears vm_page `pg'.
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	DPRINTF(CD_ZERO, ("pmap_zero_page(%p) pa %p\n", pg, pa));
	curcpu()->ci_zeropage((vaddr_t)pa);
}

/*
 * [INTERNAL]
 * Alters bits in the pte of all mappings of `pg'. For each pte, bits in
 * `set' are set and bits not in `mask' are cleared. The flags summary
 * at the head of the pv list is modified in a similar way.
 */
void
pmap_changebit(struct vm_page *pg, int set, int mask)
{
	pv_entry_t pvl, pvep;
	pt_entry_t *pte, npte, opte;
	pmap_t pmap;
	int s;
	vaddr_t va;

	DPRINTF(CD_CBIT, ("pmap_changebit(%p, %x, %x)\n", pg, set, mask));

	s = splvm();

	pvl = pg_to_pvh(pg);
	/*
	 * Clear saved attributes (modify, reference)
	 */
	pvl->pv_flags &= mask;

	if (pvl->pv_pmap != NULL) {
		/* for each listed pmap, update the affected bits */
		for (pvep = pvl; pvep != NULL; pvep = pvep->pv_next) {
			pmap = pvep->pv_pmap;
			va = pvep->pv_va;
			pte = pmap_pte(pmap, va);

			/*
			 * Check for existing and valid pte
			 */
			if (pte == NULL || !PDT_VALID(pte))
				continue;	 /* no page mapping */
#ifdef PMAPDEBUG
			if (ptoa(PG_PFNUM(*pte)) != VM_PAGE_TO_PHYS(pg))
				panic("pmap_changebit: pte %08x in pmap %p doesn't point to page %p@%p",
				    *pte, pmap, pg, VM_PAGE_TO_PHYS(pg));
#endif

			/*
			 * Update bits
			 */
			opte = *pte;
			npte = (opte | set) & mask;

			/*
			 * Invalidate pte temporarily to avoid the modified bit
			 * and/or the reference being written back by any other
			 * cpu.
			 */
			if (npte != opte) {
				invalidate_pte(pte);
				*pte = npte;
				tlb_flush(pmap, va);
			}
		}
	}

	splx(s);
}

/*
 * [INTERNAL]
 * Checks for `bit' being set in at least one pte of all mappings of `pg'.
 * The flags summary at the head of the pv list is checked first, and will
 * be set if it wasn't but the bit is found set in one pte.
 * Returns TRUE if the bit is found, FALSE if not.
 */
boolean_t
pmap_testbit(struct vm_page *pg, int bit)
{
	pv_entry_t pvl, pvep;
	pt_entry_t *pte;
	pmap_t pmap;
	int s;

	DPRINTF(CD_TBIT, ("pmap_testbit(%p, %x): ", pg, bit));

	s = splvm();

	pvl = pg_to_pvh(pg);
	if (pvl->pv_flags & bit) {
		/* we've already cached this flag for this page,
		   no use looking further... */
		DPRINTF(CD_TBIT, ("cached\n"));
		splx(s);
		return (TRUE);
	}

	if (pvl->pv_pmap != NULL) {
		/* for each listed pmap, check modified bit for given page */
		for (pvep = pvl; pvep != NULL; pvep = pvep->pv_next) {
			pmap = pvep->pv_pmap;

			pte = pmap_pte(pmap, pvep->pv_va);
			if (pte == NULL || !PDT_VALID(pte))
				continue;

#ifdef PMAPDEBUG
			if (ptoa(PG_PFNUM(*pte)) != VM_PAGE_TO_PHYS(pg))
				panic("pmap_testbit: pte %08x in pmap %p doesn't point to page %p@%p",
				    *pte, pmap, pg, VM_PAGE_TO_PHYS(pg));
#endif

			if ((*pte & bit) != 0) {
				pvl->pv_flags |= bit;
				DPRINTF(CD_TBIT, ("found\n"));
				splx(s);
				return (TRUE);
			}
		}
	}

	DPRINTF(CD_TBIT, ("not found\n"));
	splx(s);
	return (FALSE);
}

/*
 * [INTERNAL]
 * Clears `bit' in the pte of all mapping of `pg', as well as in the flags
 * summary at the head of the pv list.
 * Returns TRUE if the bit was found set in either a mapping or the summary,
 * FALSE if not.
 */
boolean_t
pmap_unsetbit(struct vm_page *pg, int bit)
{
	boolean_t rv = FALSE;
	pv_entry_t pvl, pvep;
	pt_entry_t *pte, opte;
	pmap_t pmap;
	int s;
	vaddr_t va;

	DPRINTF(CD_USBIT, ("pmap_unsetbit(%p, %x): ", pg, bit));

	s = splvm();

	pvl = pg_to_pvh(pg);

	/*
	 * Clear saved attributes
	 */
	if (pvl->pv_flags & bit) {
		pvl->pv_flags ^= bit;
		rv = TRUE;
	}

	if (pvl->pv_pmap != NULL) {
		/* for each listed pmap, update the specified bit */
		for (pvep = pvl; pvep != NULL; pvep = pvep->pv_next) {
			pmap = pvep->pv_pmap;
			va = pvep->pv_va;
			pte = pmap_pte(pmap, va);

			/*
			 * Check for existing and valid pte
			 */
			if (pte == NULL || !PDT_VALID(pte))
				continue;	 /* no page mapping */
#ifdef PMAPDEBUG
			if (ptoa(PG_PFNUM(*pte)) != VM_PAGE_TO_PHYS(pg))
				panic("pmap_unsetbit: pte %08x in pmap %p doesn't point to page %p@%p",
				    *pte, pmap, pg, VM_PAGE_TO_PHYS(pg));
#endif

			/*
			 * Update bits
			 */
			opte = *pte;
			if (opte & bit) {
				/*
				 * Invalidate pte temporarily to avoid the
				 * specified bit being written back by any
				 * other cpu.
				 */
				invalidate_pte(pte);
				*pte = opte ^ bit;
				tlb_flush(pmap, va);
				rv = TRUE;
			}
		}
	}
	splx(s);

	DPRINTF(CD_USBIT, (rv ? "TRUE\n" : "FALSE\n"));
	return (rv);
}

/*
 * [MI]
 * Checks whether `pg' is dirty.
 * Returns TRUE if there is at least one mapping of `pg' with the modified
 * bit set in its pte, FALSE if not.
 */
boolean_t
pmap_is_modified(struct vm_page *pg)
{
#ifdef M88110
	/*
	 * Since on 88110 PG_M bit tracking is done in software, we can
	 * trust the page flags without having to walk the individual
	 * ptes in case the page flags are behind actual usage.
	 */
	if (CPU_IS88110) {
		pv_entry_t pvl;
		boolean_t rc = FALSE;

		pvl = pg_to_pvh(pg);
		if (pvl->pv_flags & PG_M)
			rc = TRUE;
		DPRINTF(CD_TBIT, ("pmap_is_modified(%p) -> %x\n", pg, rc));
		return (rc);
	}
#endif

	return pmap_testbit(pg, PG_M);
}

/*
 * [MI]
 * Checks whether `pg' is in use.
 * Returns TRUE if there is at least one mapping of `pg' with the used bit
 * set in its pte, FALSE if not.
 */
boolean_t
pmap_is_referenced(struct vm_page *pg)
{
	return pmap_testbit(pg, PG_U);
}

/*
 * [MI]
 * Strengthens protection of `pg' to `prot'.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	if ((prot & VM_PROT_READ) == VM_PROT_NONE)
		pmap_remove_page(pg);
	else if ((prot & VM_PROT_WRITE) == VM_PROT_NONE)
		pmap_changebit(pg, PG_RO, ~0);
}

/*
 * Miscellaneous routines
 */

/*
 * [MI]
 * Flushes instruction cache for the range `va'..`va'+`len' in proc `p'.
 */
void
pmap_proc_iflush(struct proc *p, vaddr_t va, vsize_t len)
{
	pmap_t pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	paddr_t pa;
	vsize_t count;
	struct cpu_info *ci;

	while (len != 0) {
		count = min(len, PAGE_SIZE - (va & PAGE_MASK));
		if (pmap_extract(pmap, va, &pa)) {
#ifdef MULTIPROCESSOR
			CPU_INFO_ITERATOR cpu;

			CPU_INFO_FOREACH(cpu, ci)
#else
			ci = curcpu();
#endif
			/* CPU_INFO_FOREACH(cpu, ci) */ {
				cmmu_icache_inv(ci->ci_cpuid, pa, count);
			}
		}
		va += count;
		len -= count;
	}
}

#ifdef M88110
/*
 * [INTERNAL]
 * Updates the pte mapping `va' in `pmap' upon write fault, to set the
 * modified bit in the pte (the 88110 MMU doesn't do this and relies upon
 * the kernel to achieve this).
 * Returns TRUE if the page was indeed writeable but not marked as dirty,
 * FALSE if this is a genuine write fault.
 */
int
pmap_set_modify(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte;
	paddr_t pa;
	vm_page_t pg;
	pv_entry_t pvl;

	pte = pmap_pte(pmap, va);
#ifdef DEBUG
	if (pte == NULL)
		panic("NULL pte on write fault??");
#endif

	/* Not a first write to a writable page */
	if ((*pte & (PG_M | PG_RO)) != 0)
		return (FALSE);

	/* Mark the page as dirty */
	*pte |= PG_M;
	pa = *pte & PG_FRAME;
	pg = PHYS_TO_VM_PAGE(pa);
#ifdef DIAGNOSTIC
	if (pg == NULL)
		panic("Write fault to unmanaged page %p", pa);
#endif

	pvl = pg_to_pvh(pg);
	pvl->pv_flags |= PG_M_U;

	if (pmap == pmap_kernel())
		set_dcmd(CMMU_DCMD_INV_SATC);
	else
		set_dcmd(CMMU_DCMD_INV_UATC);

	return (TRUE);
}
#endif
