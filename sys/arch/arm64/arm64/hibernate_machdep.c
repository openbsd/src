/*	$OpenBSD: hibernate_machdep.c,v 1.3 2026/09/19 17:27:09 kettenis Exp $ */

/*
 * Copyright (c) 2026 Marcus Glocker <mglocker@openbsd.org>
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2012 Mike Larkin <mlarkin@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/hibernate.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/kcore.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_pmemrange.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/hibernate.h>
#include <machine/intr.h>
#include <machine/kcore.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <dev/ofw/fdt.h>

#include "sd.h"
#include "softraid.h"
#include "nvme.h"
#include "ufshci.h"

/* Hibernate support */
#define HIB_PTE_ATTRS	(ATTR_AF | ATTR_SH(SH_INNER) | ATTR_AP(0) | \
			    ATTR_nG | ATTR_IDX(PTE_ATTR_WB))
#define HIB_OA_MASK	0x0000fffffffff000ULL	/* output address [47:12] */

void	hibernate_enter_resume_4k_pte(vaddr_t, paddr_t);
void	hibernate_enter_resume_2m_block(vaddr_t, paddr_t);

extern	struct vm_physseg vm_physmem[];
extern	int vm_nphysseg;

/* Resume PT anchors, published by hibernate_populate_resume_pt() */
paddr_t hibernate_resume_ttbr0;
paddr_t hibernate_resume_ttbr1;
paddr_t hibernate_stack_top_va;

label_t hibernate_jmpbuf;

uint64_t hibernate_tcr;
uint64_t hibernate_ttbr0;
uint64_t hibernate_ttbr1;

/*
 * arm64 MD Hibernate functions
 *
 * see arm64 hibernate_var.h for the piglet layout used during hibernate
 */

/* Low-VA L2 pool: one page per distinct L1_LOW slot */
static int8_t	hibernate_l1_to_l2pool[VP_IDX1_CNT];
static int	hibernate_l2pool_next;

/* High-VA L2 pool: one page per distinct L1_HI slot */
static int8_t	hibernate_l1hi_to_l2pool[VP_IDX1_CNT];
static int	hibernate_l2hipool_next;
static int	hibernate_l0hi_slot;

/* Kernel-VA L2 pool: one page per distinct PT1_L1 slot */
static int8_t	hibernate_pt1_l1_to_l2pool[VP_IDX1_CNT];
static int	hibernate_pt1_l2pool_next;

static paddr_t
hibernate_get_l2_low_pa(int l1_idx)
{
	int slot;

	if (hibernate_l1_to_l2pool[l1_idx] < 0) {
		KASSERT(hibernate_l2pool_next < HIBERNATE_PT0_L2_LOW_POOL_COUNT);
		slot = hibernate_l2pool_next++;
		hibernate_l1_to_l2pool[l1_idx] = slot;
		bzero((caddr_t)(global_piglet_va +
		    HIBERNATE_PT0_L2_LOW_POOL + slot * PAGE_SIZE), PAGE_SIZE);
	}

	return global_piglet_pa + HIBERNATE_PT0_L2_LOW_POOL +
	    hibernate_l1_to_l2pool[l1_idx] * PAGE_SIZE;
}

static paddr_t
hibernate_get_l2_hi_pa(int l1_idx)
{
	int slot;

	if (hibernate_l1hi_to_l2pool[l1_idx] < 0) {
		KASSERT(hibernate_l2hipool_next < HIBERNATE_PT0_L2_HI_POOL_COUNT);
		slot = hibernate_l2hipool_next++;
		hibernate_l1hi_to_l2pool[l1_idx] = slot;
		bzero((caddr_t)(global_piglet_va +
		    HIBERNATE_PT0_L2_HI_POOL + slot * PAGE_SIZE), PAGE_SIZE);
	}

	return global_piglet_pa + HIBERNATE_PT0_L2_HI_POOL +
	    hibernate_l1hi_to_l2pool[l1_idx] * PAGE_SIZE;
}

static paddr_t
hibernate_get_pt1_l2_pa(int l1_idx)
{
	int slot;

	if (hibernate_pt1_l1_to_l2pool[l1_idx] < 0) {
		KASSERT(hibernate_pt1_l2pool_next < HIBERNATE_PT1_L2_POOL_COUNT);
		slot = hibernate_pt1_l2pool_next++;
		hibernate_pt1_l1_to_l2pool[l1_idx] = slot;
		bzero((caddr_t)(global_piglet_va +
		    HIBERNATE_PT1_L2_POOL + slot * PAGE_SIZE), PAGE_SIZE);
	}

	return global_piglet_pa + HIBERNATE_PT1_L2_POOL +
	    hibernate_pt1_l1_to_l2pool[l1_idx] * PAGE_SIZE;
}

