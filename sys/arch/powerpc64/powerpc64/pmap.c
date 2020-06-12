/*	$OpenBSD: pmap.c,v 1.7 2020/06/12 22:01:01 gkoehler Exp $ */

/*
 * Copyright (c) 2015 Martin Pieuchot
 * Copyright (c) 2001, 2002, 2007 Dale Rahn.
 * All rights reserved.
 *
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 */

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/atomic.h>

#include <uvm/uvm_extern.h>

#include <machine/cpufunc.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <dev/ofw/fdt.h>

extern char _start[], _etext[], _end[];

#define	PMAP_HASH_LOCK_INIT()		/* nothing */
#define	PMAP_HASH_LOCK(s)		(void)s
#define	PMAP_HASH_UNLOCK(s)		/* nothing */

#define	PMAP_VP_LOCK_INIT(pm)		/* nothing */
#define	PMAP_VP_LOCK(pm)		/* nothing */
#define	PMAP_VP_UNLOCK(pm)		/* nothing */
#define	PMAP_VP_ASSERT_LOCKED(pm)	/* nothing */

struct pmap kernel_pmap_store;

struct pte *pmap_ptable;
int	pmap_ptab_cnt;
uint64_t pmap_ptab_mask;

#define HTABMEMSZ	(pmap_ptab_cnt * 8 * sizeof(struct pte))
#define HTABSIZE	(ffs(pmap_ptab_cnt) - 12)

struct pate *pmap_pat;

#define PATMEMSZ	(64 * 1024)
#define PATSIZE		(ffs(PATMEMSZ) - 12)

struct pte_desc {
	/* Linked list of phys -> virt entries */
	LIST_ENTRY(pte_desc) pted_pv_list;
	struct pte pted_pte;
	pmap_t pted_pmap;
	vaddr_t pted_va;
	uint64_t pted_vsid;
};

#define PTED_VA_PTEGIDX_M	0x07
#define PTED_VA_HID_M		0x08
#define PTED_VA_MANAGED_M	0x10
#define PTED_VA_WIRED_M		0x20
#define PTED_VA_EXEC_M		0x40

/*
 * We use only 4K pages and 256MB segments.  That means p = b = 12 and
 * s = 28.
 */

#define KERNEL_VSID_BIT		0x0000001000000000ULL
#define VSID_HASH_MASK		0x0000007fffffffffULL

static inline int
PTED_HID(struct pte_desc *pted)
{
	return !!(pted->pted_va & PTED_VA_HID_M); 
}

static inline int
PTED_PTEGIDX(struct pte_desc *pted)
{
	return !!(pted->pted_va & PTED_VA_PTEGIDX_M); 
}

static inline int
PTED_MANAGED(struct pte_desc *pted)
{
	return !!(pted->pted_va & PTED_VA_MANAGED_M); 
}

static inline int
PTED_VALID(struct pte_desc *pted)
{
	return !!(pted->pted_pte.pte_hi & PTE_VALID);
}

#define TLBIEL_MAX_SETS		4096
#define TLBIEL_SET_SHIFT	12
#define TLBIEL_INVAL_SET	(0x3 << 10)

void
tlbia(void)
{
	int set;

	for (set = 0; set < TLBIEL_MAX_SETS; set++)
		tlbiel((set << TLBIEL_SET_SHIFT) | TLBIEL_INVAL_SET);
}

/*
 * Return the lowest 64 bits of the VPN for a PTE descriptor.
 */
static inline uint64_t
pmap_pted2vpn(struct pte_desc *pted)
{
	return (pted->pted_vsid << (ADDR_VSID_SHIFT - PAGE_SHIFT) |
	    (pted->pted_va & ADDR_PIDX) >> PAGE_SHIFT);
}

/*
 * Return the top 64 bits of the (80-bit) VPN for a PTE descriptor.
 */
static inline uint64_t
pmap_pted2avpn(struct pte_desc *pted)
{
	return (pted->pted_vsid << (PTE_VSID_SHIFT) |
	    (pted->pted_va & ADDR_PIDX) >>
		(ADDR_VSID_SHIFT - PTE_VSID_SHIFT));
}

static inline u_int
pmap_pte2flags(uint64_t pte_lo)
{
	return (((pte_lo & PTE_REF) ? PG_PMAP_REF : 0) |
	    ((pte_lo & PTE_CHG) ? PG_PMAP_MOD : 0));
}

static inline uint64_t
pmap_kernel_vsid(uint64_t esid)
{
	uint64_t vsid;
	vsid = (((esid << 8) | (esid > 28)) * 0x13bb) & (KERNEL_VSID_BIT - 1);
	return vsid | KERNEL_VSID_BIT;
}

