/*	$OpenBSD: subr_hibernate.c,v 1.60 2013/09/29 15:47:35 mlarkin Exp $	*/

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
#include <sys/systm.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <uvm/uvm.h>
#include <uvm/uvm_swap.h>
#include <machine/hibernate.h>

/*
 * Hibernate piglet layout information
 *
 * The piglet is a scratch area of memory allocated by the suspending kernel.
 * Its phys and virt addrs are recorded in the signature block. The piglet is
 * used to guarantee an unused area of memory that can be used by the resuming
 * kernel for various things. The piglet is excluded during unpack operations.
 * The piglet size is presently 3*HIBERNATE_CHUNK_SIZE (typically 3*4MB).
 *
 * Offset from piglet_base	Purpose
 * ----------------------------------------------------------------------------
 * 0				I/O page used during resume
 * 1*PAGE_SIZE		 	I/O page used during hibernate suspend
 * 2*PAGE_SIZE			unused
 * 3*PAGE_SIZE			copy page used during hibernate suspend
 * 4*PAGE_SIZE			final chunk ordering list (8 pages)
 * 12*PAGE_SIZE			piglet chunk ordering list (8 pages)
 * 20*PAGE_SIZE			temp chunk ordering list (8 pages)
 * 28*PAGE_SIZE			start of hiballoc area
 * 108*PAGE_SIZE		end of hiballoc area (80 pages)
 * ...				unused
 * HIBERNATE_CHUNK_SIZE		start of hibernate chunk table
 * 2*HIBERNATE_CHUNK_SIZE	bounce area for chunks being unpacked
 * 3*HIBERNATE_CHUNK_SIZE	end of piglet
 */

/* Temporary vaddr ranges used during hibernate */
vaddr_t hibernate_temp_page;
vaddr_t hibernate_copy_page;

/* Hibernate info as read from disk during resume */
union hibernate_info disk_hiber_info;
paddr_t global_pig_start;
vaddr_t global_piglet_va;

void hibernate_copy_chunk_to_piglet(paddr_t, vaddr_t, size_t);

/*
 * Hib alloc enforced alignment.
 */
#define HIB_ALIGN		8 /* bytes alignment */

/*
 * sizeof builtin operation, but with alignment constraint.
 */
#define HIB_SIZEOF(_type)	roundup(sizeof(_type), HIB_ALIGN)

struct hiballoc_entry {
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
static __inline void *
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
hib_addr_to_entry(void *addr_param)
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
void *
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
	    (void *)((caddr_t)prev + HIB_SIZEOF(struct hiballoc_entry) +
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
	uvm_unlock_fpageq();
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

	/* Ensure align is a power of 2 */
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
			    atop(piglet_addr) + atop(sz))
				goto found;
		}
	}

	/*
	 * Try to coerce the pagedaemon into freeing memory
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
	km_free((void *)va, sz, &kv_any, &kp_none);
}

/*
 * Physmem RLE compression support.
 *
 * Given a physical page address, return the number of pages starting at the
 * address that are free.  Clamps to the number of pages in
 * HIBERNATE_CHUNK_SIZE. Returns 0 if the page at addr is not free.
 */
int
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
	    (pg_end->pg_flags & PQ_FREE) == PQ_FREE; pg_end++)
		;
	return min((pg_end - pg), HIBERNATE_CHUNK_SIZE/PAGE_SIZE);
}

/*
 * Fills out the hibernate_info union pointed to by hiber_info
 * with information about this machine (swap signature block
 * offsets, number of memory ranges, kernel in use, etc)
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

	/* Make sure we have a swap partition. */
	if (dl.d_partitions[1].p_fstype != FS_SWAP ||
	    dl.d_partitions[1].p_size == 0)
		return (1);

	hiber_info->secsize = dl.d_secsize;

	/* Make sure the signature can fit in one block */
	if(sizeof(union hibernate_info) > hiber_info->secsize)
		return (1);

	/* Magic number */
	hiber_info->magic = HIBERNATE_MAGIC;
	
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
		    &hiber_info->piglet_pa, HIBERNATE_CHUNK_SIZE*3,
		    HIBERNATE_CHUNK_SIZE)) {
			printf("Hibernate failed to allocate the piglet\n");
			return (1);
		}
		hiber_info->io_page = (void *)hiber_info->piglet_va;

		/*
		 * Initialization of the hibernate IO function for drivers
		 * that need to do prep work (such as allocating memory or
		 * setting up data structures that cannot safely be done
		 * during suspend without causing side effects). There is
		 * a matching HIB_DONE call performed after the write is
		 * completed.	
		 */
		if (hiber_info->io_func(hiber_info->device, 0,
		    (vaddr_t)NULL, 0, HIB_INIT, hiber_info->io_page))
			goto fail;

	} else {
		/*
		 * Resuming kernels use a regular I/O page since we won't
		 * have access to the suspended kernel's piglet VA at this
		 * point. No need to free this I/O page as it will vanish
		 * as part of the resume.
		 */
		hiber_info->io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
		if (!hiber_info->io_page)
			return (1);
	}


	if (get_hibernate_info_md(hiber_info))
		goto fail;

	/* Calculate memory image location in swap */
	hiber_info->image_offset = dl.d_partitions[1].p_offset +
	    dl.d_partitions[1].p_size -
	    (hiber_info->image_size / hiber_info->secsize) -
	    sizeof(union hibernate_info)/hiber_info->secsize -
	    chunktable_size;

	return (0);
