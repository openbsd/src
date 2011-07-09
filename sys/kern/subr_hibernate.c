/*	$OpenBSD: subr_hibernate.c,v 1.8 2011/07/09 00:08:04 mlarkin Exp $	*/

/*
 * Copyright (c) 2011 Ariane van der Steldt <ariane@stack.nl>
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

#include <sys/hibernate.h>
#include <sys/param.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <uvm/uvm.h>
#include <machine/hibernate.h>

extern char *disk_readlabel(struct disklabel *, dev_t, char *, size_t);

/*
 * Hib alloc enforced alignment.
 */
#define HIB_ALIGN		8 /* bytes alignment */

/*
 * sizeof builtin operation, but with alignment constraint.
 */
#define HIB_SIZEOF(_type)	roundup(sizeof(_type), HIB_ALIGN)

struct hiballoc_entry
{
	size_t			hibe_use;
	size_t			hibe_space;
	RB_ENTRY(hiballoc_entry) hibe_entry;
};

/*
 * Compare hiballoc entries based on the address they manage.
 *
 * Since the address is fixed, relative to struct hiballoc_entry,
 * we just compare the hiballoc_entry pointers.
 */
static __inline int
hibe_cmp(struct hiballoc_entry *l, struct hiballoc_entry *r)
{
	return l < r ? -1 : (l > r);
}

RB_PROTOTYPE(hiballoc_addr, hiballoc_entry, hibe_entry, hibe_cmp)

/*
 * Given a hiballoc entry, return the address it manages.
 */
static __inline void*
hib_entry_to_addr(struct hiballoc_entry *entry)
{
	caddr_t addr;

	addr = (caddr_t)entry;
	addr += HIB_SIZEOF(struct hiballoc_entry);
	return addr;
}

/*
 * Given an address, find the hiballoc that corresponds.
 */
static __inline struct hiballoc_entry*
hib_addr_to_entry(void* addr_param)
{
	caddr_t addr;

	addr = (caddr_t)addr_param;
	addr -= HIB_SIZEOF(struct hiballoc_entry);
	return (struct hiballoc_entry*)addr;
}

RB_GENERATE(hiballoc_addr, hiballoc_entry, hibe_entry, hibe_cmp)

/*
 * Allocate memory from the arena.
 *
 * Returns NULL if no memory is available.
 */
void*
hib_alloc(struct hiballoc_arena *arena, size_t alloc_sz)
{
	struct hiballoc_entry *entry, *new_entry;
	size_t find_sz;

	/*
	 * Enforce alignment of HIB_ALIGN bytes.
	 *
	 * Note that, because the entry is put in front of the allocation,
	 * 0-byte allocations are guaranteed a unique address.
	 */
	alloc_sz = roundup(alloc_sz, HIB_ALIGN);

	/*
	 * Find an entry with hibe_space >= find_sz.
	 *
	 * If the root node is not large enough, we switch to tree traversal.
	 * Because all entries are made at the bottom of the free space,
	 * traversal from the end has a slightly better chance of yielding
	 * a sufficiently large space.
	 */
	find_sz = alloc_sz + HIB_SIZEOF(struct hiballoc_entry);
	entry = RB_ROOT(&arena->hib_addrs);
	if (entry != NULL && entry->hibe_space < find_sz) {
		RB_FOREACH_REVERSE(entry, hiballoc_addr, &arena->hib_addrs) {
			if (entry->hibe_space >= find_sz)
				break;
		}
	}

	/*
	 * Insufficient or too fragmented memory.
	 */
	if (entry == NULL)
		return NULL;

	/*
	 * Create new entry in allocated space.
	 */
	new_entry = (struct hiballoc_entry*)(
	    (caddr_t)hib_entry_to_addr(entry) + entry->hibe_use);
	new_entry->hibe_space = entry->hibe_space - find_sz;
	new_entry->hibe_use = alloc_sz;

	/*
	 * Insert entry.
	 */
	if (RB_INSERT(hiballoc_addr, &arena->hib_addrs, new_entry) != NULL)
		panic("hib_alloc: insert failure");
	entry->hibe_space = 0;

	/* Return address managed by entry. */
	return hib_entry_to_addr(new_entry);
}

/*
 * Free a pointer previously allocated from this arena.
 * 
 * If addr is NULL, this will be silently accepted.
 */
