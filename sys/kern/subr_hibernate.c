/*	$OpenBSD: subr_hibernate.c,v 1.17 2011/09/21 06:13:39 mlarkin Exp $	*/

/*
 * Copyright (c) 2011 Ariane van der Steldt <ariane@stack.nl>
 * Copyright (c) 2011 Mike Larkin <mlarkin@openbsd.org>
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
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <uvm/uvm.h>
#include <machine/hibernate.h>

extern char *disk_readlabel(struct disklabel *, dev_t, char *, size_t);

struct hibernate_zlib_state *hibernate_state;

/* Temporary vaddr ranges used during hibernate */
vaddr_t hibernate_temp_page;
vaddr_t hibernate_copy_page;
vaddr_t hibernate_stack_page;
vaddr_t hibernate_fchunk_area;
vaddr_t	hibernate_chunktable_area;
vaddr_t hibernate_inflate_page;

/* Hibernate info as read from disk during resume */
union hibernate_info disk_hiber_info;

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
uvm_pmr_alloc_piglet(vaddr_t *va, paddr_t *pa, vsize_t sz, paddr_t align)
{
	paddr_t			 pg_addr, piglet_addr;
	struct uvm_pmemrange	*pmr;
	struct vm_page		*pig_pg, *pg;
	struct pglist		 pageq;
	int			 pdaemon_woken;
	vaddr_t			 piglet_va;

	KASSERT((align & (align - 1)) == 0);
	pdaemon_woken = 0; /* Didn't wake the pagedaemon. */

	/*
	 * Fixup arguments: align must be at least PAGE_SIZE,
	 * sz will be converted to pagecount, since that is what
	 * pmemrange uses internally.
	 */
	if (align < PAGE_SIZE)
		align = PAGE_SIZE;
	sz = round_page(sz);

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

			if (atop(pg_addr) + pig_pg->fpgsz >=
			    atop(piglet_addr) + atop(sz)) {
				goto found;
			}
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
		    sz, UVM_PLA_FAILOK) == 0)
			goto retry;
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
	    atop(piglet_addr), atop(piglet_addr) + atop(sz), &pageq);

	*pa = piglet_addr;
	uvmexp.free -= atop(sz);

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


	/*
	 * Now allocate a va.
	 * Use direct mappings for the pages.
	 */

	piglet_va = *va = (vaddr_t)km_alloc(sz, &kv_any, &kp_none, &kd_waitok);
	if (!piglet_va) {
		uvm_pglistfree(&pageq);
		return ENOMEM;
	}

	/*
	 * Map piglet to va.
	 */
	TAILQ_FOREACH(pg, &pageq, pageq) {
		pmap_kenter_pa(piglet_va, VM_PAGE_TO_PHYS(pg), UVM_PROT_RW);
		piglet_va += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	return 0;
}

/*
 * Free a piglet area.
 */
void
uvm_pmr_free_piglet(vaddr_t va, vsize_t sz)
{
	paddr_t			 pa;
	struct vm_page		*pg;

	/*
	 * Fix parameters.
	 */
	sz = round_page(sz);

	/*
	 * Find the first page in piglet.
	 * Since piglets are contiguous, the first pg is all we need.
	 */
	if (!pmap_extract(pmap_kernel(), va, &pa))
		panic("uvm_pmr_free_piglet: piglet 0x%lx has no pages", va);
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		panic("uvm_pmr_free_piglet: unmanaged page 0x%lx", pa);

	/*
	 * Unmap.
	 */
	pmap_kremove(va, sz);
	pmap_update(pmap_kernel());

	/*
	 * Free the physical and virtual memory.
	 */
	uvm_pmr_freepages(pg, atop(sz));
	km_free((void*)va, sz, &kv_any, &kp_none);
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
 * Fills out the hibernate_info union pointed to by hiber_info
 * with information about this machine (swap signature block
 * offsets, number of memory ranges, kernel in use, etc)
 *
 */
int
get_hibernate_info(union hibernate_info *hiber_info, int suspend)
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

	/* Stash kernel version information */
	bzero(&hiber_info->kernel_version, 128);
	bcopy(version, &hiber_info->kernel_version, 
		min(strlen(version), sizeof(hiber_info->kernel_version)-1));

	if (suspend) {
		/* Allocate piglet region */
		if (uvm_pmr_alloc_piglet(&hiber_info->piglet_va,
					&hiber_info->piglet_pa,
					HIBERNATE_CHUNK_SIZE*3,
					HIBERNATE_CHUNK_SIZE)) {
			printf("Hibernate failed to allocate the piglet\n");
			return (1);
		}
	}

	if (get_hibernate_info_md(hiber_info))
		return (1);

	/* Calculate memory image location */
	hiber_info->image_offset = dl.d_partitions[1].p_offset +
		dl.d_partitions[1].p_size -
		(hiber_info->image_size / hiber_info->secsize) -
		sizeof(union hibernate_info)/hiber_info->secsize -
		chunktable_size;

	return (0);
}

/*
 * Allocate nitems*size bytes from the hiballoc area presently in use
 */ 
void
*hibernate_zlib_alloc(void *unused, int nitems, int size)
{
	return hib_alloc(&hibernate_state->hiballoc_arena, nitems*size);
}