fail:
	if (suspend)
		uvm_pmr_free_piglet(hiber_info->piglet_va,
		    HIBERNATE_CHUNK_SIZE * 3);

	return (1);
}

/*
 * Allocate nitems*size bytes from the hiballoc area presently in use
 */
void *
hibernate_zlib_alloc(void *unused, int nitems, int size)
{
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	return hib_alloc(&hibernate_state->hiballoc_arena, nitems*size);
}

/*
 * Free the memory pointed to by addr in the hiballoc area presently in
 * use
 */
void
hibernate_zlib_free(void *unused, void *addr)
{
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	hib_free(&hibernate_state->hiballoc_arena, addr);
}

/*
 * Gets the next RLE value from the image stream
 */
int
hibernate_get_next_rle(void)
{
	int rle, i;
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	/* Read RLE code */
	hibernate_state->hib_stream.next_out = (char *)&rle;
	hibernate_state->hib_stream.avail_out = sizeof(rle);

	i = inflate(&hibernate_state->hib_stream, Z_FULL_FLUSH);
	if (i != Z_OK && i != Z_STREAM_END) {
		/*
		 * XXX - this will likely reboot/hang most machines
		 *       since the console output buffer will be unmapped,
		 *       but there's not much else we can do here.
		 */
		panic("inflate rle error");
	}

	/* Sanity check what RLE value we got */
	if (rle > HIBERNATE_CHUNK_SIZE/PAGE_SIZE || rle < 0)
		panic("invalid RLE code");

	if (i == Z_STREAM_END)
		rle = -1;

	return rle;
}

/*
 * Inflate next page of data from the image stream
 */
int
hibernate_inflate_page(void)
{
	struct hibernate_zlib_state *hibernate_state;
	int i;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	/* Set up the stream for inflate */
	hibernate_state->hib_stream.next_out = (char *)HIBERNATE_INFLATE_PAGE;
	hibernate_state->hib_stream.avail_out = PAGE_SIZE;

	/* Process next block of data */
	i = inflate(&hibernate_state->hib_stream, Z_PARTIAL_FLUSH);
	if (i != Z_OK && i != Z_STREAM_END) {
		/*
		 * XXX - this will likely reboot/hang most machines
		 *       since the console output buffer will be unmapped,
		 *       but there's not much else we can do here.
		 */
		panic("inflate error");
	}

	/* We should always have extracted a full page ... */
	if (hibernate_state->hib_stream.avail_out != 0) {
		/*
		 * XXX - this will likely reboot/hang most machines
		 *       since the console output buffer will be unmapped,
		 *       but there's not much else we can do here.
		 */
		panic("incomplete page");
	}

	return (i == Z_STREAM_END);
}

/*
 * Inflate size bytes from src into dest, skipping any pages in
 * [src..dest] that are special (see hibernate_inflate_skip)
 *
 * This function executes while using the resume-time stack
 * and pmap, and therefore cannot use ddb/printf/etc. Doing so
 * will likely hang or reset the machine since the console output buffer
 * will be unmapped.
 */
void
hibernate_inflate_region(union hibernate_info *hiber_info, paddr_t dest,
    paddr_t src, size_t size)
{
	int end_stream = 0 ;
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	hibernate_state->hib_stream.next_in = (char *)src;
	hibernate_state->hib_stream.avail_in = size;

	do {
		/* Flush cache and TLB */
		hibernate_flush();

		/*
		 * Is this a special page? If yes, redirect the
		 * inflate output to a scratch page (eg, discard it)
		 */
		if (hibernate_inflate_skip(hiber_info, dest)) {
			hibernate_enter_resume_mapping(
			    HIBERNATE_INFLATE_PAGE,
			    HIBERNATE_INFLATE_PAGE, 0);
		} else {
			hibernate_enter_resume_mapping(
			    HIBERNATE_INFLATE_PAGE, dest, 0);
		}

		hibernate_flush();
		end_stream = hibernate_inflate_page();

		dest += PAGE_SIZE;
	} while (!end_stream);
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
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	/* Set up the stream for deflate */
	hibernate_state->hib_stream.next_in = (caddr_t)src;
	hibernate_state->hib_stream.avail_in = PAGE_SIZE - (src & PAGE_MASK);
	hibernate_state->hib_stream.next_out = (caddr_t)hibernate_io_page +
	    (PAGE_SIZE - *remaining);
	hibernate_state->hib_stream.avail_out = *remaining;

	/* Process next block of data */
	if (deflate(&hibernate_state->hib_stream, Z_PARTIAL_FLUSH) != Z_OK)
		panic("hibernate zlib deflate error");

	/* Update pointers and return number of bytes consumed */
	*remaining = hibernate_state->hib_stream.avail_out;
	return (PAGE_SIZE - (src & PAGE_MASK)) -
	    hibernate_state->hib_stream.avail_in;
}

