/*	$OpenBSD: pmap.c,v 1.58 2002/01/25 04:04:55 drahn Exp $	*/
/*	$NetBSD: pmap.c,v 1.1 1996/09/30 16:34:52 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/pool.h>

#include <uvm/uvm.h>

#include <machine/pcb.h>
#include <machine/powerpc.h>

pte_t *ptable;
int ptab_cnt;
u_int ptab_mask;
#define	HTABSIZE	(ptab_cnt * 64)

struct pte_ovfl {
	LIST_ENTRY(pte_ovfl) po_list;	/* Linked list of overflow entries */
	struct pte po_pte;		/* PTE for this mapping */
};

LIST_HEAD(pte_ovtab, pte_ovfl) *potable; /* Overflow entries for ptable */

/* free lists for potable entries, it is not valid to pool_put
 * in the pte spill handler.
 * pofreebusy variable is a flag to indicate that the 
 * higher level is maniuplating list0 and that spill cannot touch
 * it, the higher level can only be touching one at a time.
 * if list0 is busy list1 cannot be busy.
 */
LIST_HEAD(,pte_ovfl) pofreetable0 = LIST_HEAD_INITIALIZER(pofreetable0);
LIST_HEAD(,pte_ovfl) pofreetable1 = LIST_HEAD_INITIALIZER(pofreetable1);
volatile int pofreebusy;

struct pmap kernel_pmap_;

int physmem;
static int npgs;
static u_int nextavail;

static struct mem_region *mem, *avail;
/*
#define USE_PMAP_VP
*/

#if 0
void
dump_avail()
{
	int cnt;
	struct mem_region *mp;
	extern struct mem_region *avail;
	
	printf("memory %x\n", mem);
	for (cnt = 0, mp = mem; mp->size; mp++) {
		printf("memory region %x: start %x, size %x\n",
				cnt, mp->start, mp->size);
		cnt++;
	}
	printf("available %x\n", avail);
	for (cnt = 0, mp = avail; mp->size; mp++) {
		printf("avail region %x: start %x, size %x\n",
				cnt, mp->start, mp->size);
		cnt++;
	}
}
#endif

#ifdef USE_PMAP_VP
struct pool pmap_vp_pool;

int pmap_vp_valid(pmap_t pm, vaddr_t va);
void pmap_vp_enter(pmap_t pm, vaddr_t va, paddr_t pa);
int pmap_vp_remove(pmap_t pm, vaddr_t va);
void pmap_vp_destroy(pmap_t pm);

/* virtual to physical map */
static inline int
VP_SR(paddr_t va)
{
	return (va >> VP_SR_POS) & VP_SR_MASK;
}
static inline int
VP_IDX1(paddr_t va)
{
	return (va >> VP_IDX1_POS) & VP_IDX1_MASK;
}

static inline int
VP_IDX2(paddr_t va)
{
	return (va >> VP_IDX2_POS) & VP_IDX2_MASK;
}

int
pmap_vp_valid(pmap_t pm, vaddr_t va)
{
	pmapv_t *vp1;
	vp1 = pm->vps[VP_SR(va)];
	if (vp1 != NULL) {
		return (vp1[VP_IDX1(va)] & (1 << VP_IDX2(va)));
	}
	return 0;
}

int
pmap_vp_remove(pmap_t pm, vaddr_t va)
{
	pmapv_t *vp1;
	int s;
	int retcode;
	retcode = 0;
	vp1 = pm->vps[VP_SR(va)];
#ifdef DEBUG
	printf("pmap_vp_remove: removing va %x pm %x", va, pm);
#endif
	if (vp1 != NULL) {
		s = splhigh();
		retcode = vp1[VP_IDX1(va)] & (1 << VP_IDX2(va));
		vp1[VP_IDX1(va)] &= ~(1 << VP_IDX2(va));
		splx(s);
	}
#ifdef DEBUG
	printf(" ret %x\n", retcode);
#endif
	return retcode;
}
void
pmap_vp_enter(pmap_t pm, vaddr_t va, paddr_t pa)
{
	pmapv_t *vp1;
	pmapv_t *mem1;
	int s;
	int idx;
	idx = VP_SR(va);
	vp1 = pm->vps[idx];
#ifdef DEBUG
	printf("pmap_vp_enter: pm %x va %x vp1 %x idx %x ", pm, va, vp1, idx);
#endif
	if (vp1 == NULL) {
#ifdef DEBUG
		printf("l1 entry idx %x ", idx);
#endif
		if (pm == pmap_kernel()) {
			printf(" irk kernel allocating map?");
		}
		s = splimp();
		mem1 = pool_get(&pmap_vp_pool, PR_NOWAIT);
		splx(s);

		bzero (mem1, PAGE_SIZE);

		pm->vps[idx] = mem1;
#ifdef DEBUG
		printf("got %x ", mem1);
#endif
		vp1 = mem1;
	}
#ifdef DEBUG
	printf("l2 idx %x\n", VP_IDX2(va));
#endif

	s = splhigh();
	vp1[VP_IDX1(va)] |= (1 << VP_IDX2(va));
	splx(s);
	return;
}

void
pmap_vp_destroy(pm)
	pmap_t pm;
{
	pmapv_t *vp1;
	int s;
	int sr;
#ifdef SANITY
	int idx1;
#endif

	for (sr = 0; sr < 32; sr++) {
		vp1 = pm->vps[sr];
		if (vp1 == NULL) {
			continue;
		}
#ifdef SANITY
		for(idx1 = 0; idx1 < 1024; idx1++) {
			if (vp1[idx1] != 0) {
				printf("mapped page at %x \n"
					0); /* XXX what page was this... */
				vp1[idx2] = 0;

			}
		}
#endif
		s = splimp();
		pool_put(&pmap_vp_pool, vp1);
		splx(s);
		pm->vps[sr] = 0;
	}
}
static int vp_page0[1024];
static int vp_page1[1024];
void pmap_vp_preinit(void);

void
pmap_vp_preinit()
{
	pmap_t pm = pmap_kernel();
	/* magic addresses are 0xe0000000, 0xe8000000 */
	pm->vps[VP_SR(0xe0000000)] = vp_page0;
	pm->vps[VP_SR(0xe8000000)] = vp_page1;
}
#endif