/*
 * Free the memory pointed to by addr in the hiballoc area presently in
 * use
 */
void
hibernate_zlib_free(void *unused, void *addr)
{
	hib_free(&hibernate_state->hiballoc_arena, addr);
}

/*
 * Inflate size bytes from src into dest, skipping any pages in
 * [src..dest] that are special (see hibernate_inflate_skip)
 *
 * For each page of output data, we map HIBERNATE_TEMP_PAGE
 * to the current output page, and tell inflate() to inflate
 * its data there, resulting in the inflated data being placed
 * at the proper paddr.
 *
 * This function executes while using the resume-time stack
 * and pmap, and therefore cannot use ddb/printf/etc. Doing so
 * will likely hang or reset the machine.
 */
void
hibernate_inflate(union hibernate_info *hiber_info,
	paddr_t dest, paddr_t src, size_t size)
{
	int i;

	hibernate_state->hib_stream.avail_in = size;
	hibernate_state->hib_stream.next_in = (char *)src;

	hibernate_inflate_page = hiber_info->piglet_va + 2 * PAGE_SIZE;

	do {
		/* Flush cache and TLB */
		hibernate_flush();

		/*
		 * Is this a special page? If yes, redirect the
		 * inflate output to a scratch page (eg, discard it)
		 */
		if (hibernate_inflate_skip(hiber_info, dest))
			hibernate_enter_resume_mapping(
				hibernate_inflate_page,
				hiber_info->piglet_pa + 2 * PAGE_SIZE,
				0);
		else
			hibernate_enter_resume_mapping(
				hibernate_inflate_page,
				dest, 0);

		/* Set up the stream for inflate */
		hibernate_state->hib_stream.avail_out = PAGE_SIZE;
		hibernate_state->hib_stream.next_out =
			(char *)hiber_info->piglet_va + 2 * PAGE_SIZE;

		/* Process next block of data */
		i = inflate(&hibernate_state->hib_stream, Z_PARTIAL_FLUSH);
		if (i != Z_OK && i != Z_STREAM_END) {
			/*
			 * XXX - this will likely reboot/hang most machines,
			 *       but there's not much else we can do here.
			 */
			panic("inflate error");
		}

		dest += PAGE_SIZE - hibernate_state->hib_stream.avail_out;
	} while (i != Z_STREAM_END);
}

/*
 * deflate from src into the I/O page, up to 'remaining' bytes
 *
 * Returns number of input bytes consumed, and may reset
 * the 'remaining' parameter if not all the output space was consumed
 * (this information is needed to know how much to write to disk
 */
size_t
hibernate_deflate(union hibernate_info *hiber_info, paddr_t src,
	size_t *remaining)
{
	vaddr_t hibernate_io_page = hiber_info->piglet_va + PAGE_SIZE;

	/* Set up the stream for deflate */
	hibernate_state->hib_stream.avail_in = PAGE_SIZE -
		(src & PAGE_MASK);
	hibernate_state->hib_stream.avail_out = *remaining;
	hibernate_state->hib_stream.next_in = (caddr_t)src;
	hibernate_state->hib_stream.next_out = (caddr_t)hibernate_io_page +
		(PAGE_SIZE - *remaining);

	/* Process next block of data */
	if (deflate(&hibernate_state->hib_stream, Z_PARTIAL_FLUSH) != Z_OK)
		panic("hibernate zlib deflate error\n");

	/* Update pointers and return number of bytes consumed */
	*remaining = hibernate_state->hib_stream.avail_out;
	return (PAGE_SIZE - (src & PAGE_MASK)) -
		hibernate_state->hib_stream.avail_in;
}

/*
 * Write the hibernation information specified in hiber_info
 * to the location in swap previously calculated (last block of
 * swap), called the "signature block".
 *
 * Write the memory chunk table to the area in swap immediately
 * preceding the signature block.
 */
int
hibernate_write_signature(union hibernate_info *hiber_info)
{
	u_int8_t *io_page;
	int result = 0;

	io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!io_page)
		return (1);

	/* Write hibernate info to disk */
	if (hiber_info->io_func(hiber_info->device, hiber_info->sig_offset,
		(vaddr_t)hiber_info, hiber_info->secsize, 1, io_page)) {
			result = 1;
	}

	free(io_page, M_DEVBUF);
	return (result);
}

/*
 * Write the memory chunk table to the area in swap immediately
 * preceding the signature block. The chunk table is stored
 * in the piglet when this function is called.
 */