/*
 * Write the hibernation information specified in hiber_info
 * to the location in swap previously calculated (last block of
 * swap), called the "signature block".
 */
int
hibernate_write_signature(union hibernate_info *hiber_info)
{
	/* Write hibernate info to disk */
	return (hiber_info->io_func(hiber_info->device, hiber_info->sig_offset,
	    (vaddr_t)hiber_info, hiber_info->secsize, HIB_W,
	    hiber_info->io_page));
}

/*
 * Write the memory chunk table to the area in swap immediately
 * preceding the signature block. The chunk table is stored
 * in the piglet when this function is called.
 *
 * Return values:
 *
 * 0   -  success
 * EIO -  I/O error writing the chunktable
 */
int
hibernate_write_chunktable(union hibernate_info *hiber_info)
{
	struct hibernate_disk_chunk *chunks;
	vaddr_t hibernate_chunk_table_start;
	size_t hibernate_chunk_table_size;
	daddr_t chunkbase;
	int i;

	hibernate_chunk_table_size = HIBERNATE_CHUNK_TABLE_SIZE;

	chunkbase = hiber_info->sig_offset -
	    (hibernate_chunk_table_size / hiber_info->secsize);

	hibernate_chunk_table_start = hiber_info->piglet_va +
	    HIBERNATE_CHUNK_SIZE;

	chunks = (struct hibernate_disk_chunk *)(hiber_info->piglet_va +
	    HIBERNATE_CHUNK_SIZE);

	/* Write chunk table */
	for (i = 0; i < hibernate_chunk_table_size; i += MAXPHYS) {
		if (hiber_info->io_func(hiber_info->device,
		    chunkbase + (i/hiber_info->secsize),
		    (vaddr_t)(hibernate_chunk_table_start + i),
		    MAXPHYS, HIB_W, hiber_info->io_page))
			return (EIO);
	}

	return (0);
}

/*
 * Write an empty hiber_info to the swap signature block, which is
 * guaranteed to not match any valid hiber_info.
 */
int
hibernate_clear_signature(void)
{
	union hibernate_info blank_hiber_info;
	union hibernate_info hiber_info;

	/* Zero out a blank hiber_info */
	bzero(&blank_hiber_info, sizeof(union hibernate_info));

	/* Get the signature block location */
	if (get_hibernate_info(&hiber_info, 0))
		return (1);

	/* Write (zeroed) hibernate info to disk */
#ifdef HIBERNATE_DEBUG
	printf("clearing hibernate signature block location: %lld\n",
		hiber_info.sig_offset - hiber_info.swap_offset);
#endif /* HIBERNATE_DEBUG */
	if (hibernate_block_io(&hiber_info,
	    hiber_info.sig_offset - hiber_info.swap_offset,
	    hiber_info.secsize, (vaddr_t)&blank_hiber_info, 1))
		printf("Warning: could not clear hibernate signature\n");

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

	if (mine->nranges != disk->nranges) {
#ifdef HIBERNATE_DEBUG
		printf("hibernate memory range count mismatch\n");
#endif
		return (1);
	}

	if (strcmp(mine->kernel_version, disk->kernel_version) != 0) {
#ifdef HIBERNATE_DEBUG
		printf("hibernate kernel version mismatch\n");
#endif
		return (1);
	}

	for (i = 0; i < mine->nranges; i++) {
		if ((mine->ranges[i].base != disk->ranges[i].base) ||
		    (mine->ranges[i].end != disk->ranges[i].end) ) {
#ifdef HIBERNATE_DEBUG
			printf("hib range %d mismatch [%p-%p != %p-%p]\n",
				i, mine->ranges[i].base, mine->ranges[i].end,
				disk->ranges[i].base, disk->ranges[i].end);
#endif
			return (1);
		}
	}

	return (0);
}

/*
 * Transfers xfer_size bytes between the hibernate device specified in
 * hib_info at offset blkctr and the vaddr specified at dest.
 *
 * Separate offsets and pages are used to handle misaligned reads (reads
 * that span a page boundary).
 *
 * blkctr specifies a relative offset (relative to the start of swap),
 * not an absolute disk offset
 *
 */