static inline uint64_t
pmap_va2vsid(pmap_t pm, vaddr_t va)
{
	uint64_t esid = va >> ADDR_ESID_SHIFT;

	if (pm == pmap_kernel())
		return pmap_kernel_vsid(esid);
	panic("userland");
}

void
pmap_attr_save(paddr_t pa, uint64_t bits)
{
	struct vm_page *pg;

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		return;

	atomic_setbits_int(&pg->pg_flags,  pmap_pte2flags(bits));
}

struct pte *
pmap_ptedinhash(struct pte_desc *pted)
{
	struct pte *pte;
	vaddr_t va;
	uint64_t vsid, hash;
	int idx;

	va = pted->pted_va & ~PAGE_MASK;
	vsid = pted->pted_vsid;
	hash = (vsid & VSID_HASH_MASK) ^ ((va & ADDR_PIDX) >> ADDR_PIDX_SHIFT);
	idx = (hash & pmap_ptab_mask);

	idx ^= (PTED_HID(pted) ? pmap_ptab_mask : 0);
	pte = pmap_ptable + (idx * 8);
	pte += PTED_PTEGIDX(pted); /* increment by index into pteg */

	/*
	 * We now have the pointer to where it will be, if it is
	 * currently mapped. If the mapping was thrown away in
	 * exchange for another page mapping, then this page is not
	 * currently in the hash.
	 */
	if ((pted->pted_pte.pte_hi |
	     (PTED_HID(pted) ? PTE_HID : 0)) == pte->pte_hi)
		return pte;

	return NULL;
}

/*
 * VP routines, virtual to physical translation information.
 * These data structures are based off of the pmap, per process.
 */

struct pte_desc *
pmap_vp_lookup(pmap_t pm, vaddr_t va)
{
	/* XXX Hack to bypass this for kernel pmap. */
	static struct pte_desc pted;

	memset(&pted, 0, sizeof(pted));
	return &pted;
}

/*
 * Delete a Page Table Entry, section 5.10.1.3.
 *
 * Note: hash table must be locked.
 */
void
pte_del(struct pte *pte, uint64_t vpn)
{
	pte->pte_hi &= ~PTE_VALID;
	ptesync();	/* Ensure update completed. */
	tlbie(vpn);	/* Invalidate old translation. */
	eieio();	/* Order tlbie before tlbsync. */
	tlbsync();	/* Ensure tlbie completed on all processors. */
	ptesync();	/* Ensure tlbsync and update completed. */
}

void
pte_zap(struct pte *pte, struct pte_desc *pted)
{
	pte_del(pte, pmap_pted2vpn(pted) << PAGE_SHIFT);

	if (!PTED_MANAGED(pted))
		return;

	pmap_attr_save(pted->pted_pte.pte_lo & PTE_RPGN,
	    pte->pte_lo & (PTE_REF|PTE_CHG));
}

void
pmap_fill_pte(pmap_t pm, vaddr_t va, paddr_t pa, struct pte_desc *pted,
    vm_prot_t prot, int cache)
{
	struct pte *pte = &pted->pted_pte;

	pted->pted_pmap = pm;
	pted->pted_va = va & ~PAGE_MASK;
	pted->pted_vsid = pmap_va2vsid(pm, va);

	pte->pte_hi = (pmap_pted2avpn(pted) & PTE_AVPN);
	pte->pte_lo = (pa & PTE_RPGN);

	if (prot & PROT_WRITE)
		pte->pte_lo |= PTE_RW;
	else
		pte->pte_lo |= PTE_RO;
	if (prot & PROT_EXEC)
		pted->pted_va |= PTED_VA_EXEC_M;
	else
		pte->pte_lo |= PTE_N;

	if (cache == PMAP_CACHE_WB)
		pte->pte_lo |= PTE_M;
	else
		pte->pte_lo |= (PTE_M | PTE_I | PTE_G);
}