int
hibernate_write_chunktable(union hibernate_info *hiber_info)
{
	u_int8_t *io_page;
	int i;
	daddr_t chunkbase;
	vaddr_t hibernate_chunk_table_start;
	size_t hibernate_chunk_table_size;
	struct hibernate_disk_chunk *chunks;

	io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!io_page)
		return (1);

	hibernate_chunk_table_size = HIBERNATE_CHUNK_TABLE_SIZE;

	chunkbase = hiber_info->sig_offset -
		    (hibernate_chunk_table_size / hiber_info->secsize);

	hibernate_chunk_table_start = hiber_info->piglet_va +
					HIBERNATE_CHUNK_SIZE;

	chunks = (struct hibernate_disk_chunk *)(hiber_info->piglet_va +
		HIBERNATE_CHUNK_SIZE);

	/* Write chunk table */
	for(i=0; i < hibernate_chunk_table_size; i += MAXPHYS) {
		if(hiber_info->io_func(hiber_info->device,
			chunkbase + (i/hiber_info->secsize),
			(vaddr_t)(hibernate_chunk_table_start + i),
			MAXPHYS,
			1,
			io_page)) {
				free(io_page, M_DEVBUF);
				return (1);
		}
	}

	free(io_page, M_DEVBUF);

	return (0);
}

/*
 * Write an empty hiber_info to the swap signature block, which is
 * guaranteed to not match any valid hiber_info.
 */
int
hibernate_clear_signature()
{
	union hibernate_info blank_hiber_info;
	union hibernate_info hiber_info;
	u_int8_t *io_page;

	/* Zero out a blank hiber_info */
	bzero(&blank_hiber_info, sizeof(hiber_info));

	if (get_hibernate_info(&hiber_info, 0))
		return (1);

	io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!io_page)
		return (1);

	/* Write (zeroed) hibernate info to disk */
	/* XXX - use regular kernel write routine for this */
	if(hiber_info.io_func(hiber_info.device, hiber_info.sig_offset,
		(vaddr_t)&blank_hiber_info, hiber_info.secsize, 1, io_page))
			panic("error hibernate write 6\n");

	free(io_page, M_DEVBUF);
	
	return (0);
}

/*
 * Check chunk range overlap when calculating whether or not to copy a
 * compressed chunk to the piglet area before decompressing.
 *
 * returns zero if the ranges do not overlap, non-zero otherwise.
 */
int
hibernate_check_overlap(paddr_t r1s, paddr_t r1e, paddr_t r2s, paddr_t r2e)
{
	/* case A : end of r1 overlaps start of r2 */
	if (r1s < r2s && r1e > r2s) 
		return (1);

	/* case B : r1 entirely inside r2 */
	if (r1s >= r2s && r1e <= r2e)
		return (1);

	/* case C : r2 entirely inside r1 */
	if (r2s >= r1s && r2e <= r1e)
		return (1);

	/* case D : end of r2 overlaps start of r1 */
	if (r2s < r1s && r2e > r1s)
		return (1);

	return (0);
}

/*
 * Compare two hibernate_infos to determine if they are the same (eg,
 * we should be performing a hibernate resume on this machine.
 * Not all fields are checked - just enough to verify that the machine
 * has the same memory configuration and kernel as the one that
 * wrote the signature previously.
 */
int
hibernate_compare_signature(union hibernate_info *mine,
	union hibernate_info *disk)
{
	u_int i;

	if (mine->nranges != disk->nranges)
		return (1);

	if (strcmp(mine->kernel_version, disk->kernel_version) != 0)
		return (1);

	for (i=0; i< mine->nranges; i++) {
		if ((mine->ranges[i].base != disk->ranges[i].base) ||
			(mine->ranges[i].end != disk->ranges[i].end) )
		return (1);
	}

	return (0);
}

/*
 * Reads read_size bytes from the hibernate device specified in
 * hib_info at offset blkctr. Output is placed into the vaddr specified
 * at dest.
 *
 * Separate offsets and pages are used to handle misaligned reads (reads
 * that span a page boundary).
 *
 * blkctr specifies a relative offset (relative to the start of swap),
 * not an absolute disk offset
 *
 */
int
hibernate_read_block(union hibernate_info *hib_info, daddr_t blkctr,
	size_t read_size, vaddr_t dest)
{
	struct buf *bp;
	struct bdevsw *bdsw;
	int error;

	bp = geteblk(read_size);
	bdsw = &bdevsw[major(hib_info->device)];

	error = (*bdsw->d_open)(hib_info->device, FREAD, S_IFCHR, curproc);
	if (error) {
		printf("hibernate_read_block open failed\n");
		return (1);
	}

	bp->b_bcount = read_size;
	bp->b_blkno = blkctr;
	CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
	bp->b_dev = hib_info->device;
	bp->b_cylinder = 0;
	(*bdsw->d_strategy)(bp);

	error = biowait(bp);
	if (error) {
		printf("hibernate_read_block biowait failed %d\n", error);
		error = (*bdsw->d_close)(hib_info->device, 0, S_IFCHR,
				curproc);
		if (error) 
			printf("hibernate_read_block error close failed\n");
		return (1);
	}

	error = (*bdsw->d_close)(hib_info->device, FREAD, S_IFCHR, curproc);
	if (error) {
		printf("hibernate_read_block close failed\n");
		return (1);
	}

	bcopy(bp->b_data, (caddr_t)dest, read_size);

	bp->b_flags |= B_INVAL;
	brelse(bp);

	return (0);
}

/*
 * Reads the signature block from swap, checks against the current machine's
 * information. If the information matches, perform a resume by reading the
 * saved image into the pig area, and unpacking.
 */