int
hibernate_block_io(union hibernate_info *hib_info, daddr_t blkctr,
    size_t xfer_size, vaddr_t dest, int iswrite)
{
	struct buf *bp;
	struct bdevsw *bdsw;
	int error;

	bp = geteblk(xfer_size);
	bdsw = &bdevsw[major(hib_info->device)];

	error = (*bdsw->d_open)(hib_info->device, FREAD, S_IFCHR, curproc);
	if (error) {
		printf("hibernate_block_io open failed\n");
		return (1);
	}

	if (iswrite)
		bcopy((caddr_t)dest, bp->b_data, xfer_size);

	bp->b_bcount = xfer_size;
	bp->b_blkno = blkctr;
	CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | (iswrite ? B_WRITE : B_READ) | B_RAW);
	bp->b_dev = hib_info->device;
	bp->b_cylinder = 0;
	(*bdsw->d_strategy)(bp);

	error = biowait(bp);
	if (error) {
		printf("hibernate_block_io biowait failed %d\n", error);
		error = (*bdsw->d_close)(hib_info->device, 0, S_IFCHR,
		    curproc);
		if (error)
			printf("hibernate_block_io error close failed\n");
		return (1);
	}

	error = (*bdsw->d_close)(hib_info->device, FREAD, S_IFCHR, curproc);
	if (error) {
		printf("hibernate_block_io close failed\n");
		return (1);
	}

	if (!iswrite)
		bcopy(bp->b_data, (caddr_t)dest, xfer_size);

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
hibernate_resume(void)
{
	union hibernate_info hiber_info;
	int s;

	/* Get current running machine's hibernate info */
	bzero(&hiber_info, sizeof(hiber_info));
	if (get_hibernate_info(&hiber_info, 0))
		return;

	/* Read hibernate info from disk */
	s = splbio();

#ifdef HIBERNATE_DEBUG
	printf("reading hibernate signature block location: %lld\n",
		hiber_info.sig_offset - hiber_info.swap_offset);
#endif /* HIBERNATE_DEBUG */

	if (hibernate_block_io(&hiber_info,
	    hiber_info.sig_offset - hiber_info.swap_offset,
	    hiber_info.secsize, (vaddr_t)&disk_hiber_info, 0))
		panic("error in hibernate read");

	/* Check magic number */
	if (disk_hiber_info.magic != HIBERNATE_MAGIC) {
		splx(s);
		return;
	}

	/*
	 * We (possibly) found a hibernate signature. Clear signature first,
	 * to prevent accidental resume or endless resume cycles later.
	 */
	if (hibernate_clear_signature()) {
		splx(s);
		return;
	}

	/*
	 * If on-disk and in-memory hibernate signatures match,
	 * this means we should do a resume from hibernate.
	 */
	if (hibernate_compare_signature(&hiber_info, &disk_hiber_info)) {
		splx(s);
		return;
	}

#ifdef MULTIPROCESSOR
	hibernate_quiesce_cpus();
#endif /* MULTIPROCESSOR */

	printf("Unhibernating...");

	/* Read the image from disk into the image (pig) area */
	if (hibernate_read_image(&disk_hiber_info))
		goto fail;

	if (config_suspend(TAILQ_FIRST(&alldevs), DVACT_QUIESCE) != 0)
		goto fail;

	(void) splhigh();
	hibernate_disable_intr_machdep();
	cold = 1;

	if (config_suspend(TAILQ_FIRST(&alldevs), DVACT_SUSPEND) != 0) {
		cold = 0;
		hibernate_enable_intr_machdep();
		goto fail;
	}

	pmap_kenter_pa(HIBERNATE_HIBALLOC_PAGE, HIBERNATE_HIBALLOC_PAGE,
	    VM_PROT_ALL);
	pmap_activate(curproc);

	/* Switch stacks */
	hibernate_switch_stack_machdep();

	/* Unpack and resume */
	hibernate_unpack_image(&disk_hiber_info);

fail:
	splx(s);
	printf("\nUnable to resume hibernated image\n");
}

/*
 * Unpack image from pig area to original location by looping through the
 * list of output chunks in the order they should be restored (fchunks).
 *
 * Note that due to the stack smash protector and the fact that we have
 * switched stacks, it is not permitted to return from this function.
 */
void
hibernate_unpack_image(union hibernate_info *hiber_info)
{
	struct hibernate_disk_chunk *chunks;
	union hibernate_info local_hiber_info;
	paddr_t image_cur = global_pig_start;
	short i, *fchunks;
	char *pva = (char *)hiber_info->piglet_va;
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	/* Mask off based on arch-specific piglet page size */
	pva = (char *)((paddr_t)pva & (PIGLET_PAGE_MASK));
	fchunks = (short *)(pva + (4 * PAGE_SIZE));

	chunks = (struct hibernate_disk_chunk *)(pva +  HIBERNATE_CHUNK_SIZE);

	/* Can't use hiber_info that's passed in after this point */
	bcopy(hiber_info, &local_hiber_info, sizeof(union hibernate_info));

	/*
	 * Point of no return. Once we pass this point, only kernel code can
	 * be accessed. No global variables or other kernel data structures
	 * are guaranteed to be coherent after unpack starts.
	 *
	 * The image is now in high memory (pig area), we unpack from the pig
	 * to the correct location in memory. We'll eventually end up copying
	 * on top of ourself, but we are assured the kernel code here is the
	 * same between the hibernated and resuming kernel, and we are running
	 * on our own stack, so the overwrite is ok.
	 */
	hibernate_activate_resume_pt_machdep();

	for (i = 0; i < local_hiber_info.chunk_ctr; i++) {
		/* Reset zlib for inflate */
		if (hibernate_zlib_reset(&local_hiber_info, 0) != Z_OK)
			panic("hibernate failed to reset zlib for inflate");

		hibernate_process_chunk(&local_hiber_info, &chunks[fchunks[i]],
		    image_cur);

		image_cur += chunks[fchunks[i]].compressed_size;

	}

	/*
	 * Resume the loaded kernel by jumping to the MD resume vector.
	 * We won't be returning from this call.
	 */
	hibernate_resume_machdep();
}