void
hib_free(struct hiballoc_arena *arena, void *addr)
{
	struct hiballoc_entry *entry, *prev;

	if (addr == NULL)
		return;

	/*
	 * Derive entry from addr and check it is really in this arena.
	 */
	entry = hib_addr_to_entry(addr);
	if (RB_FIND(hiballoc_addr, &arena->hib_addrs, entry) != entry)
		panic("hib_free: freed item %p not in hib arena", addr);

	/*
	 * Give the space in entry to its predecessor.
	 *
	 * If entry has no predecessor, change its used space into free space
	 * instead.
	 */
	prev = RB_PREV(hiballoc_addr, &arena->hib_addrs, entry);
	if (prev != NULL &&
	    (void*)((caddr_t)prev + HIB_SIZEOF(struct hiballoc_entry) +
	    prev->hibe_use + prev->hibe_space) == entry) {
		/* Merge entry. */
		RB_REMOVE(hiballoc_addr, &arena->hib_addrs, entry);
		prev->hibe_space += HIB_SIZEOF(struct hiballoc_entry) +
		    entry->hibe_use + entry->hibe_space;
	} else {
	  	/* Flip used memory to free space. */
		entry->hibe_space += entry->hibe_use;
		entry->hibe_use = 0;
	}
}

/*
 * Initialize hiballoc.
 *
 * The allocator will manage memmory at ptr, which is len bytes.
 */
int
hiballoc_init(struct hiballoc_arena *arena, void *p_ptr, size_t p_len)
{
	struct hiballoc_entry *entry;
	caddr_t ptr;
	size_t len;

	RB_INIT(&arena->hib_addrs);

	/*
	 * Hib allocator enforces HIB_ALIGN alignment.
	 * Fixup ptr and len.
	 */
	ptr = (caddr_t)roundup((vaddr_t)p_ptr, HIB_ALIGN);
	len = p_len - ((size_t)ptr - (size_t)p_ptr);
	len &= ~((size_t)HIB_ALIGN - 1);

	/*
	 * Insufficient memory to be able to allocate and also do bookkeeping.
	 */
	if (len <= HIB_SIZEOF(struct hiballoc_entry))
		return ENOMEM;

	/*
	 * Create entry describing space.
	 */
	entry = (struct hiballoc_entry*)ptr;
	entry->hibe_use = 0;
	entry->hibe_space = len - HIB_SIZEOF(struct hiballoc_entry);
	RB_INSERT(hiballoc_addr, &arena->hib_addrs, entry);

	return 0;
}


/*
 * Zero all free memory.
 */
void
uvm_pmr_zero_everything(void)
{
	struct uvm_pmemrange	*pmr;
	struct vm_page		*pg;
	int			 i;

	uvm_lock_fpageq();
	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		/* Zero single pages. */
		while ((pg = TAILQ_FIRST(&pmr->single[UVM_PMR_MEMTYPE_DIRTY]))
		    != NULL) {
			uvm_pmr_remove(pmr, pg);
			uvm_pagezero(pg);
			atomic_setbits_int(&pg->pg_flags, PG_ZERO);
			uvmexp.zeropages++;
			uvm_pmr_insert(pmr, pg, 0);
		}

		/* Zero multi page ranges. */
		while ((pg = RB_ROOT(&pmr->size[UVM_PMR_MEMTYPE_DIRTY]))
		    != NULL) {
			pg--; /* Size tree always has second page. */
			uvm_pmr_remove(pmr, pg);
			for (i = 0; i < pg->fpgsz; i++) {
				uvm_pagezero(&pg[i]);
				atomic_setbits_int(&pg[i].pg_flags, PG_ZERO);
				uvmexp.zeropages++;
			}
			uvm_pmr_insert(pmr, pg, 0);
		}
	}
	uvm_unlock_fpageq();
}

/*
 * Mark all memory as dirty.
 *
 * Used to inform the system that the clean memory isn't clean for some
 * reason, for example because we just came back from hibernate.
 */
void
uvm_pmr_dirty_everything(void)
{
	struct uvm_pmemrange	*pmr;
	struct vm_page		*pg;
	int			 i;

	uvm_lock_fpageq();
	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		/* Dirty single pages. */
		while ((pg = TAILQ_FIRST(&pmr->single[UVM_PMR_MEMTYPE_ZERO]))
		    != NULL) {
			uvm_pmr_remove(pmr, pg);
			atomic_clearbits_int(&pg->pg_flags, PG_ZERO);
			uvm_pmr_insert(pmr, pg, 0);
		}

		/* Dirty multi page ranges. */
		while ((pg = RB_ROOT(&pmr->size[UVM_PMR_MEMTYPE_ZERO]))
		    != NULL) {
			pg--; /* Size tree always has second page. */
			uvm_pmr_remove(pmr, pg);
			for (i = 0; i < pg->fpgsz; i++)
				atomic_clearbits_int(&pg[i].pg_flags, PG_ZERO);
			uvm_pmr_insert(pmr, pg, 0);
		}
	}

	uvmexp.zeropages = 0;
	uvm_unlock_fpageq();
}