void
hibernate_resume()
{
	union hibernate_info hiber_info;
	u_int8_t *io_page;
	int s;

	/* Scrub temporary vaddr ranges used during resume */
	hibernate_temp_page = (vaddr_t)NULL;
	hibernate_fchunk_area = (vaddr_t)NULL;
	hibernate_chunktable_area = (vaddr_t)NULL;
	hibernate_stack_page = (vaddr_t)NULL;

	/* Get current running machine's hibernate info */
	bzero(&hiber_info, sizeof(hiber_info));
	if (get_hibernate_info(&hiber_info, 0))
		return;

	io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!io_page)
		return;

	/* Read hibernate info from disk */
	s = splbio();

	/* XXX use regular kernel read routine here */
	if(hiber_info.io_func(hiber_info.device, hiber_info.sig_offset,
				(vaddr_t)&disk_hiber_info,
				hiber_info.secsize, 0, io_page))
			panic("error in hibernate read\n");

	free(io_page, M_DEVBUF);

	/*
	 * If on-disk and in-memory hibernate signatures match,
	 * this means we should do a resume from hibernate.
	 */
	if (hibernate_compare_signature(&hiber_info,
		&disk_hiber_info))
		return;

	/*
	 * Allocate several regions of vaddrs for use during read.
	 * These mappings go into the resuming kernel's page table, and are
	 * used only during image read.
	 */
	hibernate_temp_page = (vaddr_t)km_alloc(2*PAGE_SIZE, &kv_any,
						&kp_none, &kd_nowait);
	if (!hibernate_temp_page)
		goto fail;

	hibernate_fchunk_area = (vaddr_t)km_alloc(3*PAGE_SIZE, &kv_any,
						&kp_none, &kd_nowait);
	if (!hibernate_fchunk_area)
		goto fail;

	/* Allocate a temporary chunktable area */
	hibernate_chunktable_area = (vaddr_t)malloc(HIBERNATE_CHUNK_TABLE_SIZE,
					   M_DEVBUF, M_NOWAIT);
	if (!hibernate_chunktable_area)
		goto fail;

	/* Allocate one temporary page of VAs for the resume time stack */
	hibernate_stack_page = (vaddr_t)km_alloc(PAGE_SIZE, &kv_any,
						&kp_none, &kd_nowait);
	if (!hibernate_stack_page)
		goto fail;

	/* Read the image from disk into the image (pig) area */
	if (hibernate_read_image(&disk_hiber_info))
		goto fail;

	/* Point of no return ... */

	disable_intr();
	cold = 1;

	/* Switch stacks */
	hibernate_switch_stack_machdep();

	/*
	 * Image is now in high memory (pig area), copy to correct location
	 * in memory. We'll eventually end up copying on top of ourself, but
	 * we are assured the kernel code here is the same between the
	 * hibernated and resuming kernel, and we are running on our own
	 * stack, so the overwrite is ok.
	 */
	hibernate_unpack_image(&disk_hiber_info);	
	
	/*
	 * Resume the loaded kernel by jumping to the MD resume vector.
	 * We won't be returning from this call.
	 */
	hibernate_resume_machdep();

fail:
	printf("Unable to resume hibernated image\n");	

	if (hibernate_temp_page)
		km_free((void *)hibernate_temp_page, 2*PAGE_SIZE, &kv_any,
			&kp_none);

	if (hibernate_fchunk_area)
		km_free((void *)hibernate_fchunk_area, 3*PAGE_SIZE, &kv_any,
			&kp_none);

	if (io_page)
		free((void *)io_page, M_DEVBUF);

	if (hibernate_chunktable_area)
		free((void *)hibernate_chunktable_area, M_DEVBUF);
}

/*
 * Unpack image from pig area to original location by looping through the
 * list of output chunks in the order they should be restored (fchunks).
 * This ordering is used to avoid having inflate overwrite a chunk in the
 * middle of processing that chunk. This will, of course, happen during the
 * final output chunk, where we copy the chunk to the piglet area first,
 * before inflating.
 */
void
hibernate_unpack_image(union hibernate_info *hiber_info)
{
	int i;
	paddr_t image_cur;
	vaddr_t tempva;
	struct hibernate_disk_chunk *chunks;
	char *pva;
	int *fchunks;

	pva = (char *)hiber_info->piglet_va;

	fchunks = (int *)(pva + (4 * PAGE_SIZE));

	/* Copy temporary chunktable to piglet */
	tempva = (vaddr_t)km_alloc(HIBERNATE_CHUNK_TABLE_SIZE, &kv_any,
			&kp_none, &kd_nowait);
	for (i=0; i<HIBERNATE_CHUNK_TABLE_SIZE; i += PAGE_SIZE)
		pmap_kenter_pa(tempva + i, hiber_info->piglet_pa +
			HIBERNATE_CHUNK_SIZE + i, VM_PROT_ALL);

	bcopy((caddr_t)hibernate_chunktable_area, (caddr_t)tempva,
		HIBERNATE_CHUNK_TABLE_SIZE);

	chunks = (struct hibernate_disk_chunk *)(pva +  HIBERNATE_CHUNK_SIZE);

	hibernate_activate_resume_pt_machdep();

	for (i=0; i<hiber_info->chunk_ctr; i++) {
		/* Reset zlib for inflate */
		if (hibernate_zlib_reset(hiber_info, 0) != Z_OK)
			panic("hibernate failed to reset zlib for inflate\n");

		/*	
		 * If there is a conflict, copy the chunk to the piglet area
		 * before unpacking it to its original location.
		 */
		if((chunks[fchunks[i]].flags & HIBERNATE_CHUNK_CONFLICT) == 0) 
			hibernate_inflate(hiber_info,
				chunks[fchunks[i]].base, image_cur,
				chunks[fchunks[i]].compressed_size);
		else {
			bcopy((caddr_t)image_cur,
				(caddr_t)hiber_info->piglet_va +
				HIBERNATE_CHUNK_SIZE * 2,
				chunks[fchunks[i]].compressed_size);
			hibernate_inflate(hiber_info,
				chunks[fchunks[i]].base,
				hiber_info->piglet_va +
				HIBERNATE_CHUNK_SIZE * 2,
				chunks[fchunks[i]].compressed_size);
		}
		image_cur += chunks[fchunks[i]].compressed_size;
	}
}