/*
 * Bounce a compressed image chunk to the piglet, entering mappings for the
 * copied pages as needed
 */
void
hibernate_copy_chunk_to_piglet(paddr_t img_cur, vaddr_t piglet, size_t size)
{
	size_t ct, ofs;
	paddr_t src = img_cur;
	vaddr_t dest = piglet;

	/* Copy first partial page */
	ct = (PAGE_SIZE) - (src & PAGE_MASK);
	ofs = (src & PAGE_MASK);

	if (ct < PAGE_SIZE) {
		hibernate_enter_resume_mapping(HIBERNATE_INFLATE_PAGE,
			(src - ofs), 0);
		hibernate_flush();
		bcopy((caddr_t)(HIBERNATE_INFLATE_PAGE + ofs), (caddr_t)dest, ct);
		src += ct;
		dest += ct;
	}

	/* Copy remaining pages */	
	while (src < size + img_cur) {
		hibernate_enter_resume_mapping(HIBERNATE_INFLATE_PAGE, src, 0);
		hibernate_flush();
		ct = PAGE_SIZE;
		bcopy((caddr_t)(HIBERNATE_INFLATE_PAGE), (caddr_t)dest, ct);
		hibernate_flush();
		src += ct;
		dest += ct;
	}
}

/*
 * Process a chunk by bouncing it to the piglet, followed by unpacking 
 */
void
hibernate_process_chunk(union hibernate_info *hiber_info,
    struct hibernate_disk_chunk *chunk, paddr_t img_cur)
{
	char *pva = (char *)hiber_info->piglet_va;

	hibernate_copy_chunk_to_piglet(img_cur,
	 (vaddr_t)(pva + (HIBERNATE_CHUNK_SIZE * 2)), chunk->compressed_size);

	hibernate_inflate_region(hiber_info, chunk->base,
	    (vaddr_t)(pva + (HIBERNATE_CHUNK_SIZE * 2)),
	    chunk->compressed_size);
}

/*
 * Write a compressed version of this machine's memory to disk, at the
 * precalculated swap offset:
 *
 * end of swap - signature block size - chunk table size - memory size
 *
 * The function begins by looping through each phys mem range, cutting each
 * one into MD sized chunks. These chunks are then compressed individually
 * and written out to disk, in phys mem order. Some chunks might compress
 * more than others, and for this reason, each chunk's size is recorded
 * in the chunk table, which is written to disk after the image has
 * properly been compressed and written (in hibernate_write_chunktable).
 *
 * When this function is called, the machine is nearly suspended - most
 * devices are quiesced/suspended, interrupts are off, and cold has
 * been set. This means that there can be no side effects once the
 * write has started, and the write function itself can also have no
 * side effects. This also means no printfs are permitted (since printf
 * has side effects.)
 *
 * Return values :
 *
 * 0      - success
 * EIO    - I/O error occurred writing the chunks
 * EINVAL - Failed to write a complete range
 * ENOMEM - Memory allocation failure during preparation of the zlib arena
 */