/*
 * Allocate the highest address that can hold sz.
 *
 * sz in bytes.
 */
int
uvm_pmr_alloc_pig(paddr_t *addr, psize_t sz)
{
	struct uvm_pmemrange	*pmr;
	struct vm_page		*pig_pg, *pg;

	/*
	 * Convert sz to pages, since that is what pmemrange uses internally.
	 */
	sz = atop(round_page(sz));

	uvm_lock_fpageq();

	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		RB_FOREACH_REVERSE(pig_pg, uvm_pmr_addr, &pmr->addr) {
			if (pig_pg->fpgsz >= sz) {
				goto found;
			}
		}
	}

	/*
	 * Allocation failure.
	 */
	uvm_unlock_pageq();
	return ENOMEM;

found:
	/* Remove page from freelist. */
	uvm_pmr_remove_size(pmr, pig_pg);
	pig_pg->fpgsz -= sz;
	pg = pig_pg + pig_pg->fpgsz;
	if (pig_pg->fpgsz == 0)
		uvm_pmr_remove_addr(pmr, pig_pg);
	else
		uvm_pmr_insert_size(pmr, pig_pg);

	uvmexp.free -= sz;
	*addr = VM_PAGE_TO_PHYS(pg);

	/*
	 * Update pg flags.
	 *
	 * Note that we trash the sz argument now.
	 */
	while (sz > 0) {
		KASSERT(pg->pg_flags & PQ_FREE);

		atomic_clearbits_int(&pg->pg_flags,
		    PG_PMAP0|PG_PMAP1|PG_PMAP2|PG_PMAP3);

		if (pg->pg_flags & PG_ZERO)
			uvmexp.zeropages -= sz;
		atomic_clearbits_int(&pg->pg_flags,
		    PG_ZERO|PQ_FREE);

		pg->uobject = NULL;
		pg->uanon = NULL;
		pg->pg_version++;

		/*
		 * Next.
		 */
		pg++;
		sz--;
	}

	/* Return. */
	uvm_unlock_fpageq();
	return 0;
}

/*
 * Allocate a piglet area.
 *
 * This is as low as possible.
 * Piglets are aligned.
 *
 * sz and align in bytes.
 *
 * The call will sleep for the pagedaemon to attempt to free memory.
 * The pagedaemon may decide its not possible to free enough memory, causing
 * the allocation to fail.
 */
int
uvm_pmr_alloc_piglet(paddr_t *addr, psize_t sz, paddr_t align)
{
	vaddr_t			 pg_addr, piglet_addr;
	struct uvm_pmemrange	*pmr;
	struct vm_page		*pig_pg, *pg;
	struct pglist		 pageq;
	int			 pdaemon_woken;

	KASSERT((align & (align - 1)) == 0);
	pdaemon_woken = 0; /* Didn't wake the pagedaemon. */

	/*
	 * Fixup arguments: align must be at least PAGE_SIZE,
	 * sz will be converted to pagecount, since that is what
	 * pmemrange uses internally.
	 */
	if (align < PAGE_SIZE)
		align = PAGE_SIZE;
	sz = atop(round_page(sz));

	uvm_lock_fpageq();

	TAILQ_FOREACH_REVERSE(pmr, &uvm.pmr_control.use, uvm_pmemrange_use,
	    pmr_use) {
retry:
		/*
		 * Search for a range with enough space.
		 * Use the address tree, to ensure the range is as low as
		 * possible.
		 */
		RB_FOREACH(pig_pg, uvm_pmr_addr, &pmr->addr) {
			pg_addr = VM_PAGE_TO_PHYS(pig_pg);
			piglet_addr = (pg_addr + (align - 1)) & ~(align - 1);

			if (pig_pg->fpgsz >= sz) {
				goto found;
			}

			if (atop(pg_addr) + pig_pg->fpgsz >
			    atop(piglet_addr) + sz) {
				goto found;
			}
		}

		/*
		 * Try to coerse the pagedaemon into freeing memory
		 * for the piglet.
		 *
		 * pdaemon_woken is set to prevent the code from
		 * falling into an endless loop.
		 */
		if (!pdaemon_woken) {
			pdaemon_woken = 1;
			if (uvm_wait_pla(ptoa(pmr->low), ptoa(pmr->high) - 1,
			    ptoa(sz), UVM_PLA_FAILOK) == 0)
				goto retry;
		}
	}

	/* Return failure. */
	uvm_unlock_fpageq();
	return ENOMEM;

found:
	/*
	 * Extract piglet from pigpen.
	 */
	TAILQ_INIT(&pageq);
	uvm_pmr_extract_range(pmr, pig_pg,
	    atop(piglet_addr), atop(piglet_addr) + sz, &pageq);

	*addr = piglet_addr;
	uvmexp.free -= sz;

	/*
	 * Update pg flags.
	 *
	 * Note that we trash the sz argument now.
	 */
	TAILQ_FOREACH(pg, &pageq, pageq) {
		KASSERT(pg->pg_flags & PQ_FREE);

		atomic_clearbits_int(&pg->pg_flags,
		    PG_PMAP0|PG_PMAP1|PG_PMAP2|PG_PMAP3);

		if (pg->pg_flags & PG_ZERO)
			uvmexp.zeropages--;
		atomic_clearbits_int(&pg->pg_flags,
		    PG_ZERO|PQ_FREE);

		pg->uobject = NULL;
		pg->uanon = NULL;
		pg->pg_version++;
	}

	uvm_unlock_fpageq();
	return 0;
}