/*
 * Returns the hibernate write I/O function to use on this machine
 */
hibio_fn
get_hibernate_io_function(dev_t dev)
{
	char *blkname = findblkname(major(dev));

	if (blkname == NULL)
		return NULL;

#if NSD > 0
	if (strcmp(blkname, "sd") == 0) {
		extern struct cfdriver sd_cd;
		extern int nvme_hibernate_io(dev_t dev, daddr_t blkno,
		    vaddr_t addr, size_t size, int op, void *page);
		extern int sr_hibernate_io(dev_t dev, daddr_t blkno,
		    vaddr_t addr, size_t size, int op, void *page);
		extern int ufshci_hibernate_io(dev_t dev, daddr_t blkno,
		    vaddr_t addr, size_t size, int op, void *page);
		struct device *dv = disk_lookup(&sd_cd, DISKUNIT(dev));
		struct {
			const char *driver;
			hibio_fn io_func;
		} sd_io_funcs[] = {
#if NNVME > 0
			{ "nvme", nvme_hibernate_io },
#endif
#if NSOFTRAID > 0
			{ "softraid", sr_hibernate_io },
#endif
#if NUFSHCI > 0
			{ "ufshci", ufshci_hibernate_io },
#endif
		};

		if (dv && dv->dv_parent && dv->dv_parent->dv_parent) {
			const char *driver = dv->dv_parent->dv_parent->
			    dv_cfdata->cf_driver->cd_name;
			int i;

			for (i = 0; i < nitems(sd_io_funcs); i++) {
				if (strcmp(driver, sd_io_funcs[i].driver) == 0)
					return sd_io_funcs[i].io_func;
			}
		}
	}
#endif /* NSD > 0 */
	return NULL;
}

/*
 * Gather MD-specific data and store into hiber_info
 */
int
get_hibernate_info_md(union hibernate_info *hiber_info)
{
	extern struct fdt_reg memreg[];
	extern int nmemreg;
	int i;

	if (nmemreg > nitems(hiber_info->ranges))
		return 1;

	/* Calculate memory ranges */
	hiber_info->nranges = nmemreg;
	hiber_info->image_size = 0;

	for (i = 0; i < nmemreg; i++) {
		hiber_info->ranges[i].base = memreg[i].addr;
		hiber_info->ranges[i].end = memreg[i].addr + memreg[i].size;
		hiber_info->image_size += memreg[i].size;
	}

	hibernate_sort_ranges(hiber_info);

	return 0;
}

/*
 * Enter a mapping for va->pa in the resume pagetable, using
 * the specified size.
 *
 * size : 0 if a 4KB mapping is desired
 *        1 if a 2MB mapping is desired
 */
void
hibernate_enter_resume_mapping(vaddr_t va, paddr_t pa, int size)
{
	/* HIB_SKIP bit-bucket: redirect to piglet */
	if (pa == HIBERNATE_INFLATE_PAGE)
		pa = global_piglet_pa + HIBERNATE_INFLATE_PAGE;

	if (size)
		return hibernate_enter_resume_2m_block(va, pa);
	else
		return hibernate_enter_resume_4k_pte(va, pa);
}

/*
 * Enter a 2MB block mapping for the supplied VA/PA into the resume-time pmap
 */