/*
 * This is a cache of referenced/modified bits.
 * Bits herein are shifted by ATTRSHFT.
 */
static char *pmap_attrib;
#define	ATTRSHFT	4

struct pv_entry {
	struct pv_entry *pv_next;	/* Linked list of mappings */
	int pv_idx;			/* Index into ptable */
	vm_offset_t pv_va;		/* virtual address of mapping */
	struct pmap *pv_pmap;		/* pmap associated with this map */
};

struct pool pmap_pv_pool;
struct pv_entry *pmap_alloc_pv __P((void));
void pmap_free_pv __P((struct pv_entry *));

struct pool pmap_po_pool;
struct pte_ovfl *poalloc __P((void));
void pofree __P((struct pte_ovfl *, int));

static u_int usedsr[NPMAPS / sizeof(u_int) / 8];

static int pmap_initialized;

static inline void tlbie(vm_offset_t ea);
static inline void tlbsync(void);
static inline void tlbia(void);
static inline int ptesr(sr_t *sr, vm_offset_t addr);
static inline int pteidx(sr_t sr, vm_offset_t addr);
static inline int ptematch( pte_t *ptp, sr_t sr, vm_offset_t va, int which);
int pte_insert(int idx, pte_t *pt);
int pte_spill(vm_offset_t addr);
int pmap_page_index(vm_offset_t pa);
u_int pmap_free_pages(void);
int pmap_next_page(vm_offset_t *paddr);
struct pv_entry *pmap_alloc_pv(void);
void pmap_free_pv(struct pv_entry *pv);
struct pte_ovfl *poalloc(void);
static inline int pmap_enter_pv(struct pmap *pm, int pteidx, vm_offset_t va,
	vm_offset_t pa);
void pmap_remove_pv(struct pmap *pm, int pteidx, vm_offset_t va,
	u_int32_t pte_lo);
pte_t * pte_find(struct pmap *pm, vm_offset_t va);

void addbatmap(u_int32_t vaddr, u_int32_t raddr, u_int32_t wimg);

/*
 * These small routines may have to be replaced,
 * if/when we support processors other that the 604.
 */
static inline void
tlbie(vm_offset_t ea)
{
	asm volatile ("tlbie %0" :: "r"(ea));
}

static inline void
tlbsync()
{
	asm volatile ("sync; tlbsync; sync");
}

static void
tlbia()
{
	vm_offset_t i;
	
	asm volatile ("sync");
	for (i = 0; i < 0x00040000; i += 0x00001000)
		tlbie(i);
	tlbsync();
}

static inline int
ptesr(sr, addr)
	sr_t *sr;
	vm_offset_t addr;
{
	return sr[(u_int)addr >> ADDR_SR_SHFT];
}

static inline int
pteidx(sr, addr)
	sr_t sr;
	vm_offset_t addr;
{
	int hash;
	
	hash = (sr & SR_VSID) ^ (((u_int)addr & ADDR_PIDX) >> ADDR_PIDX_SHFT);
	return hash & ptab_mask;
}

static inline int
ptematch(ptp, sr, va, which)
	pte_t *ptp;
	sr_t sr;
	vm_offset_t va;
	int which;
{
	return ptp->pte_hi
		== (((sr & SR_VSID) << PTE_VSID_SHFT)
		    | (((u_int)va >> ADDR_API_SHFT) & PTE_API)
		    | which);
}

/*
 * Try to insert page table entry *pt into the ptable at idx.
 *
 * Note: *pt mustn't have PTE_VALID set.
 * This is done here as required by Book III, 4.12.
 */
int
pte_insert(idx, pt)
	int idx;
	pte_t *pt;
{
	pte_t *ptp;
	int i;
	
	/*
	 * First try primary hash.
	 */
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
		if (!(ptp->pte_hi & PTE_VALID)) {
			*ptp = *pt;
			ptp->pte_hi &= ~PTE_HID;
			asm volatile ("sync");
			ptp->pte_hi |= PTE_VALID;
			return 1;
		}
	idx ^= ptab_mask;
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
		if (!(ptp->pte_hi & PTE_VALID)) {
			*ptp = *pt;
			ptp->pte_hi |= PTE_HID;
			asm volatile ("sync");
			ptp->pte_hi |= PTE_VALID;
			return 1;
		}
	return 0;
}

/*
 * Spill handler.
 *
 * Tries to spill a page table entry from the overflow area.
 * Note that this routine runs in real mode on a separate stack,
 * with interrupts disabled.
 */
int
pte_spill(addr)
	vm_offset_t addr;
{
	int idx, i;
	sr_t sr;
	struct pte_ovfl *po;
	pte_t ps;
	pte_t *pt;
	vm_offset_t va;

	asm ("mfsrin %0,%1" : "=r"(sr) : "r"(addr));
	idx = pteidx(sr, addr);
	for (po = potable[idx].lh_first; po; po = po->po_list.le_next)
		if (ptematch(&po->po_pte, sr, addr, 0)) {
			/*
			 * Now found an entry to be spilled into the real ptable.
			 */
			if (pte_insert(idx, &po->po_pte)) {
				LIST_REMOVE(po, po_list);
				pofree(po, 0);
				return 1;
			}
			/*
			 * Have to substitute some entry. Use the primary hash for this.
			 *
			 * Use low bits of timebase as random generator
			 */
			asm ("mftb %0" : "=r"(i));
			pt = ptable + idx * 8 + (i & 7);
			pt->pte_hi &= ~PTE_VALID;
			ps = *pt;
			asm volatile ("sync");
			/* calculate the va of the address being removed */
			va = ((pt->pte_hi & PTE_API) << ADDR_API_SHFT) |
			    ((((pt->pte_hi >> PTE_VSID_SHFT) & SR_VSID)
				^(idx ^ ((pt->pte_hi & PTE_HID) ? 0x3ff : 0)))
				    & 0x3ff) << PAGE_SHIFT;
			tlbie(va);
			tlbsync();
			*pt = po->po_pte;
			asm volatile ("sync");
			pt->pte_hi |= PTE_VALID;
			po->po_pte = ps;
			if (ps.pte_hi & PTE_HID) {
				/*
				 * We took an entry that was on the alternate hash
				 * chain, so move it to it's original chain.
				 */
				po->po_pte.pte_hi &= ~PTE_HID;
				LIST_REMOVE(po, po_list);
				LIST_INSERT_HEAD(potable + (idx ^ ptab_mask),
						 po, po_list);
			}
			return 1;
		}
	return 0;
}