/*
 * Physmem RLE compression support.
 *
 * Given a physical page address, it will return the number of pages 
 * starting at the address, that are free.
 * Returns 0 if the page at addr is not free.
 */
psize_t
uvm_page_rle(paddr_t addr)
{
	struct vm_page		*pg, *pg_end;
	struct vm_physseg	*vmp;
	int			 pseg_idx, off_idx;

	pseg_idx = vm_physseg_find(atop(addr), &off_idx);
	if (pseg_idx == -1)
		return 0;

	vmp = &vm_physmem[pseg_idx];
	pg = &vmp->pgs[off_idx];
	if (!(pg->pg_flags & PQ_FREE))
		return 0;

	/*
	 * Search for the first non-free page after pg.
	 * Note that the page may not be the first page in a free pmemrange,
	 * therefore pg->fpgsz cannot be used.
	 */
	for (pg_end = pg; pg_end <= vmp->lastpg &&
	    (pg_end->pg_flags & PQ_FREE) == PQ_FREE; pg_end++);
	return pg_end - pg;
}

/*
 * get_hibernate_info
 *
 * Fills out the hibernate_info union pointed to by hiber_info
 * with information about this machine (swap signature block
 * offsets, number of memory ranges, kernel in use, etc)
 *
 */
int
get_hibernate_info(union hibernate_info *hiber_info)
{
	int chunktable_size;
	struct disklabel dl;
	char err_string[128], *dl_ret;

	/* Determine I/O function to use */
	hiber_info->io_func = get_hibernate_io_function();
	if (hiber_info->io_func == NULL)
		return (1);

	/* Calculate hibernate device */
	hiber_info->device = swdevt[0].sw_dev;

	/* Read disklabel (used to calculate signature and image offsets) */
	dl_ret = disk_readlabel(&dl, hiber_info->device, err_string, 128);

	if (dl_ret) {
		printf("Hibernate error reading disklabel: %s\n", dl_ret);
		return (1);
	}

	hiber_info->secsize = dl.d_secsize;

	/* Make sure the signature can fit in one block */
	KASSERT(sizeof(union hibernate_info)/hiber_info->secsize == 1);

	/* Calculate swap offset from start of disk */
	hiber_info->swap_offset = dl.d_partitions[1].p_offset;

	/* Calculate signature block location */
	hiber_info->sig_offset = dl.d_partitions[1].p_offset +
		dl.d_partitions[1].p_size -
		sizeof(union hibernate_info)/hiber_info->secsize;

	chunktable_size = HIBERNATE_CHUNK_TABLE_SIZE / hiber_info->secsize;

	/* Calculate memory image location */
	hiber_info->image_offset = dl.d_partitions[1].p_offset +
		dl.d_partitions[1].p_size -
		(hiber_info->image_size / hiber_info->secsize) -
		sizeof(union hibernate_info)/hiber_info->secsize -
		chunktable_size;

	/* Stash kernel version information */
	bzero(&hiber_info->kernel_version, 128);
	bcopy(version, &hiber_info->kernel_version, 
		min(strlen(version), sizeof(hiber_info->kernel_version)-1));

	/* Allocate piglet region */
	if (uvm_pmr_alloc_piglet(&hiber_info->piglet_base, HIBERNATE_CHUNK_SIZE,
		HIBERNATE_CHUNK_SIZE)) {
		printf("Hibernate failed to allocate the piglet\n");
		return (1);
	}

	return get_hibernate_info_md(hiber_info);
}