int
hibernate_write_chunks(union hibernate_info *hiber_info)
{
	paddr_t range_base, range_end, inaddr, temp_inaddr;
	size_t nblocks, out_remaining, used;
	struct hibernate_disk_chunk *chunks;
	vaddr_t hibernate_io_page = hiber_info->piglet_va + PAGE_SIZE;
	daddr_t blkctr = hiber_info->image_offset, offset = 0;
	int i;
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	hiber_info->chunk_ctr = 0;

	/*
	 * Allocate VA for the temp and copy page.
	 * 
	 * These will become part of the suspended kernel and will
	 * be freed in hibernate_free, upon resume.
	 */
	hibernate_temp_page = (vaddr_t)km_alloc(PAGE_SIZE, &kv_any,
	    &kp_none, &kd_nowait);
	if (!hibernate_temp_page)
		return (ENOMEM);

	hibernate_copy_page = (vaddr_t)km_alloc(PAGE_SIZE, &kv_any,
	    &kp_none, &kd_nowait);
	if (!hibernate_copy_page)
		return (ENOMEM);

	pmap_kenter_pa(hibernate_copy_page,
	    (hiber_info->piglet_pa + 3*PAGE_SIZE), VM_PROT_ALL);

	pmap_activate(curproc);

	chunks = (struct hibernate_disk_chunk *)(hiber_info->piglet_va +
	    HIBERNATE_CHUNK_SIZE);

	/* Calculate the chunk regions */
	for (i = 0; i < hiber_info->nranges; i++) {
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
	for (i = 0; i < hiber_info->chunk_ctr; i++) {
		range_base = chunks[i].base;
		range_end = chunks[i].end;

		chunks[i].offset = blkctr;

		/* Reset zlib for deflate */
		if (hibernate_zlib_reset(hiber_info, 1) != Z_OK)
			return (ENOMEM);

		inaddr = range_base;

		/*
		 * For each range, loop through its phys mem region
		 * and write out the chunks (the last chunk might be
		 * smaller than the chunk size).
		 */
		while (inaddr < range_end) {
			out_remaining = PAGE_SIZE;
			while (out_remaining > 0 && inaddr < range_end) {

				/*
				 * Adjust for regions that are not evenly
				 * divisible by PAGE_SIZE or overflowed
				 * pages from the previous iteration.
				 */
				temp_inaddr = (inaddr & PAGE_MASK) +
				    hibernate_copy_page;
				
				/* Deflate from temp_inaddr to IO page */
				if (inaddr != range_end) {
					pmap_kenter_pa(hibernate_temp_page,
					    inaddr & PMAP_PA_MASK, VM_PROT_ALL);

					pmap_activate(curproc);

					bcopy((caddr_t)hibernate_temp_page,
					    (caddr_t)hibernate_copy_page,
					    PAGE_SIZE);
					inaddr += hibernate_deflate(hiber_info,
					    temp_inaddr, &out_remaining);
				}

				if (out_remaining == 0) {
					/* Filled up the page */
					nblocks =
					    PAGE_SIZE / hiber_info->secsize;

					if (hiber_info->io_func(
					    hiber_info->device,
					    blkctr, (vaddr_t)hibernate_io_page,
					    PAGE_SIZE, HIB_W,
					    hiber_info->io_page))
						return (EIO);

					blkctr += nblocks;
				}
			}
		}

		if (inaddr != range_end)
			return (EINVAL);

		/*
		 * End of range. Round up to next secsize bytes
		 * after finishing compress
		 */
		if (out_remaining == 0)
			out_remaining = PAGE_SIZE;

		/* Finish compress */
		hibernate_state->hib_stream.next_in = (caddr_t)inaddr;
		hibernate_state->hib_stream.avail_in = 0;
		hibernate_state->hib_stream.next_out =
		    (caddr_t)hibernate_io_page + (PAGE_SIZE - out_remaining);
		hibernate_state->hib_stream.avail_out = out_remaining;

		if (deflate(&hibernate_state->hib_stream, Z_FINISH) !=
		    Z_STREAM_END)
			return (EIO);

		out_remaining = hibernate_state->hib_stream.avail_out;

		used = PAGE_SIZE - out_remaining;
		nblocks = used / hiber_info->secsize;

		/* Round up to next block if needed */
		if (used % hiber_info->secsize != 0)
			nblocks ++;

		/* Write final block(s) for this chunk */
		if (hiber_info->io_func(hiber_info->device, blkctr,
		    (vaddr_t)hibernate_io_page, nblocks*hiber_info->secsize,
		    HIB_W, hiber_info->io_page))
			return (EIO);

		blkctr += nblocks;

		offset = blkctr;
		chunks[i].compressed_size = (offset - chunks[i].offset) *
		    hiber_info->secsize;
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
	char *pva = (char *)hiber_info->piglet_va;
	struct hibernate_zlib_state *hibernate_state;

	hibernate_state =
	    (struct hibernate_zlib_state *)HIBERNATE_HIBALLOC_PAGE;

	if(!deflate)
		pva = (char *)((paddr_t)pva & (PIGLET_PAGE_MASK));

	hibernate_zlib_start = (vaddr_t)(pva + (28 * PAGE_SIZE));
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
		    Z_BEST_SPEED);
	} else
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
	size_t compressed_size, disk_size, chunktable_size, pig_sz;
	paddr_t image_start, image_end, pig_start, pig_end;
	struct hibernate_disk_chunk *chunks;
	daddr_t blkctr;
	vaddr_t chunktable = (vaddr_t)NULL;
	paddr_t piglet_chunktable = hiber_info->piglet_pa +
	    HIBERNATE_CHUNK_SIZE;
	int i;

	pmap_activate(curproc);

	/* Calculate total chunk table size in disk blocks */
	chunktable_size = HIBERNATE_CHUNK_TABLE_SIZE / hiber_info->secsize;

	blkctr = hiber_info->sig_offset - chunktable_size -
			hiber_info->swap_offset;

	chunktable = (vaddr_t)km_alloc(HIBERNATE_CHUNK_TABLE_SIZE, &kv_any,
	    &kp_none, &kd_nowait);

	if (!chunktable)
		return (1);

	/* Read the chunktable from disk into the piglet chunktable */
	for (i = 0; i < HIBERNATE_CHUNK_TABLE_SIZE;
	    i += PAGE_SIZE, blkctr += PAGE_SIZE/hiber_info->secsize) {
		pmap_kenter_pa(chunktable + i, piglet_chunktable + i,
		    VM_PROT_ALL);
		pmap_update(pmap_kernel());
		hibernate_block_io(hiber_info, blkctr, PAGE_SIZE,
		    chunktable + i, 0);
	}

	blkctr = hiber_info->image_offset;
	compressed_size = 0;

	chunks = (struct hibernate_disk_chunk *)chunktable;

	for (i = 0; i < hiber_info->chunk_ctr; i++)
		compressed_size += chunks[i].compressed_size;

	disk_size = compressed_size;

	printf(" (image size: %zu)\n", compressed_size);

	/* Allocate the pig area */
	pig_sz = compressed_size + HIBERNATE_CHUNK_SIZE;
	if (uvm_pmr_alloc_pig(&pig_start, pig_sz) == ENOMEM)
		return (1);

	pig_end = pig_start + pig_sz;

	/* Calculate image extents. Pig image must end on a chunk boundary. */
	image_end = pig_end & ~(HIBERNATE_CHUNK_SIZE - 1);
	image_start = pig_start;

	image_start = image_end - disk_size;

	hibernate_read_chunks(hiber_info, image_start, image_end, disk_size,
	    chunks);

	pmap_kremove(chunktable, PAGE_SIZE);
	pmap_update(pmap_kernel());

	/* Prepare the resume time pmap/page table */
	hibernate_populate_resume_pt(hiber_info, image_start, image_end);

	return (0);
}