int avail_start;
int avail_end;
/*
 * This is called during initppc, before the system is really initialized.
 */
void
pmap_bootstrap(kernelstart, kernelend)
	u_int kernelstart, kernelend;
{
	struct mem_region *mp, *mp1;
	int cnt, i;
	u_int s, sz;

	/*
	 * Get memory.
	 */
	(fw->mem_regions)(&mem, &avail);
	physmem = 0;
	for (mp = mem; mp->size; mp++) {
		physmem += btoc(mp->size);
	}

	/*
	 * Count the number of available entries.
	 */
	for (cnt = 0, mp = avail; mp->size; mp++)
		cnt++;

	/*
	 * Page align all regions.
	 * Non-page memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	kernelstart &= ~PGOFSET;
	kernelend = (kernelend + PGOFSET) & ~PGOFSET;

	/* make certain that each section is page aligned for base and size */
	for (mp = avail; mp->size; mp++) {
		u_int32_t end;
		s = mp->start - round_page(mp->start);
		if (s != 0) {
			mp->start = round_page(mp->start);
			end = trunc_page(mp->size + mp->start);
			mp->size = end - mp->start;
		}
		mp->size = trunc_page(mp->size);
	}
	for (mp = avail; mp->size; mp++) {
		/*
		 * Check whether this region holds all of the kernel.
		 */
		s = mp->start + mp->size;
		if (mp->start < kernelstart && s > kernelend) {
			avail[cnt].start = kernelend;
			avail[cnt++].size = s - kernelend;
			mp->size = kernelstart - mp->start;
		}
		/*
		 * Look whether this regions starts within the kernel.
		 */
		if (mp->start >= kernelstart && mp->start < kernelend) {
			s = kernelend - mp->start;
			if (mp->size > s)
				mp->size -= s;
			else
				mp->size = 0;
			mp->start = kernelend;
		}
		/*
		 * Now look whether this region ends within the kernel.
		 */
		s = mp->start + mp->size;
		if (s > kernelstart && s < kernelend)
			mp->size -= s - kernelstart;
		/*
		 * Now page align the start of the region.
		 */
		s = mp->start % NBPG;
		if (mp->size >= s) {
			mp->size -= s;
			mp->start += s;
		}
		/*
		 * And now align the size of the region.
		 */
		mp->size -= mp->size % NBPG;
		/*
		 * Check whether some memory is left here.
		 */
		if (mp->size == 0) {
			bcopy(mp + 1, mp,
			      (cnt - (mp - avail)) * sizeof *mp);
			cnt--;
			mp--;
			continue;
		}
		s = mp->start;
		sz = mp->size;
		npgs += btoc(sz);
		for (mp1 = avail; mp1 < mp; mp1++)
			if (s < mp1->start)
				break;
		if (mp1 < mp) {
			bcopy(mp1, mp1 + 1, (void *)mp - (void *)mp1);
			mp1->start = s;
			mp1->size = sz;
		}
	}
#if 0
avail_start = 0;
avail_end = npgs * NBPG;
#endif

#ifdef  HTABENTS
	ptab_cnt = HTABENTS;
#else /* HTABENTS */
	ptab_cnt = 1024;
	while ((HTABSIZE << 7) < ctob(physmem)) {
		ptab_cnt <<= 1;
	}
#endif /* HTABENTS */

	/*
	 * Find suitably aligned memory for HTAB.
	 */
	for (mp = avail; mp->size; mp++) {
		if (mp->start % HTABSIZE == 0) {
			s = 0;
		} else {
			s = HTABSIZE - (mp->start % HTABSIZE) ;
		}
		if (mp->size < s + HTABSIZE)
			continue;
		ptable = (pte_t *)(mp->start + s);
		if (mp->size == s + HTABSIZE) {
			if (s)
				mp->size = s;
			else {
				bcopy(mp + 1, mp,
				      (cnt - (mp - avail)) * sizeof *mp);
				mp = avail;
			}
			break;
		}
		if (s != 0) {
			bcopy(mp, mp + 1,
			      (cnt - (mp - avail)) * sizeof *mp);
			mp++->size = s;
		}
		mp->start += s + HTABSIZE;
		mp->size -= s + HTABSIZE;
		break;
	}
	if (!mp->size)
		panic("not enough memory?");
	bzero((void *)ptable, HTABSIZE);
	ptab_mask = ptab_cnt - 1;
	
	/*
	 * We cannot do vm_bootstrap_steal_memory here,
	 * since we don't run with translation enabled yet.
	 */
	s = sizeof(struct pte_ovtab) * ptab_cnt;
	sz = round_page(s);
	for (mp = avail; mp->size; mp++)
		if (mp->size >= sz)
			break;
	if (!mp->size)
		panic("not enough memory?");
	potable = (struct pte_ovtab *)mp->start;
	mp->size -= sz;
	mp->start += sz;
	if (mp->size == 0)
		bcopy(mp + 1, mp, (cnt - (mp - avail)) * sizeof *mp);
	for (i = 0; i < ptab_cnt; i++)
		LIST_INIT(potable + i);

	/* use only one memory list */
	{ 
		u_int32_t size;
		struct mem_region *curmp;
		size = 0;
		curmp = NULL;
		for (mp = avail; mp->size; mp++) {
			if (mp->size > size) {
				size = mp->size;
				curmp=mp;
			}
		}
		mp = avail;
		if (curmp == mp) {
			++mp;
			mp->size = 0; /* lose the rest of memory */
		} else {
			*mp = *curmp;
			++mp;
			mp->size = 0; /* lose the rest of memory */
		}
	}
	
	for (mp = avail; mp->size; mp++) {
		uvm_page_physload(atop(mp->start), atop(mp->start + mp->size),
			atop(mp->start), atop(mp->start + mp->size),
			VM_FREELIST_DEFAULT);
	}

	/*
	 * Initialize kernel pmap and hardware.
	 */
#if NPMAPS >= KERNEL_SEGMENT / 16
	usedsr[KERNEL_SEGMENT / 16 / (sizeof usedsr[0] * 8)]
		|= 1 << ((KERNEL_SEGMENT / 16) % (sizeof usedsr[0] * 8));
#endif
	for (i = 0; i < 16; i++) {
		pmap_kernel()->pm_sr[i] = EMPTY_SEGMENT;
		asm volatile ("mtsrin %0,%1"
			      :: "r"(EMPTY_SEGMENT), "r"(i << ADDR_SR_SHFT) );
	}
	pmap_kernel()->pm_sr[KERNEL_SR] = KERNEL_SEGMENT;
	asm volatile ("mtsr %0,%1"
		      :: "n"(KERNEL_SR), "r"(KERNEL_SEGMENT));
	asm volatile ("sync; mtsdr1 %0; isync"
		      :: "r"((u_int)ptable | (ptab_mask >> 10)));
	tlbia();
#ifdef USE_PMAP_VP
	pmap_vp_preinit();
#endif /*  USE_PMAP_VP */
	nextavail = avail->start;
}