void
hibernate_enter_resume_2m_block(vaddr_t va, paddr_t pa)
{
	uint64_t *pte, npte;

	KASSERT((pa & ((1ULL << L2_SHIFT) - 1)) == 0);
	KASSERT(va < VM_MAX_KERNEL_ADDRESS);

	if (va >= VM_MIN_KERNEL_ADDRESS) {
		int l1_idx = (va >> L1_SHIFT) & (VP_IDX1_CNT - 1);
		paddr_t l2pool_pa = hibernate_get_pt1_l2_pa(l1_idx);
		vaddr_t l2pool_va = global_piglet_va +
		    HIBERNATE_PT1_L2_POOL +
		    hibernate_pt1_l1_to_l2pool[l1_idx] * PAGE_SIZE;

		pte = (uint64_t *)(global_piglet_va + HIBERNATE_PT1_L1_PAGE +
		    (l1_idx * sizeof(uint64_t)));
		npte = (l2pool_pa & HIB_OA_MASK) | L1_TABLE;
		*pte = 0;
		hibernate_flush();
		*pte = npte;

		pte = (uint64_t *)(l2pool_va +
		    (((va >> L2_SHIFT) & (VP_IDX2_CNT - 1)) *
		    sizeof(uint64_t)));
		npte = (pa & HIB_OA_MASK) | HIB_PTE_ATTRS | L2_BLOCK;
		*pte = 0;
		hibernate_flush();
		*pte = npte;
		return;
	}

	if (va < (1ULL << L0_SHIFT)) {
		if (va < (1ULL << L1_SHIFT)) {
			pte = (uint64_t *)(global_piglet_va +
			    HIBERNATE_PT0_L2_LOW +
			    (((va >> L2_SHIFT) & (VP_IDX2_CNT - 1)) *
			    sizeof(uint64_t)));
			npte = (pa & HIB_OA_MASK) | HIB_PTE_ATTRS | L2_BLOCK;
			*pte = 0;
			hibernate_flush();
			*pte = npte;
		} else {
			int l1_idx = (va >> L1_SHIFT) & (VP_IDX1_CNT - 1);
			paddr_t l2pool_pa = hibernate_get_l2_low_pa(l1_idx);
			vaddr_t l2pool_va = global_piglet_va +
			    HIBERNATE_PT0_L2_LOW_POOL +
			    hibernate_l1_to_l2pool[l1_idx] * PAGE_SIZE;

			pte = (uint64_t *)(global_piglet_va +
			    HIBERNATE_PT0_L1_LOW + l1_idx * sizeof(uint64_t));
			npte = (l2pool_pa & HIB_OA_MASK) | L1_TABLE;
			*pte = 0;
			hibernate_flush();
			*pte = npte;

			pte = (uint64_t *)(l2pool_va +
			    (((va >> L2_SHIFT) & (VP_IDX2_CNT - 1)) *
			    sizeof(uint64_t)));
			npte = (pa & HIB_OA_MASK) | HIB_PTE_ATTRS | L2_BLOCK;
			*pte = 0;
			hibernate_flush();
			*pte = npte;
		}
	} else {
		int l0_idx = (va >> L0_SHIFT) & (VP_IDX0_CNT - 1);
		int l1_idx = (va >> L1_SHIFT) & (VP_IDX1_CNT - 1);
		paddr_t l2pool_pa = hibernate_get_l2_hi_pa(l1_idx);
		vaddr_t l2pool_va = global_piglet_va +
		    HIBERNATE_PT0_L2_HI_POOL +
		    hibernate_l1hi_to_l2pool[l1_idx] * PAGE_SIZE;

		/*
		 * A single L1_HI page backs every L0 slot we install.
		 * Callers must share one 512GB granule (kernel VA and
		 * piglet VA both live in TTBR1 space above the kernel
		 * base); assert to catch that if it ever changes.
		 */
		if (hibernate_l0hi_slot < 0)
			hibernate_l0hi_slot = l0_idx;
		KASSERT(hibernate_l0hi_slot == l0_idx);

		pte = (uint64_t *)(global_piglet_va + HIBERNATE_PT0_L0_PAGE +
		    (l0_idx * sizeof(uint64_t)));
		npte = ((global_piglet_pa + HIBERNATE_PT0_L1_HI) & HIB_OA_MASK) | L0_TABLE;
		*pte = 0;
		hibernate_flush();
		*pte = npte;

		pte = (uint64_t *)(global_piglet_va + HIBERNATE_PT0_L1_HI +
		    (l1_idx * sizeof(uint64_t)));
		npte = (l2pool_pa & HIB_OA_MASK) | L1_TABLE;
		*pte = 0;
		hibernate_flush();
		*pte = npte;

		pte = (uint64_t *)(l2pool_va +
		    (((va >> L2_SHIFT) & (VP_IDX2_CNT - 1)) *
		    sizeof(uint64_t)));
		npte = (pa & HIB_OA_MASK) | HIB_PTE_ATTRS | L2_BLOCK;
		*pte = 0;
		hibernate_flush();
		*pte = npte;
	}
}

/*
 * Enter a 4KB PTE mapping for the supplied VA/PA into the resume-time pmap
 */
