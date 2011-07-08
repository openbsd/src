/*	$OpenBSD: subr_hibernate.c,v 1.1 2011/07/08 17:58:16 ariane Exp $	*/

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