/*
 * Restrict given range to physical memory
 */
void
pmap_real_memory(start, size)
	vm_offset_t *start;
	vm_size_t *size;
{
	struct mem_region *mp;
	
	for (mp = mem; mp->size; mp++) {
		if (*start + *size > mp->start
		    && *start < mp->start + mp->size) {
			if (*start < mp->start) {
				*size -= mp->start - *start;
				*start = mp->start;
			}
			if (*start + *size > mp->start + mp->size)
				*size = mp->start + mp->size - *start;
			return;
		}
	}
	*size = 0;
}

/*
 * Initialize anything else for pmap handling.
 * Called during vm_init().
 */
void
pmap_init()
{
	struct pv_entry *pv;
	vsize_t sz;
	vaddr_t addr;
	int i, s;
	int bank;
	char *attr;
	
	sz = (vm_size_t)((sizeof(struct pv_entry) + 1) * npgs);
	sz = round_page(sz);
	addr = uvm_km_zalloc(kernel_map, sz);
	s = splimp();
	pv = (struct pv_entry *)addr;
	for (i = npgs; --i >= 0;)
		pv++->pv_idx = -1;
#ifdef USE_PMAP_VP
	pool_init(&pmap_vp_pool, PAGE_SIZE, 0, 0, 0, "ppvl", NULL);
#endif
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0, "pvpl",
            NULL);
	pool_init(&pmap_po_pool, sizeof(struct pte_ovfl), 0, 0, 0, "popl",
            NULL);
	pmap_attrib = (char *)pv;
	bzero(pv, npgs);
	pv = (struct pv_entry *)addr;
	attr = pmap_attrib;
	for (bank = 0; bank < vm_nphysseg; bank++) {
		sz = vm_physmem[bank].end - vm_physmem[bank].start;
		vm_physmem[bank].pmseg.pvent = pv;
		vm_physmem[bank].pmseg.attrs = attr;
		pv += sz;
		attr += sz;
	}
	pmap_initialized = 1;
	splx(s);
}

/*
 * Return the index of the given page in terms of pmap_next_page() calls.
 */
int
pmap_page_index(pa)
	vm_offset_t pa;
{
	struct mem_region *mp;
	vm_size_t pre;
	
	pa &= ~PGOFSET;
	for (pre = 0, mp = avail; mp->size; mp++) {
		if (pa >= mp->start
		    && pa < mp->start + mp->size)
			return btoc(pre + (pa - mp->start));
		pre += mp->size;
	}
	return -1;
}
static inline struct pv_entry *
pmap_find_pv(paddr_t pa)
{
	int bank, off;

	bank = vm_physseg_find(atop(pa), &off);
	if (bank != -1) {
		return &vm_physmem[bank].pmseg.pvent[off];
	} 
	return NULL;
}
static inline char *
pmap_find_attr(paddr_t pa)
{
	int bank, off;

	bank = vm_physseg_find(atop(pa), &off);
	if (bank != -1) {
		return &vm_physmem[bank].pmseg.attrs[off];
	} 
	return NULL;
}

vm_offset_t ppc_kvm_size = VM_KERN_ADDR_SIZE_DEF;

/*
 * How much virtual space is available to the kernel?
 */
void
pmap_virtual_space(start, end)
	vm_offset_t *start, *end;
{
	/*
	 * Reserve one segment for kernel virtual memory
	 */
	*start = (vm_offset_t)(KERNEL_SR << ADDR_SR_SHFT);
	*end = *start + VM_KERN_ADDRESS_SIZE;
}

/*
 * Return the number of possible page indices returned
 * from pmap_page_index for any page provided by pmap_next_page.
 */
u_int
pmap_free_pages()
{
	return npgs;
}

/*
 * If there are still physical pages available, put the address of
 * the next available one at paddr and return TRUE.  Otherwise,
 * return FALSE to indicate that there are no more free pages.
 */
int
pmap_next_page(paddr)
	vm_offset_t *paddr;
{
	static int lastidx = -1;
	
	if (lastidx == -1) {
		nextavail = avail->start;
	}
	if (lastidx < 0
	    || nextavail >= avail[lastidx].start + avail[lastidx].size) {
		if (avail[++lastidx].size == 0)
			return FALSE;
		nextavail = avail[lastidx].start;
	}
	*paddr = nextavail;
	nextavail += NBPG;
	return TRUE;
}

/*
 * Create and return a physical map.
 */
struct pmap *
pmap_create()
{
	struct pmap *pm;
	
	pm = (struct pmap *)malloc(sizeof *pm, M_VMPMAP, M_WAITOK);
	bzero((caddr_t)pm, sizeof *pm);
	pmap_pinit(pm);
	return pm;
}

/*
 * Initialize a preallocated and zeroed pmap structure.
 */
void
pmap_pinit(pm)
	struct pmap *pm;
{
	int i, j, k;
	int s, seg;
	
	/*
	 * Allocate some segment registers for this pmap.
	 */
	s = splimp();
	pm->pm_refs = 1;
	for (i = 0; i < sizeof usedsr / sizeof usedsr[0]; i++)
		if (usedsr[i] != 0xffffffff) {
			j = ffs(~usedsr[i]) - 1;
			usedsr[i] |= 1 << j;
			seg = (i * sizeof usedsr[0] * 8 + j) * 16;
			for (k = 0; k < 16; k++)
				pm->pm_sr[k] = seg + k;
			splx(s);
			return;
		}
	splx(s);
	panic("out of segments");
}