/*
 * Write a compressed version of this machine's memory to disk, at the
 * precalculated swap offset:
 *
 * end of swap - signature block size - chunk table size - memory size
 *
 * The function begins by looping through each phys mem range, cutting each
 * one into 4MB chunks. These chunks are then compressed individually
 * and written out to disk, in phys mem order. Some chunks might compress
 * more than others, and for this reason, each chunk's size is recorded
 * in the chunk table, which is written to disk after the image has
 * properly been compressed and written (in hibernate_write_chunktable).
 *
 * When this function is called, the machine is nearly suspended - most
 * devices are quiesced/suspended, interrupts are off, and cold has
 * been set. This means that there can be no side effects once the
 * write has started, and the write function itself can also have no
 * side effects.
 *
 * This function uses the piglet area during this process as follows:
 *
 * offset from piglet base	use
 * -----------------------	--------------------
 * 0				i/o allocation area
 * PAGE_SIZE			i/o write area
 * 2*PAGE_SIZE			temp/scratch page
 * 3*PAGE_SIZE			temp/scratch page
 * 4*PAGE_SIZE			hiballoc arena
 * 5*PAGE_SIZE to 85*PAGE_SIZE	zlib deflate area
 * ...
 * HIBERNATE_CHUNK_SIZE		chunk table temporary area
 *
 * Some transient piglet content is saved as part of deflate,
 * but it is irrelevant during resume as it will be repurposed 
 * at that time for other things.
 */
int
hibernate_write_chunks(union hibernate_info *hiber_info)
{
	paddr_t range_base, range_end, inaddr, temp_inaddr;
	daddr_t blkctr;
	int i;
	size_t nblocks, out_remaining, used, offset;
	struct hibernate_disk_chunk *chunks;
	vaddr_t hibernate_alloc_page = hiber_info->piglet_va;
	vaddr_t hibernate_io_page = hiber_info->piglet_va + PAGE_SIZE;

	blkctr = hiber_info->image_offset;
	hiber_info->chunk_ctr = 0;
	offset = 0;

	/*
	 * Allocate VA for the temp and copy page.
	 */

	hibernate_temp_page = (vaddr_t)km_alloc(PAGE_SIZE, &kv_any,
						&kp_none, &kd_nowait);
	if (!hibernate_temp_page)
		return (1);

	hibernate_copy_page = (vaddr_t)km_alloc(PAGE_SIZE, &kv_any,
						&kp_none, &kd_nowait);
	if (!hibernate_copy_page)
		return (1);

	pmap_kenter_pa(hibernate_copy_page,
			(hiber_info->piglet_pa + 3*PAGE_SIZE),
			VM_PROT_ALL);

	/* XXX - needed on i386. check other archs */
	pmap_activate(curproc);

	chunks = (struct hibernate_disk_chunk *)(hiber_info->piglet_va +
			HIBERNATE_CHUNK_SIZE);

	/* Calculate the chunk regions */
	for (i=0; i < hiber_info->nranges; i++) {
                range_base = hiber_info->ranges[i].base;
                range_end = hiber_info->ranges[i].end;

		inaddr = range_base;

		while (inaddr < range_end) {
			chunks[hiber_info->chunk_ctr].base = inaddr;
			if (inaddr + HIBERNATE_CHUNK_SIZE < range_end)
				chunks[hiber_info->chunk_ctr].end = inaddr +
					HIBERNATE_CHUNK_SIZE;
			else
				chunks[hiber_info->chunk_ctr].end = range_end;

			inaddr += HIBERNATE_CHUNK_SIZE;
			hiber_info->chunk_ctr ++;
		}
	} 

	/* Compress and write the chunks in the chunktable */
	for (i=0; i < hiber_info->chunk_ctr; i++) {
		range_base = chunks[i].base;
		range_end = chunks[i].end;

		chunks[i].offset = blkctr; 

		/* Reset zlib for deflate */
		if (hibernate_zlib_reset(hiber_info, 1) != Z_OK)
			return (1);

		inaddr = range_base;

		/*
		 * For each range, loop through its phys mem region
		 * and write out the chunks (the last chunk might be
		 * smaller than the chunk size).
		 */
		while (inaddr < range_end) {
			out_remaining = PAGE_SIZE;
			while (out_remaining > 0 && inaddr < range_end) {
				pmap_kenter_pa(hibernate_temp_page,
					inaddr & PMAP_PA_MASK, VM_PROT_ALL);
				pmap_activate(curproc);

				bcopy((caddr_t)hibernate_temp_page,
					(caddr_t)hibernate_copy_page, PAGE_SIZE);

				/* Adjust for non page-sized regions */
				temp_inaddr = (inaddr & PAGE_MASK) +
					hibernate_copy_page;

				/* Deflate from temp_inaddr to IO page */
				inaddr += hibernate_deflate(hiber_info,
						temp_inaddr,
						&out_remaining);
			}

			if (out_remaining == 0) {
				/* Filled up the page */
				nblocks = PAGE_SIZE / hiber_info->secsize;

				if(hiber_info->io_func(hiber_info->device, blkctr,
					(vaddr_t)hibernate_io_page, PAGE_SIZE,
					1, (void *)hibernate_alloc_page))
						return (1);

				blkctr += nblocks;
			}
			
		}

		if (inaddr != range_end)
			return (1);

		/*
		 * End of range. Round up to next secsize bytes
		 * after finishing compress
		 */
		if (out_remaining == 0)
			out_remaining = PAGE_SIZE;

		/* Finish compress */
		hibernate_state->hib_stream.avail_in = 0;
		hibernate_state->hib_stream.avail_out = out_remaining;
		hibernate_state->hib_stream.next_in = (caddr_t)inaddr;
		hibernate_state->hib_stream.next_out = 
			(caddr_t)hibernate_io_page + (PAGE_SIZE - out_remaining);

		if (deflate(&hibernate_state->hib_stream, Z_FINISH) !=
			Z_STREAM_END)
				return (1);

		out_remaining = hibernate_state->hib_stream.avail_out;

		used = PAGE_SIZE - out_remaining;
		nblocks = used / hiber_info->secsize;

		/* Round up to next block if needed */
		if (used % hiber_info->secsize != 0)
			nblocks ++;

		/* Write final block(s) for this chunk */
		if( hiber_info->io_func(hiber_info->device, blkctr,
			(vaddr_t)hibernate_io_page, nblocks*hiber_info->secsize,
			1, (void *)hibernate_alloc_page))
				return (1);

		blkctr += nblocks;

		offset = blkctr;
		chunks[i].compressed_size=
			(offset-chunks[i].offset)*hiber_info->secsize;

	}

	return (0);
}