void
pte_insert(struct pte_desc *pted)
{
	struct pte *pte;
	vaddr_t va;
	uint64_t vsid, hash;
	int off, idx, i;
	int s;

	PMAP_HASH_LOCK(s);

	if ((pte = pmap_ptedinhash(pted)) != NULL)
		pte_zap(pte, pted);

	pted->pted_va &= ~(PTED_VA_HID_M|PTED_VA_PTEGIDX_M);

	va = pted->pted_va & ~PAGE_MASK;
	vsid = pted->pted_vsid;
	hash = (vsid & VSID_HASH_MASK) ^ ((va & ADDR_PIDX) >> ADDR_PIDX_SHIFT);
	idx = (hash & pmap_ptab_mask);

	/*
	 * instead of starting at the beginning of each pteg,
	 * the code should pick a random location with in the primary
	 * then search all of the entries, then if not yet found,
	 * do the same for the secondary.
	 * this would reduce the frontloading of the pteg.
	 */

	/* first just try fill of primary hash */
	pte = pmap_ptable + (idx * 8);
	for (i = 0; i < 8; i++) {
		if (pte[i].pte_hi & PTE_VALID)
			continue;

		pted->pted_va |= i;

		/* Add a Page Table Entry, section 5.10.1.1. */
		pte[i].pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;
		pte[i].pte_lo = pted->pted_pte.pte_lo;
		eieio();	/* Order 1st PTE update before 2nd. */
		pte[i].pte_hi |= PTE_VALID;
		ptesync();	/* Ensure updates completed. */

		if (i > 1)
			printf("%s: primary %d\n", __func__, i);
		goto out;
	}

	/* try fill of secondary hash */
	pte = pmap_ptable + (idx ^ pmap_ptab_mask) * 8;
	for (i = 0; i < 8; i++) {
		if (pte[i].pte_hi & PTE_VALID)
			continue;

		pted->pted_va |= (i | PTED_VA_HID_M);

		/* Add a Page Table Entry, section 5.10.1.1. */
		pte[i].pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;
		pte[i].pte_lo = pted->pted_pte.pte_lo;
		eieio();	/* Order 1st PTE update before 2nd. */
		pte[i].pte_hi |= (PTE_HID|PTE_VALID);
		ptesync();	/* Ensure updates completed. */

		printf("%s: secondary %d\n", __func__, i);
		goto out;
	}

	printf("%s: replacing!\n", __func__);

	/* need decent replacement algorithm */
	off = mftb();
	pted->pted_va |= off & (PTED_VA_PTEGIDX_M|PTED_VA_HID_M);

	idx ^= (PTED_HID(pted) ? pmap_ptab_mask : 0);
	pte = pmap_ptable + (idx * 8);
	pte += PTED_PTEGIDX(pted); /* increment by index into pteg */

	if (pte->pte_hi & PTE_VALID) {
		uint64_t avpn, vpn;

		avpn = pte->pte_hi & PTE_AVPN;
		vsid = avpn >> PTE_VSID_SHIFT;
		vpn = avpn << (ADDR_VSID_SHIFT - PTE_VSID_SHIFT - PAGE_SHIFT);

		idx ^= ((pte->pte_hi & PTE_HID) ? pmap_ptab_mask : 0);
		vpn |= ((idx ^ vsid) & (ADDR_PIDX >> ADDR_PIDX_SHIFT));

		pte_del(pte, vpn << PAGE_SHIFT);

		pmap_attr_save(pte->pte_lo & PTE_RPGN,
		    pte->pte_lo & (PTE_REF|PTE_CHG));
	}

	/* Add a Page Table Entry, section 5.10.1.1. */
	pte->pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;
	if (PTED_HID(pted))
		pte->pte_hi |= PTE_HID;
	pte->pte_lo = pted->pted_pte.pte_lo;
	eieio();	/* Order 1st PTE update before 2nd. */
	pte->pte_hi |= PTE_VALID;
	ptesync();	/* Ensure updates completed. */

out:
	PMAP_HASH_UNLOCK(s);
}

void
pmap_remove_pted(pmap_t pm, struct pte_desc *pted)
{
	struct pte *pte;
	int s;

	KASSERT(pm == pted->pted_pmap);
	PMAP_VP_ASSERT_LOCKED(pm);

	pm->pm_stats.resident_count--;

	PMAP_HASH_LOCK(s);
	if ((pte = pmap_ptedinhash(pted)) != NULL)
		pte_zap(pte, pted);
	PMAP_HASH_UNLOCK(s);

	pted->pted_va &= ~PTED_VA_EXEC_M;
	pted->pted_pte.pte_hi &= ~PTE_VALID;

#ifdef notyet
	if (PTED_MANAGED(pted))
		pmap_remove_pv(pted);

	if (pm != pmap_kernel()) {
		pmap_vp_remove(pm, pted->pted_va);
		pool_put(&pmap_pted_pool, pted);
	}
#endif
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pmap_t pm = pmap_kernel();
	struct pte_desc *pted;
	struct vm_page *pg;
	int cache = (pa & PMAP_NOCACHE) ? PMAP_CACHE_CI : PMAP_CACHE_WB;

	pted = pmap_vp_lookup(pm, va);
	KASSERT(pted);

	if (PTED_VALID(pted))
		pmap_remove_pted(pm, pted); /* pted is reused */

	pm->pm_stats.resident_count++;

	if (prot & PROT_WRITE) {
		pg = PHYS_TO_VM_PAGE(pa);
		if (pg != NULL)
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
	}

	/* Calculate PTE */
	pmap_fill_pte(pm, va, pa, pted, prot, cache);
	pted->pted_va |= PTED_VA_WIRED_M;

	/* Insert into HTAB */
	pte_insert(pted);
}