/*
 * Add a reference to the given pmap.
 */
void
pmap_reference(pm)
	struct pmap *pm;
{
	pm->pm_refs++;
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pm)
	struct pmap *pm;
{
	if (--pm->pm_refs == 0) {
		pmap_release(pm);
		free((caddr_t)pm, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(pm)
	struct pmap *pm;
{
	int i, j;
	int s;

#ifdef USE_PMAP_VP
	pmap_vp_destroy(pm);
#endif /*  USE_PMAP_VP */
	if (!pm->pm_sr[0])
		panic("pmap_release");
	i = pm->pm_sr[0] / 16;
	j = i % (sizeof usedsr[0] * 8);
	i /= sizeof usedsr[0] * 8;
	s = splimp();
	usedsr[i] &= ~(1 << j);
	splx(s);
}

/*
 * Copy the range specified by src_addr/len
 * from the source map to the range dst_addr/len
 * in the destination map.
 *
 * This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	struct pmap *dst_pmap, *src_pmap;
	vm_offset_t dst_addr, src_addr;
	vm_size_t len;
{
}

/*
 * Garbage collects the physical map system for
 * pages which are no longer used.
 * Success need not be guaranteed -- that is, there
 * may well be pages which are not referenced, but
 * others may be collected.
 * Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pm)
	struct pmap *pm;
{
}

/*
 * Fill the given physical page with zeroes.
 */
void
pmap_zero_page(pa)
	vm_offset_t pa;
{
#if 0
	bzero((caddr_t)pa, NBPG);
#else
        int i;
                 
        for (i = NBPG/CACHELINESIZE; i > 0; i--) {
                __asm __volatile ("dcbz 0,%0" :: "r"(pa));
                pa += CACHELINESIZE;
        }
#endif
}

/*
 * Copy the given physical source page to its destination.
 */
void
pmap_copy_page(src, dst)
	vm_offset_t src, dst;
{
	bcopy((caddr_t)src, (caddr_t)dst, NBPG);
}

struct pv_entry *
pmap_alloc_pv()
{
	struct pv_entry *pv;
	int s;

	/*
	 * XXX - this splimp can go away once we have PMAP_NEW and
	 *       a correct implementation of pmap_kenter.
	 */
	/*
	 * Note that it's completly ok to use a pool here because it will
	 * never map anything or call pmap_enter because we have
	 * PMAP_MAP_POOLPAGE.
	 */
	s = splimp();
	pv = pool_get(&pmap_pv_pool, PR_NOWAIT);
	splx(s);
	/*
	 * XXX - some day we might want to implement pv stealing, or
	 *       to pass down flags from pmap_enter about allowed failure.
	 *	 Right now - just panic.
	 */
	if (pv == NULL)
		panic("pmap_alloc_pv: failed to allocate pv");

	return pv;
}

void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	int s;

	/* XXX - see pmap_alloc_pv */
	s = splimp();
	pool_put(&pmap_pv_pool, pv);
	splx(s);
}

/*
 * We really hope that we don't need overflow entries
 * before the VM system is initialized!							XXX
 * XXX - see pmap_alloc_pv
 */
struct pte_ovfl *
poalloc()
{
	struct pte_ovfl *po;
	int s;
	
#ifdef DIAGNOSTIC
	if (!pmap_initialized)
		panic("poalloc");
#endif
	pofreebusy = 1;
	if (!LIST_EMPTY(&pofreetable0)) {
		po = LIST_FIRST(&pofreetable0);
		LIST_REMOVE(po,po_list);
		pofreebusy = 0;
		return po;
	}
	pofreebusy = 0;

	if (!LIST_EMPTY(&pofreetable1)) {
		po = LIST_FIRST(&pofreetable1);
		LIST_REMOVE(po,po_list);
		pofreebusy = 0;
		return po;
	}

	s = splimp();
	po = pool_get(&pmap_po_pool, PR_NOWAIT);
	splx(s);
	if (po == NULL)
		panic("poalloc: failed to alloc po");
	return po;
}

void
pofree(po, freepage)
	struct pte_ovfl *po;
	int freepage;
{
	int s;
	if (freepage) {
		s = splimp();
		pool_put(&pmap_po_pool, po);
		splx(s);
		while (!LIST_EMPTY(&pofreetable1)) {
			po = LIST_FIRST(&pofreetable1);
			LIST_REMOVE(po, po_list);
			s = splimp();
			pool_put(&pmap_po_pool, po);
			splx(s);
		}

		pofreebusy = 1;
		while (!LIST_EMPTY(&pofreetable0)) {
			po = LIST_FIRST(&pofreetable0);
			LIST_REMOVE(po, po_list);
			s = splimp();
			pool_put(&pmap_po_pool, po);
			splx(s);
		}
		pofreebusy = 0;

	} else {
		if (pofreebusy == 0)
			LIST_INSERT_HEAD(&pofreetable0, po, po_list);
		else
			LIST_INSERT_HEAD(&pofreetable1, po, po_list);
	}
}

/*
 * This returns whether this is the first mapping of a page.
 */
static inline int
pmap_enter_pv(pm, pteidx, va, pa)
	struct pmap *pm;
	int pteidx;
	vm_offset_t va;
	vm_offset_t pa;
{
	struct pv_entry *npv;
	int s, first;
	struct pv_entry *pv;
	
	if (!pmap_initialized)
		return 0;

	pv = pmap_find_pv( pa );
	if (pv == NULL) 
		return 0;

	s = splimp();

	if ((first = pv->pv_idx) == -1) {
		/*
		 * No entries yet, use header as the first entry.
		 */
		pv->pv_va = va;
		pv->pv_idx = pteidx;
		pv->pv_pmap = pm;
		pv->pv_next = NULL;
	} else {
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		npv = pmap_alloc_pv();
		npv->pv_va = va;
		npv->pv_idx = pteidx;
		npv->pv_pmap = pm;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
	}
	splx(s);
	return first;
}

void
pmap_remove_pv(pm, pteidx, va, pte_lo)
	struct pmap *pm;                                            
	int pteidx;
	vm_offset_t va;
	u_int32_t pte_lo;
{
	struct pv_entry *pv, *npv;

        int bank, pg;
	vm_offset_t pa;
	char *attr;

	pa = pte_lo & ~PGOFSET;
                   
        bank = vm_physseg_find(atop(pa), &pg);
        if (bank == -1)
                return;
        pv =   &vm_physmem[bank].pmseg.pvent[pg];
        attr = &vm_physmem[bank].pmseg.attrs[pg];

	/*
	 * First transfer reference/change bits to cache.
	 */
	*attr |= (pte_lo & (PTE_REF | PTE_CHG)) >> ATTRSHFT;
	
	/*
	 * Remove from the PV table.
	 *
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (va == pv->pv_va && pm == pv->pv_pmap) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			pmap_free_pv(npv);
		} else {
			pv->pv_pmap = 0;
			pv->pv_idx = -1;
		}
	} else {
		for (; (npv = pv->pv_next) != NULL; pv = npv) {
			if (va == npv->pv_va && pm == npv->pv_pmap)
			{
				break;
			}
		}
		if (npv) {
			pv->pv_next = npv->pv_next;
			pmap_free_pv(npv);
		}
#if 1
#ifdef	DIAGNOSTIC
		else {
			printf("pmap_remove_pv: not on list\n");
			/*
			panic("pmap_remove_pv: not on list");
			*/
		}
#endif
#endif
	}
}