/*
 * Reset the zlib stream state and allocate a new hiballoc area for either
 * inflate or deflate. This function is called once for each hibernate chunk.
 * Calling hiballoc_init multiple times is acceptable since the memory it is
 * provided is unmanaged memory (stolen). We use the memory provided to us
 * by the piglet allocated via the supplied hiber_info.
 */
int
hibernate_zlib_reset(union hibernate_info *hiber_info, int deflate)
{
	vaddr_t hibernate_zlib_start;
	size_t hibernate_zlib_size;

	hibernate_state = (struct hibernate_zlib_state *)hiber_info->piglet_va +
				(4 * PAGE_SIZE);

	hibernate_zlib_start = hiber_info->piglet_va + (5 * PAGE_SIZE);
	hibernate_zlib_size = 80 * PAGE_SIZE;

	bzero((caddr_t)hibernate_zlib_start, hibernate_zlib_size);
	bzero((caddr_t)hibernate_state, PAGE_SIZE);

	/* Set up stream structure */
	hibernate_state->hib_stream.zalloc = (alloc_func)hibernate_zlib_alloc;
	hibernate_state->hib_stream.zfree = (free_func)hibernate_zlib_free;

	/* Initialize the hiballoc arena for zlib allocs/frees */
	hiballoc_init(&hibernate_state->hiballoc_arena,
		(caddr_t)hibernate_zlib_start, hibernate_zlib_size);

	if (deflate) {
		return deflateInit(&hibernate_state->hib_stream,
			Z_DEFAULT_COMPRESSION);
	}
	else
		return inflateInit(&hibernate_state->hib_stream);
}

/*
 * Reads the hibernated memory image from disk, whose location and
 * size are recorded in hiber_info. Begin by reading the persisted
 * chunk table, which records the original chunk placement location
 * and compressed size for each. Next, allocate a pig region of
 * sufficient size to hold the compressed image. Next, read the
 * chunks into the pig area (calling hibernate_read_chunks to do this),
 * and finally, if all of the above succeeds, clear the hibernate signature.
 * The function will then return to hibernate_resume, which will proceed
 * to unpack the pig image to the correct place in memory.
 */