extern struct fdt_reg memreg[];
extern int nmemreg;

#ifdef DDB
extern struct fdt_reg initrd_reg;
#endif

void memreg_add(const struct fdt_reg *);
void memreg_remove(const struct fdt_reg *);

vaddr_t zero_page;
vaddr_t copy_src_page;
vaddr_t copy_dst_page;
vaddr_t virtual_avail = VM_MIN_KERNEL_ADDRESS;

void *
pmap_steal_avail(size_t size, size_t align)
{
	struct fdt_reg reg;
	uint64_t start, end;
	int i;

	for (i = 0; i < nmemreg; i++) {
		if (memreg[i].size > size) {
			start = (memreg[i].addr + (align - 1)) & ~(align - 1);
			end = start + size;
			if (end <= memreg[i].addr + memreg[i].size) {
				reg.addr = start;
				reg.size = end - start;
				memreg_remove(&reg);
				return (void *)start;
			}
		}
	}
	panic("can't allocate");
}

void
pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{
	*start = virtual_avail;
	*end = VM_MAX_KERNEL_ADDRESS;
}

pmap_t
pmap_create(void)
{
	printf("%s", __func__);
	return NULL;
}

void
pmap_reference(pmap_t pm)
{
}

void
pmap_destroy(pmap_t pm)
{
	panic(__func__);
}

void
pmap_init(void)
{
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr,
    vsize_t len, vaddr_t src_addr)
{
	panic(__func__);
}

int
pmap_enter(pmap_t pm, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	KASSERT(pm == pmap_kernel());
	pmap_kenter_pa(va, pa, prot);
	return 0;
}

void
pmap_remove(pmap_t pm, vaddr_t sva, vaddr_t eva)
{
	KASSERT(pm == pmap_kernel());
	pmap_kremove(sva, eva - sva);
}

void
pmap_protect(pmap_t pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	panic(__func__);
}

void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	panic(__func__);
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	vaddr_t eva = va + len;
	struct pte_desc pted;
	struct pte *pte;
	int s;

	while (va < eva) {
		pmap_fill_pte(pmap_kernel(), va, 0, &pted, 0, 0);
		pted.pted_pte.pte_hi |= PTE_VALID;

		PMAP_HASH_LOCK(s);
		if ((pte = pmap_ptedinhash(&pted)) != NULL)
			pte_zap(pte, &pted);
		PMAP_HASH_UNLOCK(s);

		va += PAGE_SIZE;
	}
}

int
pmap_is_referenced(struct vm_page *pg)
{
	return 0;
}

int
pmap_is_modified(struct vm_page *pg)
{
	return 0;
}

int
pmap_clear_reference(struct vm_page *pg)
{
	return 0;
}

int
pmap_clear_modify(struct vm_page *pg)
{
	return 0;
}

int
pmap_extract(pmap_t pm, vaddr_t va, paddr_t *pa)
{
	struct pte_desc pted;
	struct pte *pte;
	int s;

	if (pm == pmap_kernel() &&
	    va >= (vaddr_t)_start && va < (vaddr_t)_end) {
		*pa = va;
		return 1;
	}

	pmap_fill_pte(pm, va, 0, &pted, 0, 0);
	pted.pted_pte.pte_hi |= PTE_VALID;

	PMAP_HASH_LOCK(s);
	if ((pte = pmap_ptedinhash(&pted)) != NULL)
		*pa = (pte->pte_lo & PTE_RPGN) | (va & PAGE_MASK);
	PMAP_HASH_UNLOCK(s);

	return (pte != NULL);
}

void
pmap_activate(struct proc *p)
{
}

void
pmap_deactivate(struct proc *p)
{
}

void
pmap_unwire(pmap_t pm, vaddr_t va)
{
}

void
pmap_collect(pmap_t pm)
{
}

void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	paddr_t va = zero_page + cpu_number() * PAGE_SIZE;

	pmap_kenter_pa(va, pa, PROT_READ | PROT_WRITE);
	memset((void *)va, 0, PAGE_SIZE);
	pmap_kremove(va, PAGE_SIZE);
}