int
pmap_enter_c_pv(struct pmap *pm, vm_offset_t va, vm_offset_t pa,
	vm_prot_t prot, int flags, int cacheable, int pv);

/*
 * Insert physical page at pa into the given pmap at virtual address va.
 */
int
pmap_enter(pm, va, pa, prot, flags)
	struct pmap *pm;
	vm_offset_t va, pa;
	vm_prot_t prot;
	int flags;
{
	return pmap_enter_c_pv(pm, va, pa, prot, flags, PMAP_CACHE_DEFAULT,
		TRUE);
}
int
pmap_enter_c_pv(pm, va, pa, prot, flags, cacheable, pv)
	struct pmap *pm;
	vm_offset_t va, pa;
	vm_prot_t prot;
	int flags;
	int cacheable;
	int pv;
{
	sr_t sr;
	int idx, s;
	pte_t pte;
	struct pte_ovfl *po;
	struct mem_region *mp;

	/*
	 * Have to remove any existing mapping first.
	 */
	pmap_remove(pm, va, va + NBPG-1);

	pm->pm_stats.resident_count++;

#ifdef USE_PMAP_VP
	pmap_vp_enter(pm, va, pa);
#endif /*  USE_PMAP_VP */
	
	/*
	 * Compute the HTAB index.
	 */
	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr, va);
	/*
	 * Construct the PTE.
	 *
	 * Note: Don't set the valid bit for correct operation of tlb update.
	 */
	pte.pte_hi = ((sr & SR_VSID) << PTE_VSID_SHFT)
		| ((va & ADDR_PIDX) >> ADDR_API_SHFT);
	if (cacheable == PMAP_CACHE_DEFAULT) {
		pte.pte_lo = (pa & PTE_RPGN) | PTE_M | PTE_I | PTE_G;
		for (mp = mem; mp->size; mp++) {
			if (pa >= mp->start && pa < mp->start + mp->size) {
				pte.pte_lo &= ~(PTE_I | PTE_G);
				break;
			}
		}
	} else if (cacheable == PMAP_CACHE_CI) {
		pte.pte_lo = (pa & PTE_RPGN) | PTE_M | PTE_I | PTE_G;
	} else if (cacheable == PMAP_CACHE_WT) {
		pte.pte_lo = (pa & PTE_RPGN) | PTE_M | PTE_W | PTE_G;
	} else if (cacheable == PMAP_CACHE_WB) {
		pte.pte_lo = (pa & PTE_RPGN) | PTE_M;
	} else {
		panic("pmap_enter_c_pv: invalid cacheable %x\n", cacheable);
	}
	if (prot & VM_PROT_WRITE)
		pte.pte_lo |= PTE_RW;
	else
		pte.pte_lo |= PTE_RO;

	/*
	 * Now record mapping for later back-translation.
	 */
	if (pv == TRUE) {
		if (pmap_enter_pv(pm, idx, va, pa)) {
			/* 
			 * Flush the real memory from the cache.
			 */
			syncicache((void *)pa, NBPG);
		}
	}
	
	s = splimp();
	/*
	 * Try to insert directly into HTAB.
	 */
	if (pte_insert(idx, &pte)) {
		splx(s);
		return (0);
	}
	
	/*
	 * Have to allocate overflow entry.
	 *
	 * Note, that we must use real addresses for these.
	 */
	po = poalloc();
	po->po_pte = pte;
	LIST_INSERT_HEAD(potable + idx, po, po_list);
	splx(s);

	return (0);
}

#define KERN_MAP_PV TRUE

void
pmap_kenter_cache(va, pa, prot, cacheable)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int cacheable;
{
	pmap_enter_c_pv(pmap_kernel(), va, pa, prot, PMAP_WIRED, cacheable,
		KERN_MAP_PV);
}
void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pmap_enter_c_pv(pmap_kernel(), va, pa, prot, PMAP_WIRED,
		PMAP_CACHE_DEFAULT, KERN_MAP_PV);
}

void pmap_remove_pvl( struct pmap *pm, vm_offset_t va, vm_offset_t endva,
	int pv);
void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	for (len >>= PAGE_SHIFT; len > 0; len--, va += PAGE_SIZE) {
		pmap_remove_pvl(pmap_kernel(), va, va + PAGE_SIZE, KERN_MAP_PV);
	}
}

void pmap_pte_invalidate(vaddr_t va, pte_t *ptp);
void
pmap_pte_invalidate(vaddr_t va, pte_t *ptp)
{
	ptp->pte_hi &= ~PTE_VALID;
	asm volatile ("sync");
	tlbie(va);
	tlbsync();
}


/*
 * Remove the given range of mapping entries.
 */
void
pmap_remove(pm, va, endva)
	struct pmap *pm;
	vm_offset_t va, endva;
{
	pmap_remove_pvl(pm, va, endva, TRUE);
}