/*
 * Read the hibernated memory chunks from disk (chunk information at this
 * point is stored in the piglet) into the pig area specified by
 * [pig_start .. pig_end]. Order the chunks so that the final chunk is the
 * only chunk with overlap possibilities.
 */
int
hibernate_read_chunks(union hibernate_info *hib_info, paddr_t pig_start,
    paddr_t pig_end, size_t image_compr_size,
    struct hibernate_disk_chunk *chunks)
{
	paddr_t img_index, img_cur, r1s, r1e, r2s, r2e;
	paddr_t copy_start, copy_end, piglet_cur;
	paddr_t piglet_base = hib_info->piglet_pa;
	paddr_t piglet_end = piglet_base + HIBERNATE_CHUNK_SIZE;
	daddr_t blkctr;
	size_t processed, compressed_size, read_size;
	int overlap, found, nchunks, nochunks = 0, nfchunks = 0, npchunks = 0;
	short *ochunks, *pchunks, *fchunks, i, j;
	vaddr_t tempva = (vaddr_t)NULL, hibernate_fchunk_area = (vaddr_t)NULL;

	global_pig_start = pig_start;

	pmap_activate(curproc);

	/*
	 * These mappings go into the resuming kernel's page table, and are
	 * used only during image read. They dissappear from existence
	 * when the suspended kernel is unpacked on top of us.
	 */
	tempva = (vaddr_t)km_alloc(2*PAGE_SIZE, &kv_any, &kp_none, &kd_nowait);
	if (!tempva)
		return (1);
	hibernate_fchunk_area = (vaddr_t)km_alloc(24*PAGE_SIZE, &kv_any,
	    &kp_none, &kd_nowait);
	if (!hibernate_fchunk_area)
		return (1);

	/* Final output chunk ordering VA */
	fchunks = (short *)hibernate_fchunk_area;

	/* Piglet chunk ordering VA */
	pchunks = (short *)(hibernate_fchunk_area + (8*PAGE_SIZE));

	/* Final chunk ordering VA */
	ochunks = (short *)(hibernate_fchunk_area + (16*PAGE_SIZE));

	/* Map the chunk ordering region */
	for(i=0; i<24 ; i++) {
		pmap_kenter_pa(hibernate_fchunk_area + (i*PAGE_SIZE),
			piglet_base + ((4+i)*PAGE_SIZE), VM_PROT_ALL);
		pmap_update(pmap_kernel());
	}

	nchunks = hib_info->chunk_ctr;

	/* Initially start all chunks as unplaced */
	for (i = 0; i < nchunks; i++)
		chunks[i].flags = 0;