void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t srcpa = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dstpa = VM_PAGE_TO_PHYS(dstpg);
	vaddr_t srcva = copy_src_page + cpu_number() * PAGE_SIZE;
	vaddr_t dstva = copy_dst_page + cpu_number() * PAGE_SIZE;

	pmap_kenter_pa(srcva, srcpa, PROT_READ);
	pmap_kenter_pa(dstva, dstpa, PROT_READ | PROT_WRITE);
	memcpy((void *)dstva, (void *)srcva, PAGE_SIZE);
	pmap_kremove(srcva, PAGE_SIZE);
	pmap_kremove(dstva, PAGE_SIZE);
}

void
pmap_proc_iflush(struct process *pr, vaddr_t va, vsize_t len)
{
	panic(__func__);
}

void
pmap_bootstrap(void)
{
	uint64_t esid, slbe, slbv;
	paddr_t start, end, pa;
	vaddr_t va;
	vm_prot_t prot;
	int idx = 0;

	/* Clear SLB. */
	slbia();
	slbie(slbmfee(0));

	/* Clear TLB. */
	tlbia();

#define HTABENTS 2048

	pmap_ptab_cnt = HTABENTS;
	while (pmap_ptab_cnt * 2 < physmem)
		pmap_ptab_cnt <<= 1;

	/*
	 * allocate suitably aligned memory for HTAB
	 */
	pmap_ptable = pmap_steal_avail(HTABMEMSZ, HTABMEMSZ);
	memset(pmap_ptable, 0, HTABMEMSZ);
	pmap_ptab_mask = pmap_ptab_cnt - 1;

	/* Map page tables. */
	start = (paddr_t)pmap_ptable;
	end = start + HTABMEMSZ;
	for (pa = start; pa < end; pa += PAGE_SIZE)
		pmap_kenter_pa(pa, pa, PROT_READ | PROT_WRITE);

	/* Map kernel. */
	start = (paddr_t)_start;
	end = (paddr_t)_end;
	for (pa = start; pa < end; pa += PAGE_SIZE) {
		if (pa < (paddr_t)_etext)
			prot = PROT_READ | PROT_EXEC;
		else
			prot = PROT_READ | PROT_WRITE;
		pmap_kenter_pa(pa, pa, prot);
	}

#ifdef DDB
	/* Map initrd. */
	start = initrd_reg.addr;
	end = initrd_reg.addr + initrd_reg.size;
	for (pa = start; pa < end; pa += PAGE_SIZE)
		pmap_kenter_pa(pa, pa, PROT_READ | PROT_WRITE);
#endif

	/* Allocate partition table. */
	pmap_pat = pmap_steal_avail(PATMEMSZ, PATMEMSZ);
	memset(pmap_pat, 0, PATMEMSZ);
	pmap_pat[0].pate_htab = (paddr_t)pmap_ptable | HTABSIZE;
	mtptcr((paddr_t)pmap_pat | PATSIZE);

	/* SLB entry for the kernel. */
	esid = (vaddr_t)_start >> ADDR_ESID_SHIFT;
	slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID | idx++;
	slbv = pmap_kernel_vsid(esid) << SLBV_VSID_SHIFT;
	slbmte(slbv, slbe);

	/* SLB entry for the page tables. */
	esid = (vaddr_t)pmap_ptable >> ADDR_ESID_SHIFT;
	slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID | idx++;
	slbv = pmap_kernel_vsid(esid) << SLBV_VSID_SHIFT;
	slbmte(slbv, slbe);

	/* SLB entries for kernel VA. */
	for (va = VM_MIN_KERNEL_ADDRESS; va < VM_MAX_KERNEL_ADDRESS;
	     va += 256 * 1024 * 1024) {
		esid = va >> ADDR_ESID_SHIFT;
		slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID | idx++;
		slbv = pmap_kernel_vsid(esid) << SLBV_VSID_SHIFT;
		slbmte(slbv, slbe);
	}

	zero_page = virtual_avail;
	virtual_avail += MAXCPUS * PAGE_SIZE;
	copy_src_page = virtual_avail;
	virtual_avail += MAXCPUS * PAGE_SIZE;
	copy_dst_page = virtual_avail;
	virtual_avail += MAXCPUS * PAGE_SIZE;
}

#ifdef DDB
/*
 * DDB will edit the PTE to gain temporary write access to a page in
 * the read-only kernel text.
 */
struct pte *
pmap_get_kernel_pte(vaddr_t va)
{
	struct pte_desc pted;

	pmap_fill_pte(pmap_kernel(), va, 0, &pted, 0, 0);
	pted.pted_pte.pte_hi |= PTE_VALID;
	return pmap_ptedinhash(&pted);
}
#endif