void
pmap_remove_pvl(pm, va, endva, pv)
	struct pmap *pm;
	vm_offset_t va, endva;
	int pv;
{
	int idx, i, s;
	int found; /* if found, we are done, only one mapping per va */
	sr_t sr;
	pte_t *ptp;
	struct pte_ovfl *po, *npo;
	
	s = splimp();
	for (; va < endva; va += NBPG) {
#ifdef USE_PMAP_VP
		if (0 == pmap_vp_remove(pm, va)) {
			/* no mapping */
			continue;
		}
#endif /*  USE_PMAP_VP */
		found = 0;
		sr = ptesr(pm->pm_sr, va);
		idx = pteidx(sr, va);
		for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
			if (ptematch(ptp, sr, va, PTE_VALID)) {
				pmap_pte_invalidate(va, ptp);
				if (pv == TRUE) {
					pmap_remove_pv(pm, idx, va,
						ptp->pte_lo);
				}
				pm->pm_stats.resident_count--;
				found = 1;
				break;
			}
		if (found) {
			continue;
		}
		for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
			if (ptematch(ptp, sr, va, PTE_VALID | PTE_HID)) {
				pmap_pte_invalidate(va, ptp);
				if (pv == TRUE) {
					pmap_remove_pv(pm, idx, va,
						ptp->pte_lo);
				}
				pm->pm_stats.resident_count--;
				found = 1;
				break;
			}
		if (found) {
			continue;
		}
		for (po = potable[idx].lh_first; po; po = npo) {
			npo = po->po_list.le_next;
			if (ptematch(&po->po_pte, sr, va, 0)) {
				if (pv == TRUE) {
					pmap_remove_pv(pm, idx, va,
						po->po_pte.pte_lo);
				}
				LIST_REMOVE(po, po_list);
				pofree(po, 1);
				pm->pm_stats.resident_count--;
				break;
			}
		}
	}
	splx(s);
}

pte_t *
pte_find(pm, va)
	struct pmap *pm;
	vm_offset_t va;
{
	int idx, i;
	sr_t sr;
	pte_t *ptp;
	struct pte_ovfl *po;

	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr, va);
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
		if (ptematch(ptp, sr, va, PTE_VALID))
			return ptp;
	for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
		if (ptematch(ptp, sr, va, PTE_VALID | PTE_HID))
			return ptp;
	for (po = potable[idx].lh_first; po; po = po->po_list.le_next)
		if (ptematch(&po->po_pte, sr, va, 0))
			return &po->po_pte;
	return 0;
}

/*
 * Get the physical page address for the given pmap/virtual address.
 */
boolean_t
pmap_extract(pm, va, pap)
	struct pmap *pm;
	vaddr_t va;
	paddr_t *pap;
{
	pte_t *ptp;
	int s = splimp();
	boolean_t ret;
	
	if (!(ptp = pte_find(pm, va))) {
		/* return address 0 if not mapped??? */
		ret = FALSE;
		if (pm == pmap_kernel() && va < 0x80000000){
			/* if in kernel, va==pa for 0 - 0x80000000 */
			*pap = va;
			ret = TRUE;
		}
		splx(s);
		return ret;
	}
	*pap = (ptp->pte_lo & PTE_RPGN) | (va & ADDR_POFF);
	splx(s);
	return TRUE;
}

/*
 * Lower the protection on the specified range of this pmap.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_protect(pm, sva, eva, prot)
	struct pmap *pm;
	vm_offset_t sva, eva;
	vm_prot_t prot;
{
	pte_t *ptp;
	int valid, s;
	
	if (prot & VM_PROT_READ) {
		s = splimp();
		while (sva < eva) {
			if ((ptp = pte_find(pm, sva))) {
				valid = ptp->pte_hi & PTE_VALID;
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(sva);
				tlbsync();
				ptp->pte_lo &= ~PTE_PP;
				ptp->pte_lo |= PTE_RO;
				asm volatile ("sync");
				ptp->pte_hi |= valid;
			}
			sva += NBPG;
		}
		splx(s);
		return;
	}
	pmap_remove(pm, sva, eva);
}

boolean_t
ptemodify(pa, mask, val)
	paddr_t pa;
	u_int mask;
	u_int val;
{
	struct pv_entry *pv;
	pte_t *ptp;
	struct pte_ovfl *po;
	int i, s;
	char * pattr;
	boolean_t ret;
	u_int32_t pte_hi;
	int found;
	vaddr_t va;
	sr_t sr;
	struct pmap *pm;

	ret = ptebits(pa, mask);
	
	pv = pmap_find_pv(pa);
	if (pv == NULL) 
		return (ret);
	pattr = pmap_find_attr(pa);

	/*
	 * First modify bits in cache.
	 */
	*pattr &= ~mask >> ATTRSHFT;
	*pattr |= val >> ATTRSHFT;
	
	if (pv->pv_idx < 0)
		return (ret);

	s = splimp();
	for (; pv; pv = pv->pv_next) {
		va = pv->pv_va;
		pm = pv->pv_pmap;
		sr = ptesr(pm->pm_sr, va);
		pte_hi = ((sr & SR_VSID) << PTE_VSID_SHFT)
		    | ((va & ADDR_PIDX) >> ADDR_API_SHFT);
		found = 0;
		for (ptp = ptable + pv->pv_idx * 8, i = 8; --i >= 0; ptp++)
			if ((pte_hi | PTE_VALID) == ptp->pte_hi) {
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(pv->pv_va);
				tlbsync();
				ptp->pte_lo &= ~mask;
				ptp->pte_lo |= val;
				asm volatile ("sync");
				ptp->pte_hi |= PTE_VALID;
				found = 1;
				break;
			}
		if (found)
			continue;
		for (ptp = ptable + (pv->pv_idx ^ ptab_mask) * 8, i = 8;
		    --i >= 0; ptp++) {
			if ((pte_hi | PTE_VALID | PTE_HID) == ptp->pte_hi) {
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(pv->pv_va);
				tlbsync();
				ptp->pte_lo &= ~mask;
				ptp->pte_lo |= val;
				asm volatile ("sync");
				ptp->pte_hi |= PTE_VALID;
				found = 1;
			}
		}
		if (found)
			continue;
		for (po = potable[pv->pv_idx].lh_first;
		    po; po = po->po_list.le_next) {
			if (pte_hi == po->po_pte.pte_hi) {
				po->po_pte.pte_lo &= ~mask;
				po->po_pte.pte_lo |= val;
			}
		}
	}
	splx(s);

	return (ret);
}