int
hibernate_read_image(union hibernate_info *hiber_info)
{
	int i;
	paddr_t image_start, image_end, pig_start, pig_end;
	daddr_t blkctr;
	struct hibernate_disk_chunk *chunks;
	size_t compressed_size, disk_size, chunktable_size, pig_sz;

	/* Calculate total chunk table size in disk blocks */
	chunktable_size = HIBERNATE_CHUNK_TABLE_SIZE / hiber_info->secsize;

	blkctr = hiber_info->sig_offset - chunktable_size -
			hiber_info->swap_offset;

	for(i=0; i < HIBERNATE_CHUNK_TABLE_SIZE;
	    i += MAXPHYS, blkctr += MAXPHYS/hiber_info->secsize)
		hibernate_read_block(hiber_info, blkctr, MAXPHYS,
			hibernate_chunktable_area + i);

	blkctr = hiber_info->image_offset;
	compressed_size = 0;
	chunks = (struct hibernate_disk_chunk *)hibernate_chunktable_area;

	for (i=0; i<hiber_info->chunk_ctr; i++)
		compressed_size += chunks[i].compressed_size;

	disk_size = compressed_size;

	/* Allocate the pig area */
	pig_sz =  compressed_size + HIBERNATE_CHUNK_SIZE;
	if (uvm_pmr_alloc_pig(&pig_start, pig_sz) == ENOMEM)
		return (1);

	pig_end = pig_start + pig_sz;

	/* Calculate image extents. Pig image must end on a chunk boundary. */
	image_end = pig_end & ~(HIBERNATE_CHUNK_SIZE - 1);
	image_start = pig_start;

	image_start = image_end - disk_size;

	hibernate_read_chunks(hiber_info, image_start, image_end, disk_size);

	/* Prepare the resume time pmap/page table */
	hibernate_populate_resume_pt(hiber_info, image_start, image_end);

	/* Read complete, clear the signature and return */
	return hibernate_clear_signature();
}

/*
 * Read the hibernated memory chunks from disk (chunk information at this
 * point is stored in the piglet) into the pig area specified by
 * [pig_start .. pig_end]. Order the chunks so that the final chunk is the
 * only chunk with overlap possibilities.
 *
 * This function uses the piglet area during this process as follows:
 *
 * offset from piglet base	use
 * -----------------------	--------------------
 * 0				i/o allocation area
 * PAGE_SIZE			i/o write area
 * 2*PAGE_SIZE			temp/scratch page
 * 3*PAGE_SIZE			temp/scratch page
 * 4*PAGE_SIZE to 6*PAGE_SIZE	chunk ordering area
 * 7*PAGE_SIZE			hiballoc arena
 * 8*PAGE_SIZE to 88*PAGE_SIZE	zlib deflate area
 * ...
 * HIBERNATE_CHUNK_SIZE		chunk table temporary area
 */