	/*
	 * Search the list for chunks that are outside the pig area. These
	 * can be placed first in the final output list.
	 */
	for (i = 0; i < nchunks; i++) {
		if (chunks[i].end <= pig_start || chunks[i].base >= pig_end) {
			ochunks[nochunks] = i;
			fchunks[nfchunks] = i;
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
		found = 0;
		j = -1;
		for (i = 0; i < nchunks; i++)
			if (chunks[i].base < img_index &&
			    chunks[i].flags == 0 ) {
				j = i;
				img_index = chunks[i].base;
			}

		if (j != -1) {
			found = 1;
			ochunks[nochunks] = j;
			nochunks++;
			chunks[j].flags |= HIBERNATE_CHUNK_PLACED;
		}
	} while (found);

	img_index = pig_start;

	/*
	 * Identify chunk output conflicts (chunks whose pig load area
	 * corresponds to their original memory placement location)
	 */
	for (i = 0; i < nochunks ; i++) {
		overlap = 0;
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
	img_index = pig_start;
	for (i = 0; i < nochunks ; i++) {
		/*
		 * If a conflict is detected, consume enough compressed
		 * output chunks to fill the piglet
		 */
		if (chunks[ochunks[i]].flags & HIBERNATE_CHUNK_CONFLICT) {
			copy_start = piglet_base;
			copy_end = piglet_end;
			piglet_cur = piglet_base;
			npchunks = 0;
			j = i;

			while (copy_start < copy_end && j < nochunks) {
				piglet_cur +=
				    chunks[ochunks[j]].compressed_size;
				pchunks[npchunks] = ochunks[j];
				npchunks++;
				copy_start +=
				    chunks[ochunks[j]].compressed_size;
				img_index += chunks[ochunks[j]].compressed_size;
				i++;
				j++;
			}

			piglet_cur = piglet_base;
			for (j = 0; j < npchunks; j++) {
				piglet_cur +=
				    chunks[pchunks[j]].compressed_size;
				fchunks[nfchunks] = pchunks[j];
				chunks[pchunks[j]].flags |=
				    HIBERNATE_CHUNK_USED;
				nfchunks++;
			}
		} else {
			/*
			 * No conflict, chunk can be added without copying
			 */
			if ((chunks[ochunks[i]].flags &
			    HIBERNATE_CHUNK_USED) == 0) {
				fchunks[nfchunks] = ochunks[i];
				chunks[ochunks[i]].flags |=
				    HIBERNATE_CHUNK_USED;
				nfchunks++;
			}
			img_index += chunks[ochunks[i]].compressed_size;
		}
	}

	img_index = pig_start;
	for (i = 0; i < nfchunks; i++) {
		piglet_cur = piglet_base;
		img_index += chunks[fchunks[i]].compressed_size;
	}

	img_cur = pig_start;

	for (i = 0; i < nfchunks; i++) {
		blkctr = chunks[fchunks[i]].offset - hib_info->swap_offset;
		processed = 0;
		compressed_size = chunks[fchunks[i]].compressed_size;

		while (processed < compressed_size) {
			pmap_kenter_pa(tempva, img_cur, VM_PROT_ALL);
			pmap_kenter_pa(tempva + PAGE_SIZE, img_cur+PAGE_SIZE,
			    VM_PROT_ALL);
			pmap_update(pmap_kernel());

			if (compressed_size - processed >= PAGE_SIZE)
				read_size = PAGE_SIZE;
			else
				read_size = compressed_size - processed;

			hibernate_block_io(hib_info, blkctr, read_size,
			    tempva + (img_cur & PAGE_MASK), 0);

			blkctr += (read_size / hib_info->secsize);

			hibernate_flush();
			pmap_kremove(tempva, PAGE_SIZE);
			pmap_kremove(tempva + PAGE_SIZE, PAGE_SIZE);
			processed += read_size;
			img_cur += read_size;
		}
	}

	pmap_kremove(hibernate_fchunk_area, PAGE_SIZE);
	pmap_kremove((vaddr_t)pchunks, PAGE_SIZE);
	pmap_kremove((vaddr_t)fchunks, PAGE_SIZE);
	pmap_update(pmap_kernel());	

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
hibernate_suspend(void)
{
	union hibernate_info hib_info;
	size_t swap_size;

	/*
	 * Calculate memory ranges, swap offsets, etc.
	 * This also allocates a piglet whose physaddr is stored in
	 * hib_info->piglet_pa and vaddr stored in hib_info->piglet_va
	 */
	if (get_hibernate_info(&hib_info, 1))
		return (1);

	swap_size = hib_info.image_size + hib_info.secsize +
		HIBERNATE_CHUNK_TABLE_SIZE;

	if (uvm_swap_check_range(hib_info.device, swap_size)) {
		printf("insufficient swap space for hibernate\n");
		return (1);
	}

	pmap_kenter_pa(HIBERNATE_HIBALLOC_PAGE, HIBERNATE_HIBALLOC_PAGE,
		VM_PROT_ALL);
	pmap_activate(curproc);

	/* Stash the piglet VA so we can free it in the resuming kernel */
	global_piglet_va = hib_info.piglet_va;

	if (hibernate_write_chunks(&hib_info))
		return (1);

	if (hibernate_write_chunktable(&hib_info))
		return (1);

	if (hibernate_write_signature(&hib_info))
		return (1);

	/* Allow the disk to settle */
	delay(500000);

	/*
	 * Give the device-specific I/O function a notification that we're
	 * done, and that it can clean up or shutdown as needed.
	 */
	hib_info.io_func(hib_info.device, 0, (vaddr_t)NULL, 0,
	    HIB_DONE, hib_info.io_page);

	return (0);
}

/*
 * Free items allocated by hibernate_suspend()
 */
void
hibernate_free(void)
{
	if (global_piglet_va)
		uvm_pmr_free_piglet(global_piglet_va,
		    3*HIBERNATE_CHUNK_SIZE);

	if (hibernate_copy_page)
		pmap_kremove(hibernate_copy_page, PAGE_SIZE);
	if (hibernate_temp_page)
		pmap_kremove(hibernate_temp_page, PAGE_SIZE);

	pmap_update(pmap_kernel());

	if (hibernate_copy_page)
		km_free((void *)hibernate_copy_page, PAGE_SIZE,
		    &kv_any, &kp_none);
	if (hibernate_temp_page)
		km_free((void *)hibernate_temp_page, PAGE_SIZE,
		    &kv_any, &kp_none);

	global_piglet_va = 0;
	hibernate_copy_page = 0;
	hibernate_temp_page = 0;
}