int
ptebits(pa, bit)
	vm_offset_t pa;
	int bit;
{
	struct pv_entry *pv;
	pte_t *ptp;
	struct pte_ovfl *po;
	int i, s, bits = 0;
	char *pattr;

	pv = pmap_find_pv(pa);
	if (pv == NULL)
		return 0;
	pattr = pmap_find_attr(pa);

	/*
	 * First try the cache.
	 */
	bits |= ((*pattr) << ATTRSHFT) & bit;
	if (bits == bit)
		return bits;

	if (pv->pv_idx < 0)
		return 0;
	
	s = splimp();
	for (; pv; pv = pv->pv_next) {
		for (ptp = ptable + pv->pv_idx * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				bits |= ptp->pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
		for (ptp = ptable + (pv->pv_idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				bits |= ptp->pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
		for (po = potable[pv->pv_idx].lh_first; po; po = po->po_list.le_next)
			if ((po->po_pte.pte_lo & PTE_RPGN) == pa) {
				bits |= po->po_pte.pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
	}
	splx(s);
	return bits;
}

/*
 * Lower the protection on the specified physical page.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t prot;
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(pg);
	vm_offset_t va;
	int s;
	struct pmap *pm;
	struct pv_entry *pv, *npv;
	int idx, i;
	sr_t sr;
	pte_t *ptp;
	struct pte_ovfl *po, *npo;
	int found;
	char *pattr;
	
	pa &= ~ADDR_POFF;
	if (prot & VM_PROT_READ) {
		ptemodify(pa, PTE_PP, PTE_RO);
		return;
	}

	pattr = pmap_find_attr(pa);
	pv = pmap_find_pv(pa);
	if (pv == NULL) 
		return;

	s = splimp();
	while (pv->pv_idx != -1) {
		va = pv->pv_va;
		pm = pv->pv_pmap;
#ifdef USE_PMAP_VP
		pmap_vp_remove(pm, va);
#endif /*  USE_PMAP_VP */

		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			pmap_free_pv(npv);
		} else {
			pv->pv_pmap = 0;
			pv->pv_idx = -1;
		}

		/* now remove this entry from the table */
		found = 0;
		sr = ptesr(pm->pm_sr, va);
		idx = pteidx(sr, va);
		for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++) {
			if (ptematch(ptp, sr, va, PTE_VALID)) {
				pmap_pte_invalidate(va, ptp);
				*pattr |= (ptp->pte_lo & (PTE_REF | PTE_CHG))
					>> ATTRSHFT;
				pm->pm_stats.resident_count--;
				found = 1;
				break;
			}
		}
		if (found)
			continue;
		for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0;
			ptp++)
		{
			if (ptematch(ptp, sr, va, PTE_VALID | PTE_HID)) {
				pmap_pte_invalidate(va, ptp);
				*pattr |= (ptp->pte_lo & (PTE_REF | PTE_CHG))
					>> ATTRSHFT;
				pm->pm_stats.resident_count--;
				found = 1;
				break;
			}
		}
		if (found)
			continue;
		for (po = potable[idx].lh_first; po; po = npo) {
			npo = po->po_list.le_next;
			if (ptematch(&po->po_pte, sr, va, 0)) {
				LIST_REMOVE(po, po_list);
				pofree(po, 1);
				pm->pm_stats.resident_count--;
				break;
			}
		}

		
	}
	splx(s);
}
/*
 * this code to manipulate the BAT tables was added here
 * because it is closely related to the vm system.
 * --dsr
 */

#include <machine/bat.h>

/* one major problem of mapping IO with bats, is that it
 * is not possible to use caching on any level of granularity 
 * that is reasonable.
 * This is just enhancing an existing design (that should be
 * replaced in my opinion).
 *
 * Current design only allow mapping of 256 MB block. (normally 1-1)
 * but not necessarily (in the case of PCI io at 0xf0000000 where
 * it might be desireable to map it elsewhere because that is
 * where the stack is?)
 */
void
addbatmap(u_int32_t vaddr, u_int32_t raddr, u_int32_t wimg)
{
	u_int32_t segment;
	segment = vaddr >> (32 - 4);
	battable[segment].batu = BATU(vaddr);
	battable[segment].batl = BATL(raddr, wimg);
}

/* ??? */
void
pmap_activate(struct proc *p)
{
#if 0
	struct pcb *pcb = &p->p_addr->u_pcb;
	pmap_t pmap = p->p_vmspace->vm_map.pmap;

	/*
	 * XXX Normally performed in cpu_fork();
	 */
	if (pcb->pcb_pm != pmap) {
		pcb->pcb_pm = pmap;
		(void) pmap_extract(pmap_kernel(), (vaddr_t)pcb->pcb_pm,
		    (paddr_t *)&pcb->pcb_pmreal);
	}
	curpcb=pcb;
	if (p == curproc) {
		/* Disable interrupts while switching. */
		__asm __volatile("mfmsr %0" : "=r"(psl) :);
		psl &= ~PSL_EE;
		__asm __volatile("mtmsr %0" :: "r"(psl));

		/* Store pointer to new current pmap. */
		curpm = pcb->pcb_pmreal;

		/* Save kernel SR. */
		__asm __volatile("mfsr %0,14" : "=r"(ksr) :);

		/*
		 * Set new segment registers.  We use the pmap's real
		 * address to avoid accessibility problems.
		 */
		rpm = pcb->pcb_pmreal;
		for (i = 0; i < 16; i++) {
			seg = rpm->pm_sr[i];
			__asm __volatile("mtsrin %0,%1"
			    :: "r"(seg), "r"(i << ADDR_SR_SHFT));
		}

		/* Restore kernel SR. */
		__asm __volatile("mtsr 14,%0" :: "r"(ksr));

		/* Interrupts are OK again. */
		psl |= PSL_EE;
		__asm __volatile("mtmsr %0" :: "r"(psl));
	}
#endif
	return;
}
/* ??? */
void
pmap_deactivate(struct proc *p)
{
	return;
}