int
hibernate_read_chunks(union hibernate_info *hib_info, paddr_t pig_start,
			paddr_t pig_end, size_t image_compr_size)
{
	paddr_t img_index, img_cur, r1s, r1e, r2s, r2e;
	paddr_t copy_start, copy_end, piglet_cur;
	paddr_t piglet_base = hib_info->piglet_pa;
	paddr_t piglet_end = piglet_base + HIBERNATE_CHUNK_SIZE;
	daddr_t blkctr;
	size_t processed, compressed_size, read_size;
	int i, j, overlap, found, nchunks, nochunks=0, nfchunks=0, npchunks=0;
	struct hibernate_disk_chunk *chunks;
	u_int8_t *ochunks, *pchunks, *fchunks;

	/* Map the chunk ordering region */
	pmap_kenter_pa(hibernate_fchunk_area,
		piglet_base + (4*PAGE_SIZE), VM_PROT_ALL);
	pmap_kenter_pa(hibernate_fchunk_area + PAGE_SIZE,
		piglet_base + (5*PAGE_SIZE), VM_PROT_ALL);
	pmap_kenter_pa(hibernate_fchunk_area + 2*PAGE_SIZE,
		piglet_base + (6*PAGE_SIZE),
	 	VM_PROT_ALL);

	/* Temporary output chunk ordering */
	ochunks = (u_int8_t *)hibernate_fchunk_area;

	/* Piglet chunk ordering */
	pchunks = (u_int8_t *)hibernate_fchunk_area + PAGE_SIZE;

	/* Final chunk ordering */
	fchunks = (u_int8_t *)hibernate_fchunk_area + 2*PAGE_SIZE;

	nchunks = hib_info->chunk_ctr;
	chunks = (struct hibernate_disk_chunk *)hibernate_chunktable_area;

	/* Initially start all chunks as unplaced */
	for (i=0; i < nchunks; i++)
		chunks[i].flags=0;

	/*
	 * Search the list for chunks that are outside the pig area. These
	 * can be placed first in the final output list.
	 */
	for (i=0; i < nchunks; i++) {
		if(chunks[i].end <= pig_start || chunks[i].base >= pig_end) { 
			ochunks[nochunks] = (u_int8_t)i;
			fchunks[nfchunks] = (u_int8_t)i;
			nochunks++;
			nfchunks++;
			chunks[i].flags |= HIBERNATE_CHUNK_USED;
		}
	}
 
	/*
	 * Walk the ordering, place the chunks in ascending memory order.
	 * Conflicts might arise, these are handled next.
	 */
	do {
		img_index = -1;
		found=0;
		j=-1;
		for (i=0; i < nchunks; i++)
			if (chunks[i].base < img_index && 
			    chunks[i].flags == 0 ) {
				j = i;
				img_index = chunks[i].base;
			}

		if (j != -1) {
			found = 1;
			ochunks[nochunks] = (short)j;
			nochunks++;
			chunks[j].flags |= HIBERNATE_CHUNK_PLACED;
		}
	} while (found);

	img_index=pig_start;

	/*
	 * Identify chunk output conflicts (chunks whose pig load area
	 * corresponds to their original memory placement location)
	 */
	for(i=0; i< nochunks ; i++) {
		overlap=0;
		r1s = img_index;
		r1e = img_index + chunks[ochunks[i]].compressed_size;
		r2s = chunks[ochunks[i]].base;
		r2e = chunks[ochunks[i]].end;

		overlap = hibernate_check_overlap(r1s, r1e, r2s, r2e);
		if (overlap)
 			chunks[ochunks[i]].flags |= HIBERNATE_CHUNK_CONFLICT;
		
		img_index += chunks[ochunks[i]].compressed_size;
	}

	/*
	 * Prepare the final output chunk list. Calculate an output
	 * inflate strategy for overlapping chunks if needed.
	 */
	img_index=pig_start;
	for (i=0; i < nochunks ; i++) {
		/*
		 * If a conflict is detected, consume enough compressed
		 * output chunks to fill the piglet
		 */
		if (chunks[ochunks[i]].flags & HIBERNATE_CHUNK_CONFLICT) {
			copy_start = piglet_base;
			copy_end = piglet_end;
			piglet_cur = piglet_base;
			npchunks = 0;
			j=i;
			while (copy_start < copy_end && j < nochunks) {
				piglet_cur += chunks[ochunks[j]].compressed_size;
				pchunks[npchunks] = ochunks[j];
				npchunks++;
				copy_start += chunks[ochunks[j]].compressed_size;
				img_index += chunks[ochunks[j]].compressed_size;
				i++;
				j++;
			}	

			piglet_cur = piglet_base;
			for (j=0; j < npchunks; j++) {
				piglet_cur += chunks[pchunks[j]].compressed_size;
				fchunks[nfchunks] = pchunks[j];
				chunks[pchunks[j]].flags |= HIBERNATE_CHUNK_USED;
				nfchunks++;
			}	
		} else {
			/*
			 * No conflict, chunk can be added without copying
			 */
			if ((chunks[ochunks[i]].flags &
			    HIBERNATE_CHUNK_USED) == 0) {
				fchunks[nfchunks] = ochunks[i];
				chunks[ochunks[i]].flags |= HIBERNATE_CHUNK_USED;
				nfchunks++;
			}
				
			img_index += chunks[ochunks[i]].compressed_size;
		}
	}

	img_index = pig_start;
	for(i=0 ; i< nfchunks; i++) {
		piglet_cur = piglet_base;
		img_index += chunks[fchunks[i]].compressed_size;
	}	

	img_cur = pig_start;
	
	for(i=0; i<nfchunks; i++) {
		blkctr = chunks[fchunks[i]].offset - hib_info->swap_offset;
		processed = 0;
		compressed_size = chunks[fchunks[i]].compressed_size;

		while (processed < compressed_size) {
			pmap_kenter_pa(hibernate_temp_page, img_cur,
				VM_PROT_ALL);
			pmap_kenter_pa(hibernate_temp_page + PAGE_SIZE,
				img_cur+PAGE_SIZE, VM_PROT_ALL);

			/* XXX - needed on i386. check other archs */
			pmap_activate(curproc);
			if (compressed_size - processed >= PAGE_SIZE)
				read_size = PAGE_SIZE;
			else
				read_size = compressed_size - processed;
			
			hibernate_read_block(hib_info, blkctr, read_size,
				hibernate_temp_page + (img_cur & PAGE_MASK));

			blkctr += (read_size / hib_info->secsize);

			hibernate_flush();
			pmap_kremove(hibernate_temp_page, PAGE_SIZE);
			pmap_kremove(hibernate_temp_page + PAGE_SIZE,
				PAGE_SIZE);
			processed += read_size;
			img_cur += read_size;
		}
	}

	return (0);
}

/*
 * Hibernating a machine comprises the following operations:
 *  1. Calculating this machine's hibernate_info information
 *  2. Allocating a piglet and saving the piglet's physaddr
 *  3. Calculating the memory chunks
 *  4. Writing the compressed chunks to disk
 *  5. Writing the chunk table
 *  6. Writing the signature block (hibernate_info)
 *
 * On most architectures, the function calling hibernate_suspend would 
 * then power off the machine using some MD-specific implementation.
 */
int
hibernate_suspend()
{
	union hibernate_info hib_info;

	/*
	 * Calculate memory ranges, swap offsets, etc. 
	 * This also allocates a piglet whose physaddr is stored in
	 * hib_info->piglet_pa and vaddr stored in hib_info->piglet_va
	 */
	if (get_hibernate_info(&hib_info, 1))
		return (1);

	/* XXX - Won't need to zero everything with RLE */
	uvm_pmr_zero_everything();

	if (hibernate_write_chunks(&hib_info))
		return (1);

	if (hibernate_write_chunktable(&hib_info))
		return (1);

	return hibernate_write_signature(&hib_info);
}