void
hibernate_enter_resume_4k_pte(vaddr_t va, paddr_t pa)
{
	uint64_t *pte, npte;

	/* Mappings entered here must be in the first 4MB VA */
	KASSERT(va < 2 * L2_SIZE);

	pte = (uint64_t *)(global_piglet_va + HIBERNATE_PT0_L3_LOW +
	    (((va >> PAGE_SHIFT) & (VP_IDX3_CNT - 1)) * sizeof(uint64_t)));

	if (va >= L2_SIZE)
		pte += VP_IDX3_CNT;

	npte = (pa & HIB_OA_MASK) | HIB_PTE_ATTRS | L3_P;
	*pte = 0;
	hibernate_flush();
	*pte = npte;
}

/*
 * Create the resume-time page table. This table maps the image (pig) area,
 * the kernel text area, and various utility pages for use during resume,
 * since we cannot overwrite the resuming kernel's page table during inflate
 * and expect things to work properly.
 */
void
hibernate_populate_resume_pt(union hibernate_info *hib_info,
    paddr_t image_start, paddr_t image_end)
{
	extern uint64_t pmap_avail_kvo;
	extern char __text_start[], _end[];
	vaddr_t kern_start_2m_va, kern_end_2m_va, va;
	vaddr_t piglet_va = hib_info->piglet_va;
	paddr_t piglet_pa = hib_info->piglet_pa;
	paddr_t piglet_start, piglet_end, pa;
	paddr_t pig_2m_start, pig_2m_end;
	uint64_t *pte;

	/*
	 * Resume stack at normal kernel VA (used before the resume
	 * page tables are activated)
	 */
	hibernate_stack_top_va = piglet_va + HIBERNATE_STACK_PAGE +
	    HIBERNATE_STACK_OFFSET;

	memset(hibernate_l1_to_l2pool, -1, sizeof(hibernate_l1_to_l2pool));
	hibernate_l2pool_next = 0;
	memset(hibernate_l1hi_to_l2pool, -1, sizeof(hibernate_l1hi_to_l2pool));
	hibernate_l2hipool_next = 0;
	hibernate_l0hi_slot = -1;
	memset(hibernate_pt1_l1_to_l2pool, -1, sizeof(hibernate_pt1_l1_to_l2pool));
	hibernate_pt1_l2pool_next = 0;

	bzero((caddr_t)(piglet_va + HIBERNATE_PT0_L0_PAGE), PAGE_SIZE);
	bzero((caddr_t)(piglet_va + HIBERNATE_PT0_L1_LOW), PAGE_SIZE);
	bzero((caddr_t)(piglet_va + HIBERNATE_PT0_L1_HI), PAGE_SIZE);
	bzero((caddr_t)(piglet_va + HIBERNATE_PT0_L2_LOW), PAGE_SIZE);
	bzero((caddr_t)(piglet_va + HIBERNATE_PT0_L3_LOW), 2 * PAGE_SIZE);
	bzero((caddr_t)(piglet_va + HIBERNATE_PT1_L1_PAGE), PAGE_SIZE);
	bzero((caddr_t)(piglet_va + HIBERNATE_STACK_PAGE - 3 * PAGE_SIZE),
	    3 * PAGE_SIZE);

	/* Root chain for the first 4MB of VA */
	pte = (uint64_t *)(piglet_va + HIBERNATE_PT0_L0_PAGE);
	pa = piglet_pa + HIBERNATE_PT0_L1_LOW;
	pte[0] = (pa & HIB_OA_MASK) | L0_TABLE;

	pte = (uint64_t *)(piglet_va + HIBERNATE_PT0_L1_LOW);
	pa = piglet_pa + HIBERNATE_PT0_L2_LOW;
	pte[0] = (pa & HIB_OA_MASK) | L1_TABLE;

	pte = (uint64_t *)(piglet_va + HIBERNATE_PT0_L2_LOW);
	pa = piglet_pa + HIBERNATE_PT0_L3_LOW;
	pte[0] = (pa & HIB_OA_MASK) | L2_TABLE;
	pa += PAGE_SIZE;
	pte[1] = (pa & HIB_OA_MASK) | L2_TABLE;

	hibernate_resume_ttbr0 = piglet_pa + HIBERNATE_PT0_L0_PAGE;
	hibernate_resume_ttbr1 = piglet_pa + HIBERNATE_PT1_L1_PAGE;
	
	/* Map kernel at normal kernel VA */
	kern_start_2m_va = (vaddr_t)__text_start & ~((1ULL << L2_SHIFT) - 1);
	kern_end_2m_va   = ((vaddr_t)_end + ((1ULL << L2_SHIFT) - 1)) &
	    ~((1ULL << L2_SHIFT) - 1);
	for (va = kern_start_2m_va; va < kern_end_2m_va;
	    va += (1ULL << L2_SHIFT)) {
		pa = va + pmap_avail_kvo;
		hibernate_enter_resume_mapping(va,
		    pa & ~((1ULL << L2_SHIFT) - 1), 1);
	}

	/* Identity-map piglet */
	piglet_start = piglet_pa;
	piglet_end = piglet_pa + HIBERNATE_CHUNK_SIZE * 4;
	for (pa = piglet_start; pa < piglet_end; pa += (1ULL << L2_SHIFT))
		hibernate_enter_resume_mapping(pa, pa, 1);

	/* Map piglet at normal kernel VA */
	for (va = piglet_va, pa = piglet_pa;
	     va < piglet_va + HIBERNATE_CHUNK_SIZE * 4;
	     va += (1ULL << L2_SHIFT), pa += (1ULL << L2_SHIFT))
		hibernate_enter_resume_mapping(va, pa, 1);

	/* Identity-map pig (compressed image); zlib dereferences PA as VA */
	pig_2m_start = image_start & ~((1ULL << L2_SHIFT) - 1);
	pig_2m_end = (image_end + ((1ULL << L2_SHIFT) - 1)) &
	    ~((1ULL << L2_SHIFT) - 1);
	for (pa = pig_2m_start; pa < pig_2m_end; pa += (1ULL << L2_SHIFT))
		hibernate_enter_resume_mapping(pa, pa, 1);

	__asm volatile ("dsb sy" ::: "memory");
}

/*
 * Decide how to handle an inflate destination PA.
 *
 * Return values:
 *  HIB_MOVE: divert the inflate write into the piglet retguard save
 *    slot (MI code sends it to piglet_pa + 110*PAGE_SIZE + retguard_ofs).
 *    hibernate_retguard_copy_machdep() restores it after unpack.
 *  HIB_SKIP: skip inflate for this PA (piglet region -- resumer must not
 *    clobber its own scratch state during unpack).
 *  0: inflate normally.
 */
int
hibernate_inflate_skip(union hibernate_info *hib_info, paddr_t dest)
{
	extern paddr_t retguard_start_phys, retguard_end_phys;
	extern paddr_t hibdata_start_phys, hibdata_end_phys;

	if (dest >= hib_info->piglet_pa &&
	    dest < (hib_info->piglet_pa + 4 * HIBERNATE_CHUNK_SIZE))
		return HIB_SKIP;

	/*
	 * Retguard region: cookies at these PAs encode the resumer's live
	 * call chain.  Diverting inflate writes into the piglet keeps them
	 * intact through unpack; hibernate_retguard_copy_machdep() writes
	 * the suspender snapshot back in one shot after the loop.
	 */
	if (dest >= retguard_start_phys && dest < retguard_end_phys)
		return HIB_MOVE;

	/*
	 * Hibernate data region: kernel data that needs to be preserved.
	 */
	if (dest >= hibdata_start_phys && dest < hibdata_end_phys)
		return HIB_SKIP;

	return 0;
}

void
hibernate_enable_intr_machdep(void)
{
	intr_enable();
}

void
hibernate_disable_intr_machdep(void)
{
	intr_disable();
}

#ifdef MULTIPROCESSOR
/*
 * Quiesce CPUs in a multiprocessor machine before resuming.
 */
void
hibernate_quiesce_cpus(void)
{
	extern volatile int cpu_parked;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	KASSERT(CPU_IS_PRIMARY(curcpu()));

	cpu_parked = 1;
	CPU_INFO_FOREACH(cii, ci) {
		if (CPU_IS_PRIMARY(ci))
			continue;
		if ((ci->ci_flags & CPUF_PRESENT) == 0)
			continue;
		atomic_setbits_int(&ci->ci_flags, CPUF_GO | CPUF_PARK);
		__asm volatile("dsb sy; sev" ::: "memory");
		while ((ci->ci_flags & CPUF_PARKED) == 0)
			CPU_BUSY_CYCLE();
	}

	/* Wait a bit for the APs to park themselves */
	delay(500000);
}
#endif /* MULTIPROCESSOR */

/* No-op on arm64: HIBERNATE_HIBALLOC_PAGE is a HIGH VA, mapped by uvm at boot */
int
hibernate_pmap_setup_md(void)
{
	return 0;
}

void
hibernate_pmap_teardown_md(void)
{
}
