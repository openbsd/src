/*	$OpenBSD: uvm_pmemrange.c,v 1.27 2011/07/06 19:50:38 beck Exp $	*/

/*
 * Copyright (c) 2009, 2010 Ariane van der Steldt <ariane@stack.nl>
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
#include <uvm/uvm.h>
#include <sys/malloc.h>
#include <sys/proc.h>		/* XXX for atomic */
#include <sys/kernel.h>

/*
 * 2 trees: addr tree and size tree.
 *
 * The allocator keeps chunks of free pages (called a range).
 * Two pages are part of the same range if:
 * - all pages in between are part of that range,
 * - they are of the same memory type (zeroed or non-zeroed),
 * - they are part of the same pmemrange.
 * A pmemrange is a range of memory which is part of the same vm_physseg
 * and has a use-count.
 *
 * addr tree is vm_page[0].objt
 * size tree is vm_page[1].objt
 *
 * The size tree is not used for memory ranges of 1 page, instead,
 * single queue is vm_page[0].pageq
 *
 * vm_page[0].fpgsz describes the length of a free range. Two adjecent ranges
 * are joined, unless:
 * - they have pages in between them which are not free
 * - they belong to different memtypes (zeroed vs dirty memory)
 * - they are in different pmemrange areas (ISA vs non-ISA memory for instance)
 * - they are not a continuation of the same array
 * The latter issue is caused by vm_physseg ordering and splitting from the
 * MD initialization machinery. The MD code is dependant on freelists and
 * happens to split ISA memory from non-ISA memory.
 * (Note: freelists die die die!)
 *
 * uvm_page_init guarantees that every vm_physseg contains an array of
 * struct vm_page. Also, uvm_page_physload allocates an array of struct
 * vm_page. This code depends on that array. The array may break across
 * vm_physsegs boundaries.
 */

/*
 * Validate the flags of the page. (Used in asserts.)
 * Any free page must have the PQ_FREE flag set.
 * Free pages may be zeroed.
 * Pmap flags are left untouched.
 *
 * The PQ_FREE flag is not checked here: by not checking, we can easily use
 * this check in pages which are freed.
 */
#define VALID_FLAGS(pg_flags)						\
	(((pg_flags) & ~(PQ_FREE|PG_ZERO|				\
	    PG_PMAP0|PG_PMAP1|PG_PMAP2|PG_PMAP3)) == 0x0)

/* Tree comparators. */
int	uvm_pmemrange_addr_cmp(struct uvm_pmemrange *, struct uvm_pmemrange *);
int	uvm_pmemrange_use_cmp(struct uvm_pmemrange *, struct uvm_pmemrange *);
int	uvm_pmr_addr_cmp(struct vm_page *, struct vm_page *);
int	uvm_pmr_size_cmp(struct vm_page *, struct vm_page *);
int	uvm_pmr_pg_to_memtype(struct vm_page *);

#ifdef DDB
void	uvm_pmr_print(void);
#endif

/*
 * Memory types. The page flags are used to derive what the current memory
 * type of a page is.
 */
int
uvm_pmr_pg_to_memtype(struct vm_page *pg)
{
	if (pg->pg_flags & PG_ZERO)
		return UVM_PMR_MEMTYPE_ZERO;
	/* Default: dirty memory. */
	return UVM_PMR_MEMTYPE_DIRTY;
}

/* Trees. */
RB_PROTOTYPE(uvm_pmr_addr, vm_page, objt, uvm_pmr_addr_cmp);
RB_PROTOTYPE(uvm_pmr_size, vm_page, objt, uvm_pmr_size_cmp);
RB_PROTOTYPE(uvm_pmemrange_addr, uvm_pmemrange, pmr_addr,
    uvm_pmemrange_addr_cmp);
RB_GENERATE(uvm_pmr_addr, vm_page, objt, uvm_pmr_addr_cmp);
RB_GENERATE(uvm_pmr_size, vm_page, objt, uvm_pmr_size_cmp);
RB_GENERATE(uvm_pmemrange_addr, uvm_pmemrange, pmr_addr,
    uvm_pmemrange_addr_cmp);

/* Validation. */
#ifdef DEBUG
void	uvm_pmr_assertvalid(struct uvm_pmemrange *pmr);
#else
#define uvm_pmr_assertvalid(pmr)	do {} while (0)
#endif

int			 uvm_pmr_get1page(psize_t, int, struct pglist *,
			    paddr_t, paddr_t);

struct uvm_pmemrange	*uvm_pmr_allocpmr(void);
struct vm_page		*uvm_pmr_nfindsz(struct uvm_pmemrange *, psize_t, int);
struct vm_page		*uvm_pmr_nextsz(struct uvm_pmemrange *,
			    struct vm_page *, int);
void			 uvm_pmr_pnaddr(struct uvm_pmemrange *pmr,
			    struct vm_page *pg, struct vm_page **pg_prev,
			    struct vm_page **pg_next);
struct vm_page		*uvm_pmr_insert_addr(struct uvm_pmemrange *,
			    struct vm_page *, int);
void			 uvm_pmr_insert_size(struct uvm_pmemrange *,
			    struct vm_page *);
struct vm_page		*uvm_pmr_insert(struct uvm_pmemrange *,
			    struct vm_page *, int);
void			 uvm_pmr_remove_size(struct uvm_pmemrange *,
			    struct vm_page *);
void			 uvm_pmr_remove_addr(struct uvm_pmemrange *,
			    struct vm_page *);
void			 uvm_pmr_remove(struct uvm_pmemrange *,
			    struct vm_page *);
struct vm_page		*uvm_pmr_findnextsegment(struct uvm_pmemrange *,
			    struct vm_page *, paddr_t);
psize_t			 uvm_pmr_remove_1strange(struct pglist *, paddr_t,
			    struct vm_page **, int);
void			 uvm_pmr_split(paddr_t);
struct uvm_pmemrange	*uvm_pmemrange_find(paddr_t);
struct uvm_pmemrange	*uvm_pmemrange_use_insert(struct uvm_pmemrange_use *,
			    struct uvm_pmemrange *);
struct vm_page		*uvm_pmr_extract_range(struct uvm_pmemrange *,
			    struct vm_page *, paddr_t, paddr_t,
			    struct pglist *);
psize_t			 pow2divide(psize_t, psize_t);
struct vm_page		*uvm_pmr_rootupdate(struct uvm_pmemrange *,
			    struct vm_page *, paddr_t, paddr_t, int);

/*
 * Computes num/denom and rounds it up to the next power-of-2.
 *
 * This is a division function which calculates an approximation of
 * num/denom, with result =~ num/denom. It is meant to be fast and doesn't
 * have to be accurate.
 *
 * Providing too large a value makes the allocator slightly faster, at the
 * risk of hitting the failure case more often. Providing too small a value
 * makes the allocator a bit slower, but less likely to hit a failure case.
 */
psize_t
pow2divide(psize_t num, psize_t denom)
{
	int rshift;

	for (rshift = 0; num > denom; rshift++, denom <<= 1)
		;
	return (paddr_t)1 << rshift;
}

/*
 * Predicate: lhs is a subrange or rhs.
 *
 * If rhs_low == 0: don't care about lower bound.
 * If rhs_high == 0: don't care about upper bound.
 */
#define PMR_IS_SUBRANGE_OF(lhs_low, lhs_high, rhs_low, rhs_high)	\
	(((rhs_low) == 0 || (lhs_low) >= (rhs_low)) &&			\
	((rhs_high) == 0 || (lhs_high) <= (rhs_high)))

/*
 * Predicate: lhs intersects with rhs.
 *
 * If rhs_low == 0: don't care about lower bound.
 * If rhs_high == 0: don't care about upper bound.
 * Ranges don't intersect if they don't have any page in common, array
 * semantics mean that < instead of <= should be used here.
 */
#define PMR_INTERSECTS_WITH(lhs_low, lhs_high, rhs_low, rhs_high)	\
	(((rhs_low) == 0 || (rhs_low) < (lhs_high)) &&			\
	((rhs_high) == 0 || (lhs_low) < (rhs_high)))

/*
 * Align to power-of-2 alignment.
 */
#define PMR_ALIGN(pgno, align)						\
	(((pgno) + ((align) - 1)) & ~((align) - 1))


/*
 * Comparator: sort by address ascending.
 */
int
uvm_pmemrange_addr_cmp(struct uvm_pmemrange *lhs, struct uvm_pmemrange *rhs)
{
	return lhs->low < rhs->low ? -1 : lhs->low > rhs->low;
}

/*
 * Comparator: sort by use ascending.
 *
 * The higher the use value of a range, the more devices need memory in
 * this range. Therefor allocate from the range with the lowest use first.
 */
int
uvm_pmemrange_use_cmp(struct uvm_pmemrange *lhs, struct uvm_pmemrange *rhs)
{
	int result;

	result = lhs->use < rhs->use ? -1 : lhs->use > rhs->use;
	if (result == 0)
		result = uvm_pmemrange_addr_cmp(lhs, rhs);
	return result;
}

int
uvm_pmr_addr_cmp(struct vm_page *lhs, struct vm_page *rhs)
{
	paddr_t lhs_addr, rhs_addr;

	lhs_addr = VM_PAGE_TO_PHYS(lhs);
	rhs_addr = VM_PAGE_TO_PHYS(rhs);

	return (lhs_addr < rhs_addr ? -1 : lhs_addr > rhs_addr);
}

int
uvm_pmr_size_cmp(struct vm_page *lhs, struct vm_page *rhs)
{
	psize_t lhs_size, rhs_size;
	int cmp;

	/* Using second tree, so we receive pg[1] instead of pg[0]. */
	lhs_size = (lhs - 1)->fpgsz;
	rhs_size = (rhs - 1)->fpgsz;

	cmp = (lhs_size < rhs_size ? -1 : lhs_size > rhs_size);
	if (cmp == 0)
		cmp = uvm_pmr_addr_cmp(lhs - 1, rhs - 1);
	return cmp;
}

/*
 * Find the first range of free pages that is at least sz pages long.
 */
struct vm_page *
uvm_pmr_nfindsz(struct uvm_pmemrange *pmr, psize_t sz, int mti)
{
	struct	vm_page *node, *best;

	KASSERT(sz >= 1);

	if (sz == 1 && !TAILQ_EMPTY(&pmr->single[mti]))
		return TAILQ_FIRST(&pmr->single[mti]);

	node = RB_ROOT(&pmr->size[mti]);
	best = NULL;
	while (node != NULL) {
		if ((node - 1)->fpgsz >= sz) {
			best = (node - 1);
			node = RB_LEFT(node, objt);
		} else
			node = RB_RIGHT(node, objt);
	}
	return best;
}

/*
 * Finds the next range. The next range has a size >= pg->fpgsz.
 * Returns NULL if no more ranges are available.
 */
struct vm_page *
uvm_pmr_nextsz(struct uvm_pmemrange *pmr, struct vm_page *pg, int mt)
{
	struct vm_page *npg;

	KASSERT(pmr != NULL && pg != NULL);
	if (pg->fpgsz == 1) {
		if (TAILQ_NEXT(pg, pageq) != NULL)
			return TAILQ_NEXT(pg, pageq);
		else
			npg = RB_MIN(uvm_pmr_size, &pmr->size[mt]);
	} else
		npg = RB_NEXT(uvm_pmr_size, &pmr->size[mt], pg + 1);

	return npg == NULL ? NULL : npg - 1;
}

/*
 * Finds the previous and next ranges relative to the (uninserted) pg range.
 *
 * *pg_prev == NULL if no previous range is available, that can join with
 * 	pg.
 * *pg_next == NULL if no next range is available, that can join with
 * 	pg.
 */
void
uvm_pmr_pnaddr(struct uvm_pmemrange *pmr, struct vm_page *pg,
    struct vm_page **pg_prev, struct vm_page **pg_next)
{
	KASSERT(pg_prev != NULL && pg_next != NULL);

	*pg_next = RB_NFIND(uvm_pmr_addr, &pmr->addr, pg);
	if (*pg_next == NULL)
		*pg_prev = RB_MAX(uvm_pmr_addr, &pmr->addr);
	else
		*pg_prev = RB_PREV(uvm_pmr_addr, &pmr->addr, *pg_next);

	KDASSERT(*pg_next == NULL ||
	    VM_PAGE_TO_PHYS(*pg_next) > VM_PAGE_TO_PHYS(pg));
	KDASSERT(*pg_prev == NULL ||
	    VM_PAGE_TO_PHYS(*pg_prev) < VM_PAGE_TO_PHYS(pg));

	/* Reset if not contig. */
	if (*pg_prev != NULL &&
	    (atop(VM_PAGE_TO_PHYS(*pg_prev)) + (*pg_prev)->fpgsz
	    != atop(VM_PAGE_TO_PHYS(pg)) ||
	    *pg_prev + (*pg_prev)->fpgsz != pg || /* Array broke. */
	    uvm_pmr_pg_to_memtype(*pg_prev) != uvm_pmr_pg_to_memtype(pg)))
		*pg_prev = NULL;
	if (*pg_next != NULL &&
	    (atop(VM_PAGE_TO_PHYS(pg)) + pg->fpgsz
	    != atop(VM_PAGE_TO_PHYS(*pg_next)) ||
	    pg + pg->fpgsz != *pg_next || /* Array broke. */
	    uvm_pmr_pg_to_memtype(*pg_next) != uvm_pmr_pg_to_memtype(pg)))
		*pg_next = NULL;
	return;
}

/*
 * Remove a range from the address tree.
 * Address tree maintains pmr counters.
 */
void
uvm_pmr_remove_addr(struct uvm_pmemrange *pmr, struct vm_page *pg)
{
	KDASSERT(RB_FIND(uvm_pmr_addr, &pmr->addr, pg) == pg);
	KDASSERT(pg->pg_flags & PQ_FREE);
	RB_REMOVE(uvm_pmr_addr, &pmr->addr, pg);

	pmr->nsegs--;
}
/*
 * Remove a range from the size tree.
 */
void
uvm_pmr_remove_size(struct uvm_pmemrange *pmr, struct vm_page *pg)
{
	int memtype;
#ifdef DEBUG
	struct vm_page *i;
#endif

	KDASSERT(pg->fpgsz >= 1);
	KDASSERT(pg->pg_flags & PQ_FREE);
	memtype = uvm_pmr_pg_to_memtype(pg);

	if (pg->fpgsz == 1) {
#ifdef DEBUG
		TAILQ_FOREACH(i, &pmr->single[memtype], pageq) {
			if (i == pg)
				break;
		}
		KDASSERT(i == pg);
#endif
		TAILQ_REMOVE(&pmr->single[memtype], pg, pageq);
	} else {
		KDASSERT(RB_FIND(uvm_pmr_size, &pmr->size[memtype],
		    pg + 1) == pg + 1);
		RB_REMOVE(uvm_pmr_size, &pmr->size[memtype], pg + 1);
	}
}
/* Remove from both trees. */
void
uvm_pmr_remove(struct uvm_pmemrange *pmr, struct vm_page *pg)
{
	uvm_pmr_assertvalid(pmr);
	uvm_pmr_remove_size(pmr, pg);
	uvm_pmr_remove_addr(pmr, pg);
	uvm_pmr_assertvalid(pmr);
}

/*
 * Insert the range described in pg.
 * Returns the range thus created (which may be joined with the previous and
 * next ranges).
 * If no_join, the caller guarantees that the range cannot possibly join
 * with adjecent ranges.
 */
struct vm_page *
uvm_pmr_insert_addr(struct uvm_pmemrange *pmr, struct vm_page *pg, int no_join)
{
	struct vm_page *prev, *next;

#ifdef DEBUG
	struct vm_page *i;
	int mt;
#endif

	KDASSERT(pg->pg_flags & PQ_FREE);
	KDASSERT(pg->fpgsz >= 1);

#ifdef DEBUG
	for (mt = 0; mt < UVM_PMR_MEMTYPE_MAX; mt++) {
		TAILQ_FOREACH(i, &pmr->single[mt], pageq)
			KDASSERT(i != pg);
		if (pg->fpgsz > 1) {
			KDASSERT(RB_FIND(uvm_pmr_size, &pmr->size[mt],
			    pg + 1) == NULL);
		}
		KDASSERT(RB_FIND(uvm_pmr_addr, &pmr->addr, pg) == NULL);
	}
#endif

	if (!no_join) {
		uvm_pmr_pnaddr(pmr, pg, &prev, &next);
		if (next != NULL) {
			uvm_pmr_remove_size(pmr, next);
			uvm_pmr_remove_addr(pmr, next);
			pg->fpgsz += next->fpgsz;
			next->fpgsz = 0;
		}
		if (prev != NULL) {
			uvm_pmr_remove_size(pmr, prev);
			prev->fpgsz += pg->fpgsz;
			pg->fpgsz = 0;
			return prev;
		}
	}

	RB_INSERT(uvm_pmr_addr, &pmr->addr, pg);

	pmr->nsegs++;

	return pg;
}
/*
 * Insert the range described in pg.
 * Returns the range thus created (which may be joined with the previous and
 * next ranges).
 * Page must already be in the address tree.
 */
void
uvm_pmr_insert_size(struct uvm_pmemrange *pmr, struct vm_page *pg)
{
	int memtype;
#ifdef DEBUG
	struct vm_page *i;
	int mti;
#endif

	KDASSERT(pg->fpgsz >= 1);
	KDASSERT(pg->pg_flags & PQ_FREE);

	memtype = uvm_pmr_pg_to_memtype(pg);
#ifdef DEBUG
	for (mti = 0; mti < UVM_PMR_MEMTYPE_MAX; mti++) {
		TAILQ_FOREACH(i, &pmr->single[mti], pageq)
			KDASSERT(i != pg);
		if (pg->fpgsz > 1) {
			KDASSERT(RB_FIND(uvm_pmr_size, &pmr->size[mti],
			    pg + 1) == NULL);
		}
		KDASSERT(RB_FIND(uvm_pmr_addr, &pmr->addr, pg) == pg);
	}
	for (i = pg; i < pg + pg->fpgsz; i++)
		KASSERT(uvm_pmr_pg_to_memtype(i) == memtype);
#endif

	if (pg->fpgsz == 1)
		TAILQ_INSERT_TAIL(&pmr->single[memtype], pg, pageq);
	else
		RB_INSERT(uvm_pmr_size, &pmr->size[memtype], pg + 1);
}
/* Insert in both trees. */
struct vm_page *
uvm_pmr_insert(struct uvm_pmemrange *pmr, struct vm_page *pg, int no_join)
{
	uvm_pmr_assertvalid(pmr);
	pg = uvm_pmr_insert_addr(pmr, pg, no_join);
	uvm_pmr_insert_size(pmr, pg);
	uvm_pmr_assertvalid(pmr);
	return pg;
}

/*
 * Find the last page that is part of this segment.
 * => pg: the range at which to start the search.
 * => boundary: the page number boundary specification (0 = no boundary).
 * => pmr: the pmemrange of the page.
 * 
 * This function returns 1 before the next range, so if you want to have the
 * next range, you need to run TAILQ_NEXT(result, pageq) after calling.
 * The reason is that this way, the length of the segment is easily
 * calculated using: atop(result) - atop(pg) + 1.
 * Hence this function also never returns NULL.
 */
struct vm_page *
uvm_pmr_findnextsegment(struct uvm_pmemrange *pmr,
    struct vm_page *pg, paddr_t boundary)
{
	paddr_t	first_boundary;
	struct	vm_page *next;
	struct	vm_page *prev;

	KDASSERT(pmr->low <= atop(VM_PAGE_TO_PHYS(pg)) &&
	    pmr->high > atop(VM_PAGE_TO_PHYS(pg)));
	if (boundary != 0) {
		first_boundary =
		    PMR_ALIGN(atop(VM_PAGE_TO_PHYS(pg)) + 1, boundary);
	} else
		first_boundary = 0;

	/*
	 * Increase next until it hits the first page of the next segment.
	 *
	 * While loop checks the following:
	 * - next != NULL	we have not reached the end of pgl
	 * - boundary == 0 || next < first_boundary
	 *			we do not cross a boundary
	 * - atop(prev) + 1 == atop(next)
	 *			still in the same segment
	 * - low <= last
	 * - high > last	still in the same memory range
	 * - memtype is equal	allocator is unable to view different memtypes
	 *			as part of the same segment
	 * - prev + 1 == next	no array breakage occurs
	 */
	prev = pg;
	next = TAILQ_NEXT(prev, pageq);
	while (next != NULL &&
	    (boundary == 0 || atop(VM_PAGE_TO_PHYS(next)) < first_boundary) &&
	    atop(VM_PAGE_TO_PHYS(prev)) + 1 == atop(VM_PAGE_TO_PHYS(next)) &&
	    pmr->low <= atop(VM_PAGE_TO_PHYS(next)) &&
	    pmr->high > atop(VM_PAGE_TO_PHYS(next)) &&
	    uvm_pmr_pg_to_memtype(prev) == uvm_pmr_pg_to_memtype(next) &&
	    prev + 1 == next) {
		prev = next;
		next = TAILQ_NEXT(prev, pageq);
	}

	/*
	 * End of this segment.
	 */
	return prev;
}

/*
 * Remove the first segment of contiguous pages from pgl.
 * A segment ends if it crosses boundary (unless boundary = 0) or
 * if it would enter a different uvm_pmemrange.
 *
 * Work: the page range that the caller is currently working with.
 * May be null.
 *
 * If is_desperate is non-zero, the smallest segment is erased. Otherwise,
 * the first segment is erased (which, if called by uvm_pmr_getpages(),
 * probably is the smallest or very close to it).
 */
psize_t
uvm_pmr_remove_1strange(struct pglist *pgl, paddr_t boundary,
    struct vm_page **work, int is_desperate)
{
	struct vm_page *start, *end, *iter, *iter_end, *inserted;
	psize_t count;
	struct uvm_pmemrange *pmr, *pmr_iter;

	KASSERT(!TAILQ_EMPTY(pgl));

	/*
	 * Initialize to first page.
	 * Unless desperate scan finds a better candidate, this is what'll be
	 * erased.
	 */
	start = TAILQ_FIRST(pgl);
	pmr = uvm_pmemrange_find(atop(VM_PAGE_TO_PHYS(start)));
	end = uvm_pmr_findnextsegment(pmr, start, boundary);

	/*
	 * If we are desperate, we _really_ want to get rid of the smallest
	 * element (rather than a close match to the smallest element).
	 */
	if (is_desperate) {
		/* Linear search for smallest segment. */
		pmr_iter = pmr;
		for (iter = TAILQ_NEXT(end, pageq);
		    iter != NULL && start != end;
		    iter = TAILQ_NEXT(iter_end, pageq)) {
			/*
			 * Only update pmr if it doesn't match current
			 * iteration.
			 */
			if (pmr->low > atop(VM_PAGE_TO_PHYS(iter)) ||
			    pmr->high <= atop(VM_PAGE_TO_PHYS(iter))) {
				pmr_iter = uvm_pmemrange_find(atop(
				    VM_PAGE_TO_PHYS(iter)));
			}

			iter_end = uvm_pmr_findnextsegment(pmr_iter, iter,
			    boundary);

			/*
			 * Current iteration is smaller than best match so
			 * far; update.
			 */
			if (VM_PAGE_TO_PHYS(iter_end) - VM_PAGE_TO_PHYS(iter) <
			    VM_PAGE_TO_PHYS(end) - VM_PAGE_TO_PHYS(start)) {
				start = iter;
				end = iter_end;
				pmr = pmr_iter;
			}
		}
	}

	/*
	 * Calculate count and end of the list.
	 */
	count = atop(VM_PAGE_TO_PHYS(end) - VM_PAGE_TO_PHYS(start)) + 1;
	end = TAILQ_NEXT(end, pageq);

	/*
	 * Actually remove the range of pages.
	 *
	 * Sadly, this cannot be done using pointer iteration:
	 * vm_physseg is not guaranteed to be sorted on address, hence
	 * uvm_page_init() may not have initialized its array sorted by
	 * page number.
	 */
	for (iter = start; iter != end; iter = iter_end) {
		iter_end = TAILQ_NEXT(iter, pageq);
		TAILQ_REMOVE(pgl, iter, pageq);
	}

	start->fpgsz = count;
	inserted = uvm_pmr_insert(pmr, start, 0);

	/*
	 * If the caller was working on a range and this function modified
	 * that range, update the pointer.
	 */
	if (work != NULL && *work != NULL &&
	    atop(VM_PAGE_TO_PHYS(inserted)) <= atop(VM_PAGE_TO_PHYS(*work)) &&
	    atop(VM_PAGE_TO_PHYS(inserted)) + inserted->fpgsz >
	    atop(VM_PAGE_TO_PHYS(*work)))
		*work = inserted;
	return count;
}

/*
 * Extract a number of pages from a segment of free pages.
 * Called by uvm_pmr_getpages.
 *
 * Returns the segment that was created from pages left over at the tail
 * of the remove set of pages, or NULL if no pages were left at the tail.
 */
struct vm_page *
uvm_pmr_extract_range(struct uvm_pmemrange *pmr, struct vm_page *pg,
    paddr_t start, paddr_t end, struct pglist *result)
{
	struct vm_page *after, *pg_i;
	psize_t before_sz, after_sz;
#ifdef DEBUG
	psize_t i;
#endif

	KDASSERT(end > start);
	KDASSERT(pmr->low <= atop(VM_PAGE_TO_PHYS(pg)));
	KDASSERT(pmr->high >= atop(VM_PAGE_TO_PHYS(pg)) + pg->fpgsz);
	KDASSERT(atop(VM_PAGE_TO_PHYS(pg)) <= start);
	KDASSERT(atop(VM_PAGE_TO_PHYS(pg)) + pg->fpgsz >= end);

	before_sz = start - atop(VM_PAGE_TO_PHYS(pg));
	after_sz = atop(VM_PAGE_TO_PHYS(pg)) + pg->fpgsz - end;
	KDASSERT(before_sz + after_sz + (end - start) == pg->fpgsz);
	uvm_pmr_assertvalid(pmr);

	uvm_pmr_remove_size(pmr, pg);
	if (before_sz == 0)
		uvm_pmr_remove_addr(pmr, pg);
	after = pg + before_sz + (end - start);

	/* Add selected pages to result. */
	for (pg_i = pg + before_sz; pg_i != after; pg_i++) {
		KDASSERT(pg_i->pg_flags & PQ_FREE);
		pg_i->fpgsz = 0;
		TAILQ_INSERT_TAIL(result, pg_i, pageq);
	}

	/* Before handling. */
	if (before_sz > 0) {
		pg->fpgsz = before_sz;
		uvm_pmr_insert_size(pmr, pg);
	}

	/* After handling. */
	if (after_sz > 0) {
#ifdef DEBUG
		for (i = 0; i < after_sz; i++) {
			KASSERT(!uvm_pmr_isfree(after + i));
		}
#endif
		KDASSERT(atop(VM_PAGE_TO_PHYS(after)) == end);
		after->fpgsz = after_sz;
		after = uvm_pmr_insert_addr(pmr, after, 1);
		uvm_pmr_insert_size(pmr, after);
	}

	uvm_pmr_assertvalid(pmr);
	return (after_sz > 0 ? after : NULL);
}

/*
 * Acquire a number of pages.
 *
 * count:	the number of pages returned
 * start:	lowest page number
 * end:		highest page number +1
 * 		(start = end = 0: no limitation)
 * align:	power-of-2 alignment constraint (align = 1: no alignment)
 * boundary:	power-of-2 boundary (boundary = 0: no boundary)
 * maxseg:	maximum number of segments to return
 * flags:	UVM_PLA_* flags
 * result:	returned pages storage (uses pageq)
 */
int
uvm_pmr_getpages(psize_t count, paddr_t start, paddr_t end, paddr_t align,
    paddr_t boundary, int maxseg, int flags, struct pglist *result)
{
	struct	uvm_pmemrange *pmr;	/* Iterate memory ranges. */
	struct	vm_page *found, *f_next; /* Iterate chunks. */
	psize_t	fcount;			/* Current found pages. */
	int	fnsegs;			/* Current segment counter. */
	int	try, start_try;
	psize_t	search[3];
	paddr_t	fstart, fend;		/* Pages to be taken from found. */
	int	memtype;		/* Requested memtype. */
	int	memtype_init;		/* Best memtype. */
	int	desperate;		/* True if allocation failed. */
#ifdef DIAGNOSTIC
	struct	vm_page *diag_prev;	/* Used during validation. */
#endif /* DIAGNOSTIC */

	/*
	 * Validate arguments.
	 */
	KASSERT(count > 0 &&
	    (start == 0 || end == 0 || start < end) &&
	    align >= 1 && powerof2(align) &&
	    maxseg > 0 &&
	    (boundary == 0 || powerof2(boundary)) &&
	    (boundary == 0 || maxseg * boundary >= count) &&
	    TAILQ_EMPTY(result));

	/*
	 * TRYCONTIG is a noop if you only want a single segment.
	 * Remove it if that's the case: otherwise it'll deny the fast
	 * allocation.
	 */
	if (maxseg == 1 || count == 1)
		flags &= ~UVM_PLA_TRYCONTIG;

	/*
	 * Configure search.
	 *
	 * search[0] is one segment, only used in UVM_PLA_TRYCONTIG case.
	 * search[1] is multiple segments, chosen to fulfill the search in
	 *   approximately even-sized segments.
	 *   This is a good trade-off between slightly reduced allocation speed
	 *   and less fragmentation.
	 * search[2] is the worst case, in which all segments are evaluated.
	 *   This provides the least fragmentation, but makes the search
	 *   possibly longer (although in the case it is selected, that no
	 *   longer matters most).
	 *
	 * The exception is when maxseg == 1: since we can only fulfill that
	 * with one segment of size pages, only a single search type has to
	 * be attempted.
	 */
	if (maxseg == 1 || count == 1) {
		start_try = 2;
		search[2] = count;
	} else if (maxseg >= count && (flags & UVM_PLA_TRYCONTIG) == 0) {
		start_try = 2;
		search[2] = 1;
	} else {
		start_try = 0;
		search[0] = count;
		search[1] = pow2divide(count, maxseg);
		search[2] = 1;
		if ((flags & UVM_PLA_TRYCONTIG) == 0)
			start_try = 1;
		if (search[1] >= search[0]) {
			search[1] = search[0];
			start_try = 1;
		}
		if (search[2] >= search[start_try]) {
			start_try = 2;
		}
	}

	/*
	 * Memory type: if zeroed memory is requested, traverse the zero set.
	 * Otherwise, traverse the dirty set.
	 *
	 * The memtype iterator is reinitialized to memtype_init on entrance
	 * of a pmemrange.
	 */
	if (flags & UVM_PLA_ZERO)
		memtype_init = UVM_PMR_MEMTYPE_ZERO;
	else
		memtype_init = UVM_PMR_MEMTYPE_DIRTY;

	/*
	 * Initially, we're not desperate.
	 *
	 * Note that if we return from a sleep, we are still desperate.
	 * Chances are that memory pressure is still high, so resetting
	 * seems over-optimistic to me.
	 */
	desperate = 0;

	uvm_lock_fpageq();

retry:		/* Return point after sleeping. */
	fcount = 0;
	fnsegs = 0;

retry_desperate:
	/*
	 * If we just want any page(s), go for the really fast option.
	 */
	if (count <= maxseg && align == 1 && boundary == 0 &&
	    (flags & UVM_PLA_TRYCONTIG) == 0) {
		fcount += uvm_pmr_get1page(count - fcount, memtype_init,
		    result, start, end);

		/*
		 * If we found sufficient pages, go to the succes exit code.
		 *
		 * Otherwise, go immediately to fail, since we collected
		 * all we could anyway.
		 */
		if (fcount == count)
			goto out;
		else
			goto fail;
	}

	/*
	 * The heart of the contig case.
	 *
	 * The code actually looks like this:
	 *
	 * foreach (struct pmemrange) {
	 *	foreach (memtype) {
	 *		foreach(try) {
	 *			foreach (free range of memtype in pmemrange,
	 *			    starting at search[try]) {
	 *				while (range has space left)
	 *					take from range
	 *			}
	 *		}
	 *	}
	 *
	 *	if next pmemrange has higher usecount than current:
	 *		enter desperate case (which will drain the pmemranges
	 *		until empty prior to moving to the next one)
	 * }
	 *
	 * When desperate is activated, try always starts at the highest
	 * value. The memtype loop is using a goto ReScanMemtype.
	 * The try loop is using a goto ReScan.
	 * The 'range has space left' loop uses label DrainFound.
	 *
	 * Writing them all as loops would take up a lot of screen space in
	 * the form of indentation and some parts are easier to express
	 * using the labels.
	 */

	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		/* Empty range. */
		if (pmr->nsegs == 0)
			continue;

		/* Outside requested range. */
		if (!PMR_INTERSECTS_WITH(pmr->low, pmr->high, start, end))
			continue;

		memtype = memtype_init;

rescan_memtype:	/* Return point at memtype++. */
		try = start_try;

rescan:		/* Return point at try++. */
		for (found = uvm_pmr_nfindsz(pmr, search[try], memtype);
		    found != NULL;
		    found = f_next) {
			f_next = uvm_pmr_nextsz(pmr, found, memtype);

			fstart = atop(VM_PAGE_TO_PHYS(found));
			if (start != 0)
				fstart = MAX(start, fstart);
drain_found:
			/*
			 * Throw away the first segment if fnsegs == maxseg
			 *
			 * Note that f_next is still valid after this call,
			 * since we only allocated from entries before f_next.
			 * We don't revisit the entries we already extracted
			 * from unless we entered the desperate case.
			 */
			if (fnsegs == maxseg) {
				fnsegs--;
				fcount -=
				    uvm_pmr_remove_1strange(result, boundary,
				    &found, desperate);
			}

			fstart = PMR_ALIGN(fstart, align);
			fend = atop(VM_PAGE_TO_PHYS(found)) + found->fpgsz;
			if (fstart >= fend)
				continue;
			if (boundary != 0) {
				fend =
				    MIN(fend, PMR_ALIGN(fstart + 1, boundary));
			}
			if (end != 0)
				fend = MIN(end, fend);
			if (fend - fstart > count - fcount)
				fend = fstart + (count - fcount);

			fcount += fend - fstart;
			fnsegs++;
			found = uvm_pmr_extract_range(pmr, found,
			    fstart, fend, result);

			if (fcount == count)
				goto out;

			/*
			 * If there's still space left in found, try to
			 * fully drain it prior to continueing.
			 */
			if (found != NULL) {
				fstart = fend;
				goto drain_found;
			}
		}

		/*
		 * Try a smaller search now.
		 */
		if (++try < nitems(search))
			goto rescan;

		/*
		 * Exhaust all memory types prior to going to the next memory
		 * segment.
		 * This means that zero-vs-dirty are eaten prior to moving
		 * to a pmemrange with a higher use-count.
		 *
		 * Code is basically a difficult way of writing:
		 * memtype = memtype_init;
		 * do {
		 *	...;
		 *	memtype += 1;
		 *	memtype %= MEMTYPE_MAX;
		 * } while (memtype != memtype_init);
		 */
		memtype += 1;
		if (memtype == UVM_PMR_MEMTYPE_MAX)
			memtype = 0;
		if (memtype != memtype_init)
			goto rescan_memtype;

		/*
		 * If not desperate, enter desperate case prior to eating all
		 * the good stuff in the next range.
		 */
		if (!desperate && TAILQ_NEXT(pmr, pmr_use) != NULL &&
		    TAILQ_NEXT(pmr, pmr_use)->use != pmr->use)
			break;
	}

	/*
	 * Not enough memory of the requested type available. Fall back to
	 * less good memory that we'll clean up better later.
	 *
	 * This algorithm is not very smart though, it just starts scanning
	 * a different typed range, but the nicer ranges of the previous
	 * iteration may fall out. Hence there is a small chance of a false
	 * negative.
	 *
	 * When desparate: scan all sizes starting at the smallest
	 * (start_try = 1) and do not consider UVM_PLA_TRYCONTIG (which may
	 * allow us to hit the fast path now).
	 *
	 * Also, because we will revisit entries we scanned before, we need
	 * to reset the page queue, or we may end up releasing entries in
	 * such a way as to invalidate f_next.
	 */
	if (!desperate) {
		desperate = 1;
		start_try = nitems(search) - 1;
		flags &= ~UVM_PLA_TRYCONTIG;

		while (!TAILQ_EMPTY(result))
			uvm_pmr_remove_1strange(result, 0, NULL, 0);
		fnsegs = 0;
		fcount = 0;
		goto retry_desperate;
	}

fail:
	/*
	 * Allocation failed.
	 */

	/* XXX: claim from memory reserve here */

	while (!TAILQ_EMPTY(result))
		uvm_pmr_remove_1strange(result, 0, NULL, 0);

	if (flags & UVM_PLA_WAITOK) {
		if (uvm_wait_pla(ptoa(start), ptoa(end) - 1, ptoa(count),
		    flags & UVM_PLA_FAILOK) == 0)
			goto retry;
		KASSERT(flags & UVM_PLA_FAILOK);
	} else
		wakeup(&uvm.pagedaemon);
	uvm_unlock_fpageq();

	return ENOMEM;

out:

	/*
	 * Allocation succesful.
	 */

	uvmexp.free -= fcount;

	uvm_unlock_fpageq();

	/* Update statistics and zero pages if UVM_PLA_ZERO. */
#ifdef DIAGNOSTIC
	fnsegs = 0;
	fcount = 0;
	diag_prev = NULL;
#endif /* DIAGNOSTIC */
	TAILQ_FOREACH(found, result, pageq) {
		atomic_clearbits_int(&found->pg_flags,
		    PG_PMAP0|PG_PMAP1|PG_PMAP2|PG_PMAP3);

		if (found->pg_flags & PG_ZERO) {
			uvmexp.zeropages--;
		}
		if (flags & UVM_PLA_ZERO) {
			if (found->pg_flags & PG_ZERO)
				uvmexp.pga_zerohit++;
			else {
				uvmexp.pga_zeromiss++;
				uvm_pagezero(found);
			}
		}
		atomic_clearbits_int(&found->pg_flags, PG_ZERO|PQ_FREE);

		found->uobject = NULL;
		found->uanon = NULL;
		found->pg_version++;

		/*
		 * Validate that the page matches range criterium.
		 */
		KDASSERT(start == 0 || atop(VM_PAGE_TO_PHYS(found)) >= start);
		KDASSERT(end == 0 || atop(VM_PAGE_TO_PHYS(found)) < end);

#ifdef DIAGNOSTIC
		/*
		 * Update fcount (# found pages) and
		 * fnsegs (# found segments) counters.
		 */
		if (diag_prev == NULL ||
		    /* new segment if it contains a hole */
		    atop(VM_PAGE_TO_PHYS(diag_prev)) + 1 !=
		    atop(VM_PAGE_TO_PHYS(found)) ||
		    /* new segment if it crosses boundary */
		    (atop(VM_PAGE_TO_PHYS(diag_prev)) & ~(boundary - 1)) !=
		    (atop(VM_PAGE_TO_PHYS(found)) & ~(boundary - 1)))
			fnsegs++;
		fcount++;

		diag_prev = found;
#endif /* DIAGNOSTIC */
	}

#ifdef DIAGNOSTIC
	/*
	 * Panic on algorithm failure.
	 */
	if (fcount != count || fnsegs > maxseg) {
		panic("pmemrange allocation error: "
		    "allocated %ld pages in %d segments, "
		    "but request was %ld pages in %d segments",
		    fcount, fnsegs, count, maxseg);
	}
#endif /* DIAGNOSTIC */

	return 0;
}

/*
 * Free a number of contig pages (invoked by uvm_page_init).
 */
void
uvm_pmr_freepages(struct vm_page *pg, psize_t count)
{
	struct uvm_pmemrange *pmr;
	psize_t i, pmr_count;

	for (i = 0; i < count; i++) {
		KASSERT(atop(VM_PAGE_TO_PHYS(&pg[i])) ==
		    atop(VM_PAGE_TO_PHYS(pg)) + i);

		if (!((pg[i].pg_flags & PQ_FREE) == 0 &&
		    VALID_FLAGS(pg[i].pg_flags))) {
			printf("Flags: 0x%x, will panic now.\n",
			    pg[i].pg_flags);
		}
		KASSERT((pg[i].pg_flags & PQ_FREE) == 0 &&
		    VALID_FLAGS(pg[i].pg_flags));
		atomic_setbits_int(&pg[i].pg_flags, PQ_FREE);
		atomic_clearbits_int(&pg[i].pg_flags, PG_ZERO);
	}

	uvm_lock_fpageq();

	while (count > 0) {
		pmr = uvm_pmemrange_find(atop(VM_PAGE_TO_PHYS(pg)));
		KASSERT(pmr != NULL);

		pmr_count = MIN(count, pmr->high - atop(VM_PAGE_TO_PHYS(pg)));
		pg->fpgsz = pmr_count;
		uvm_pmr_insert(pmr, pg, 0);

		uvmexp.free += pmr_count;
		count -= pmr_count;
		pg += pmr_count;
	}
	wakeup(&uvmexp.free);

	uvm_wakeup_pla(VM_PAGE_TO_PHYS(pg), ptoa(count));

	uvm_unlock_fpageq();
}

/*
 * Free all pages in the queue.
 */
void
uvm_pmr_freepageq(struct pglist *pgl)
{
	struct vm_page *pg;
	paddr_t pstart;
	psize_t plen;

	TAILQ_FOREACH(pg, pgl, pageq) {
		if (!((pg->pg_flags & PQ_FREE) == 0 &&
		    VALID_FLAGS(pg->pg_flags))) {
			printf("Flags: 0x%x, will panic now.\n",
			    pg->pg_flags);
		}
		KASSERT((pg->pg_flags & PQ_FREE) == 0 &&
		    VALID_FLAGS(pg->pg_flags));
		atomic_setbits_int(&pg->pg_flags, PQ_FREE);
		atomic_clearbits_int(&pg->pg_flags, PG_ZERO);
	}

	uvm_lock_fpageq();
	while (!TAILQ_EMPTY(pgl)) {
		pstart = VM_PAGE_TO_PHYS(TAILQ_FIRST(pgl));
		plen = uvm_pmr_remove_1strange(pgl, 0, NULL, 0);
		uvmexp.free += plen;

		uvm_wakeup_pla(pstart, ptoa(plen));
	}
	wakeup(&uvmexp.free);
	uvm_unlock_fpageq();

	return;
}

/*
 * Store a pmemrange in the list.
 *
 * The list is sorted by use.
 */
struct uvm_pmemrange *
uvm_pmemrange_use_insert(struct uvm_pmemrange_use *useq,
    struct uvm_pmemrange *pmr)
{
	struct uvm_pmemrange *iter;
	int cmp = 1;

	TAILQ_FOREACH(iter, useq, pmr_use) {
		cmp = uvm_pmemrange_use_cmp(pmr, iter);
		if (cmp == 0)
			return iter;
		if (cmp == -1)
			break;
	}

	if (iter == NULL)
		TAILQ_INSERT_TAIL(useq, pmr, pmr_use);
	else
		TAILQ_INSERT_BEFORE(iter, pmr, pmr_use);
	return NULL;
}

#ifdef DEBUG
/*
 * Validation of the whole pmemrange.
 * Called with fpageq locked.
 */
void
uvm_pmr_assertvalid(struct uvm_pmemrange *pmr)
{
	struct vm_page *prev, *next, *i, *xref;
	int lcv, mti;

	/* Empty range */
	if (pmr->nsegs == 0)
		return;

	/* Validate address tree. */
	RB_FOREACH(i, uvm_pmr_addr, &pmr->addr) {
		/* Validate the range. */
		KASSERT(i->fpgsz > 0);
		KASSERT(atop(VM_PAGE_TO_PHYS(i)) >= pmr->low);
		KASSERT(atop(VM_PAGE_TO_PHYS(i)) + i->fpgsz
		    <= pmr->high);

		/* Validate each page in this range. */
		for (lcv = 0; lcv < i->fpgsz; lcv++) {
			/*
			 * Only the first page has a size specification.
			 * Rest is size 0.
			 */
			KASSERT(lcv == 0 || i[lcv].fpgsz == 0);
			/*
			 * Flag check.
			 */
			KASSERT(VALID_FLAGS(i[lcv].pg_flags) &&
			    (i[lcv].pg_flags & PQ_FREE) == PQ_FREE);
			/*
			 * Free pages are:
			 * - not wired
			 * - not loaned
			 * - have no vm_anon
			 * - have no uvm_object
			 */
			KASSERT(i[lcv].wire_count == 0);
			KASSERT(i[lcv].loan_count == 0);
			KASSERT(i[lcv].uanon == (void*)0xdeadbeef ||
			    i[lcv].uanon == NULL);
			KASSERT(i[lcv].uobject == (void*)0xdeadbeef ||
			    i[lcv].uobject == NULL);
			/*
			 * Pages in a single range always have the same
			 * memtype.
			 */
			KASSERT(uvm_pmr_pg_to_memtype(&i[0]) ==
			    uvm_pmr_pg_to_memtype(&i[lcv]));
		}

		/* Check that it shouldn't be joined with its predecessor. */
		prev = RB_PREV(uvm_pmr_addr, &pmr->addr, i);
		if (prev != NULL) {
			KASSERT(uvm_pmr_pg_to_memtype(i) !=
			    uvm_pmr_pg_to_memtype(prev) ||
			    atop(VM_PAGE_TO_PHYS(i)) >
			    atop(VM_PAGE_TO_PHYS(prev)) + prev->fpgsz ||
			    prev + prev->fpgsz != i);
		}

		/* Assert i is in the size tree as well. */
		if (i->fpgsz == 1) {
			TAILQ_FOREACH(xref,
			    &pmr->single[uvm_pmr_pg_to_memtype(i)], pageq) {
				if (xref == i)
					break;
			}
			KASSERT(xref == i);
		} else {
			KASSERT(RB_FIND(uvm_pmr_size,
			    &pmr->size[uvm_pmr_pg_to_memtype(i)], i + 1) ==
			    i + 1);
		}
	}

	/* Validate size tree. */
	for (mti = 0; mti < UVM_PMR_MEMTYPE_MAX; mti++) {
		for (i = uvm_pmr_nfindsz(pmr, 1, mti); i != NULL; i = next) {
			next = uvm_pmr_nextsz(pmr, i, mti);
			if (next != NULL) {
				KASSERT(i->fpgsz <=
				    next->fpgsz);
			}

			/* Assert i is in the addr tree as well. */
			KASSERT(RB_FIND(uvm_pmr_addr, &pmr->addr, i) == i);

			/* Assert i is of the correct memory type. */
			KASSERT(uvm_pmr_pg_to_memtype(i) == mti);
		}
	}

	/* Validate nsegs statistic. */
	lcv = 0;
	RB_FOREACH(i, uvm_pmr_addr, &pmr->addr)
		lcv++;
	KASSERT(pmr->nsegs == lcv);
}
#endif /* DEBUG */

/*
 * Split pmr at split point pageno.
 * Called with fpageq unlocked.
 *
 * Split is only applied if a pmemrange spans pageno.
 */
void
uvm_pmr_split(paddr_t pageno)
{
	struct uvm_pmemrange *pmr, *drain;
	struct vm_page *rebuild, *prev, *next;
	psize_t prev_sz;

	uvm_lock_fpageq();
	pmr = uvm_pmemrange_find(pageno);
	if (pmr == NULL || !(pmr->low < pageno)) {
		/* No split required. */
		uvm_unlock_fpageq();
		return;
	}

	KASSERT(pmr->low < pageno);
	KASSERT(pmr->high > pageno);

	/*
	 * uvm_pmr_allocpmr() calls into malloc() which in turn calls into
	 * uvm_kmemalloc which calls into pmemrange, making the locking
	 * a bit hard, so we just race!
	 */
	uvm_unlock_fpageq();
	drain = uvm_pmr_allocpmr();
	uvm_lock_fpageq();
	pmr = uvm_pmemrange_find(pageno);
	if (pmr == NULL || !(pmr->low < pageno)) {
		/*
		 * We lost the race since someone else ran this or a related
		 * function, however this should be triggered very rarely so
		 * we just leak the pmr.
		 */
		printf("uvm_pmr_split: lost one pmr\n");
		uvm_unlock_fpageq();
		return;
	}

	drain->low = pageno;
	drain->high = pmr->high;
	drain->use = pmr->use;

	uvm_pmr_assertvalid(pmr);
	uvm_pmr_assertvalid(drain);
	KASSERT(drain->nsegs == 0);

	RB_FOREACH(rebuild, uvm_pmr_addr, &pmr->addr) {
		if (atop(VM_PAGE_TO_PHYS(rebuild)) >= pageno)
			break;
	}
	if (rebuild == NULL)
		prev = RB_MAX(uvm_pmr_addr, &pmr->addr);
	else
		prev = RB_PREV(uvm_pmr_addr, &pmr->addr, rebuild);
	KASSERT(prev == NULL || atop(VM_PAGE_TO_PHYS(prev)) < pageno);

	/*
	 * Handle free chunk that spans the split point.
	 */
	if (prev != NULL &&
	    atop(VM_PAGE_TO_PHYS(prev)) + prev->fpgsz > pageno) {
		psize_t before, after;

		KASSERT(atop(VM_PAGE_TO_PHYS(prev)) < pageno);

		uvm_pmr_remove(pmr, prev);
		prev_sz = prev->fpgsz;
		before = pageno - atop(VM_PAGE_TO_PHYS(prev));
		after = atop(VM_PAGE_TO_PHYS(prev)) + prev_sz - pageno;

		KASSERT(before > 0);
		KASSERT(after > 0);

		prev->fpgsz = before;
		uvm_pmr_insert(pmr, prev, 1);
		(prev + before)->fpgsz = after;
		uvm_pmr_insert(drain, prev + before, 1);
	}

	/*
	 * Move free chunks that no longer fall in the range.
	 */
	for (; rebuild != NULL; rebuild = next) {
		next = RB_NEXT(uvm_pmr_addr, &pmr->addr, rebuild);

		uvm_pmr_remove(pmr, rebuild);
		uvm_pmr_insert(drain, rebuild, 1);
	}

	pmr->high = pageno;
	uvm_pmr_assertvalid(pmr);
	uvm_pmr_assertvalid(drain);

	RB_INSERT(uvm_pmemrange_addr, &uvm.pmr_control.addr, drain);
	uvm_pmemrange_use_insert(&uvm.pmr_control.use, drain);
	uvm_unlock_fpageq();
}

/*
 * Increase the usage counter for the given range of memory.
 *
 * The more usage counters a given range of memory has, the more will be
 * attempted not to allocate from it.
 *
 * Addresses here are in paddr_t, not page-numbers.
 * The lowest and highest allowed address are specified.
 */
void
uvm_pmr_use_inc(paddr_t low, paddr_t high)
{
	struct uvm_pmemrange *pmr;
	paddr_t sz;

	/* pmr uses page numbers, translate low and high. */
	high++;
	high = atop(trunc_page(high));
	low = atop(round_page(low));
	uvm_pmr_split(low);
	uvm_pmr_split(high);

	sz = 0;
	uvm_lock_fpageq();
	/* Increase use count on segments in range. */
	RB_FOREACH(pmr, uvm_pmemrange_addr, &uvm.pmr_control.addr) {
		if (PMR_IS_SUBRANGE_OF(pmr->low, pmr->high, low, high)) {
			TAILQ_REMOVE(&uvm.pmr_control.use, pmr, pmr_use);
			pmr->use++;
			sz += pmr->high - pmr->low;
			uvm_pmemrange_use_insert(&uvm.pmr_control.use, pmr);
		}
		uvm_pmr_assertvalid(pmr);
	}
	uvm_unlock_fpageq();

	KASSERT(sz >= high - low);
}

/*
 * Allocate a pmemrange.
 *
 * If called from uvm_page_init, the uvm_pageboot_alloc is used.
 * If called after uvm_init, malloc is used.
 * (And if called in between, you're dead.)
 */
struct uvm_pmemrange *
uvm_pmr_allocpmr(void)
{
	struct uvm_pmemrange *nw;
	int i;

	/* We're only ever hitting the !uvm.page_init_done case for now. */
	if (!uvm.page_init_done) {
		nw = (struct uvm_pmemrange *)
		    uvm_pageboot_alloc(sizeof(struct uvm_pmemrange));
	} else {
		nw = malloc(sizeof(struct uvm_pmemrange),
		    M_VMMAP, M_NOWAIT);
	}
	KASSERT(nw != NULL);
	bzero(nw, sizeof(struct uvm_pmemrange));
	RB_INIT(&nw->addr);
	for (i = 0; i < UVM_PMR_MEMTYPE_MAX; i++) {
		RB_INIT(&nw->size[i]);
		TAILQ_INIT(&nw->single[i]);
	}
	return nw;
}

/*
 * Initialization of pmr.
 * Called by uvm_page_init.
 *
 * Sets up pmemranges.
 */
void
uvm_pmr_init(void)
{
	struct uvm_pmemrange *new_pmr;
	int i;

	TAILQ_INIT(&uvm.pmr_control.use);
	RB_INIT(&uvm.pmr_control.addr);
	TAILQ_INIT(&uvm.pmr_control.allocs);

	/* By default, one range for the entire address space. */
	new_pmr = uvm_pmr_allocpmr();
	new_pmr->low = 0;
	new_pmr->high = atop((paddr_t)-1) + 1; 

	RB_INSERT(uvm_pmemrange_addr, &uvm.pmr_control.addr, new_pmr);
	uvm_pmemrange_use_insert(&uvm.pmr_control.use, new_pmr);

	for (i = 0; uvm_md_constraints[i] != NULL; i++) {
		uvm_pmr_use_inc(uvm_md_constraints[i]->ucr_low,
	    	    uvm_md_constraints[i]->ucr_high);
	}
}

/*
 * Find the pmemrange that contains the given page number.
 *
 * (Manually traverses the binary tree, because that is cheaper on stack
 * usage.)
 */
struct uvm_pmemrange *
uvm_pmemrange_find(paddr_t pageno)
{
	struct uvm_pmemrange *pmr;

	pmr = RB_ROOT(&uvm.pmr_control.addr);
	while (pmr != NULL) {
		if (pmr->low > pageno)
			pmr = RB_LEFT(pmr, pmr_addr);
		else if (pmr->high <= pageno)
			pmr = RB_RIGHT(pmr, pmr_addr);
		else
			break;
	}

	return pmr;
}

#if defined(DDB) || defined(DEBUG)
/*
 * Return true if the given page is in any of the free lists.
 * Used by uvm_page_printit.
 * This function is safe, even if the page is not on the freeq.
 * Note: does not apply locking, only called from ddb.
 */
int
uvm_pmr_isfree(struct vm_page *pg)
{
	struct vm_page *r;
	struct uvm_pmemrange *pmr;

	pmr = uvm_pmemrange_find(atop(VM_PAGE_TO_PHYS(pg)));
	if (pmr == NULL)
		return 0;
	r = RB_NFIND(uvm_pmr_addr, &pmr->addr, pg);
	if (r == NULL)
		r = RB_MAX(uvm_pmr_addr, &pmr->addr);
	else
		r = RB_PREV(uvm_pmr_addr, &pmr->addr, r);
	if (r == NULL)
		return 0; /* Empty tree. */

	KDASSERT(atop(VM_PAGE_TO_PHYS(r)) <= atop(VM_PAGE_TO_PHYS(pg)));
	return atop(VM_PAGE_TO_PHYS(r)) + r->fpgsz >
	    atop(VM_PAGE_TO_PHYS(pg));
}
#endif /* DEBUG */

/*
 * Given a root of a tree, find a range which intersects start, end and
 * is of the same memtype.
 *
 * Page must be in the address tree.
 */
struct vm_page*
uvm_pmr_rootupdate(struct uvm_pmemrange *pmr, struct vm_page *init_root,
    paddr_t start, paddr_t end, int memtype)
{
	int	direction;
	struct	vm_page *root;
	struct	vm_page *high, *high_next;
	struct	vm_page *low, *low_next;

	KDASSERT(pmr != NULL && init_root != NULL);
	root = init_root;

	/*
	 * Which direction to use for searching.
	 */
	if (start != 0 && atop(VM_PAGE_TO_PHYS(root)) + root->fpgsz <= start)
		direction =  1;
	else if (end != 0 && atop(VM_PAGE_TO_PHYS(root)) >= end)
		direction = -1;
	else /* nothing to do */
		return root;

	/*
	 * First, update root to fall within the chosen range.
	 */
	while (root && !PMR_INTERSECTS_WITH(
	    atop(VM_PAGE_TO_PHYS(root)),
	    atop(VM_PAGE_TO_PHYS(root)) + root->fpgsz,
	    start, end)) {
		if (direction == 1)
			root = RB_RIGHT(root, objt);
		else
			root = RB_LEFT(root, objt);
	}
	if (root == NULL || uvm_pmr_pg_to_memtype(root) == memtype)
		return root;

	/*
	 * Root is valid, but of the wrong memtype.
	 *
	 * Try to find a range that has the given memtype in the subtree
	 * (memtype mismatches are costly, either because the conversion
	 * is expensive, or a later allocation will need to do the opposite
	 * conversion, which will be expensive).
	 *
	 *
	 * First, simply increase address until we hit something we can use.
	 * Cache the upper page, so we can page-walk later.
	 */
	high = root;
	high_next = RB_RIGHT(high, objt);
	while (high_next != NULL && PMR_INTERSECTS_WITH(
	    atop(VM_PAGE_TO_PHYS(high_next)),
	    atop(VM_PAGE_TO_PHYS(high_next)) + high_next->fpgsz,
	    start, end)) {
		high = high_next;
		if (uvm_pmr_pg_to_memtype(high) == memtype)
			return high;
		high_next = RB_RIGHT(high, objt);
	}

	/*
	 * Second, decrease the address until we hit something we can use.
	 * Cache the lower page, so we can page-walk later.
	 */
	low = root;
	low_next = RB_RIGHT(low, objt);
	while (low_next != NULL && PMR_INTERSECTS_WITH(
	    atop(VM_PAGE_TO_PHYS(low_next)),
	    atop(VM_PAGE_TO_PHYS(low_next)) + low_next->fpgsz,
	    start, end)) {
		low = low_next;
		if (uvm_pmr_pg_to_memtype(low) == memtype)
			return low;
		low_next = RB_RIGHT(low, objt);
	}

	/*
	 * Ack, no hits. Walk the address tree until to find something usable.
	 */
	for (low = RB_NEXT(uvm_pmr_addr, &pmr->addr, low);
	    low != high;
	    low = RB_NEXT(uvm_pmr_addr, &pmr->addr, low)) {
		KDASSERT(PMR_IS_SUBRANGE_OF(atop(VM_PAGE_TO_PHYS(low)),
	    	    atop(VM_PAGE_TO_PHYS(low)) + low->fpgsz,
	    	    start, end));
		if (uvm_pmr_pg_to_memtype(low) == memtype)
			return low;
	}

	/*
	 * Nothing found.
	 */
	return NULL;
}

/*
 * Allocate any page, the fastest way. Page number constraints only.
 */
int
uvm_pmr_get1page(psize_t count, int memtype_init, struct pglist *result,
    paddr_t start, paddr_t end)
{
	struct	uvm_pmemrange *pmr;
	struct	vm_page *found, *splitpg;
	psize_t	fcount;
	int	memtype;

	fcount = 0;
	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		/* We're done. */
		if (fcount == count)
			break;

		/* Outside requested range. */
		if (!(start == 0 && end == 0) &&
		    !PMR_INTERSECTS_WITH(pmr->low, pmr->high, start, end))
			continue;

		/* Range is empty. */
		if (pmr->nsegs == 0)
			continue;

		/* Loop over all memtypes, starting at memtype_init. */
		memtype = memtype_init;
		while (fcount != count) {
			found = TAILQ_FIRST(&pmr->single[memtype]);
			/*
			 * If found is outside the range, walk the list
			 * until we find something that intersects with
			 * boundaries.
			 */
			while (found && !PMR_INTERSECTS_WITH(
			    atop(VM_PAGE_TO_PHYS(found)),
			    atop(VM_PAGE_TO_PHYS(found)) + 1,
			    start, end))
				found = TAILQ_NEXT(found, pageq);

			if (found == NULL) {
				found = RB_ROOT(&pmr->size[memtype]);
				/* Size tree gives pg[1] instead of pg[0] */
				if (found != NULL) {
					found--;

					found = uvm_pmr_rootupdate(pmr, found,
					    start, end, memtype);
				}
			}
			if (found != NULL) {
				uvm_pmr_assertvalid(pmr);
				uvm_pmr_remove_size(pmr, found);

				/*
				 * If the page intersects the end, then it'll
				 * need splitting.
				 *
				 * Note that we don't need to split if the page
				 * intersects start: the drain function will
				 * simply stop on hitting start.
				 */
				if (end != 0 && atop(VM_PAGE_TO_PHYS(found)) +
				    found->fpgsz > end) {
					psize_t splitsz =
					    atop(VM_PAGE_TO_PHYS(found)) +
					    found->fpgsz - end;

					uvm_pmr_remove_addr(pmr, found);
					uvm_pmr_assertvalid(pmr);
					found->fpgsz -= splitsz;
					splitpg = found + found->fpgsz;
					splitpg->fpgsz = splitsz;
					uvm_pmr_insert(pmr, splitpg, 1);

					/*
					 * At this point, splitpg and found
					 * actually should be joined.
					 * But we explicitly disable that,
					 * because we will start subtracting
					 * from found.
					 */
					KASSERT(start == 0 ||
					    atop(VM_PAGE_TO_PHYS(found)) +
					    found->fpgsz > start);
					uvm_pmr_insert_addr(pmr, found, 1);
				}

				/*
				 * Fetch pages from the end.
				 * If the range is larger than the requested
				 * number of pages, this saves us an addr-tree
				 * update.
				 *
				 * Since we take from the end and insert at
				 * the head, any ranges keep preserved.
				 */
				while (found->fpgsz > 0 && fcount < count &&
				    (start == 0 ||
				    atop(VM_PAGE_TO_PHYS(found)) +
				    found->fpgsz > start)) {
					found->fpgsz--;
					fcount++;
					TAILQ_INSERT_HEAD(result,
					    &found[found->fpgsz], pageq);
				}
				if (found->fpgsz > 0) {
					uvm_pmr_insert_size(pmr, found);
					KDASSERT(fcount == count);
					uvm_pmr_assertvalid(pmr);
					return fcount;
				}

				/*
				 * Delayed addr-tree removal.
				 */
				uvm_pmr_remove_addr(pmr, found);
				uvm_pmr_assertvalid(pmr);
			} else {
				/*
				 * Skip to the next memtype.
				 */
				memtype += 1;
				if (memtype == UVM_PMR_MEMTYPE_MAX)
					memtype = 0;
				if (memtype == memtype_init)
					break;
			}
		}
	}

	/*
	 * Search finished.
	 *
	 * Ran out of ranges before enough pages were gathered, or we hit the
	 * case where found->fpgsz == count - fcount, in which case the
	 * above exit condition didn't trigger.
	 *
	 * On failure, caller will free the pages.
	 */
	return fcount;
}

#ifdef DDB
/*
 * Print information about pmemrange.
 * Does not do locking (so either call it from DDB or acquire fpageq lock
 * before invoking.
 */
void
uvm_pmr_print(void)
{
	struct	uvm_pmemrange *pmr;
	struct	vm_page *pg;
	psize_t	size[UVM_PMR_MEMTYPE_MAX];
	psize_t	free;
	int	useq_len;
	int	mt;

	printf("Ranges, use queue:\n");
	useq_len = 0;
	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		useq_len++;
		free = 0;
		for (mt = 0; mt < UVM_PMR_MEMTYPE_MAX; mt++) {
			pg = RB_MAX(uvm_pmr_size, &pmr->size[mt]);
			if (pg != NULL)
				pg--;
			else
				pg = TAILQ_FIRST(&pmr->single[mt]);
			size[mt] = (pg == NULL ? 0 : pg->fpgsz);

			RB_FOREACH(pg, uvm_pmr_addr, &pmr->addr)
				free += pg->fpgsz;
		}

		printf("* [0x%lx-0x%lx] use=%d nsegs=%ld",
		    (unsigned long)pmr->low, (unsigned long)pmr->high,
		    pmr->use, (unsigned long)pmr->nsegs);
		for (mt = 0; mt < UVM_PMR_MEMTYPE_MAX; mt++) {
			printf(" maxsegsz[%d]=0x%lx", mt,
			    (unsigned long)size[mt]);
		}
		printf(" free=0x%lx\n", (unsigned long)free);
	}
	printf("#ranges = %d\n", useq_len);
}
#endif

/*
 * uvm_wait_pla: wait (sleep) for the page daemon to free some pages
 * in a specific physmem area.
 *
 * Returns ENOMEM if the pagedaemon failed to free any pages.
 * If not failok, failure will lead to panic.
 *
 * Must be called with fpageq locked.
 */
int
uvm_wait_pla(paddr_t low, paddr_t high, paddr_t size, int failok)
{
	struct uvm_pmalloc pma;
	const char *wmsg = "pmrwait";

	/*
	 * Prevent deadlock.
	 */
	if (curproc == uvm.pagedaemon_proc) {
		msleep(&uvmexp.free, &uvm.fpageqlock, PVM, wmsg, hz >> 3);
		return 0;
	}

	for (;;) {
		pma.pm_constraint.ucr_low = low;
		pma.pm_constraint.ucr_high = high;
		pma.pm_size = size;
		pma.pm_flags = UVM_PMA_LINKED;
		TAILQ_INSERT_TAIL(&uvm.pmr_control.allocs, &pma, pmq);

		wakeup(&uvm.pagedaemon);		/* wake the daemon! */
		while (pma.pm_flags & (UVM_PMA_LINKED | UVM_PMA_BUSY))
			msleep(&pma, &uvm.fpageqlock, PVM, wmsg, 0);

		if (!(pma.pm_flags & UVM_PMA_FREED) &&
		    pma.pm_flags & UVM_PMA_FAIL) {
			if (failok)
				return ENOMEM;
			printf("uvm_wait: failed to free %ld pages between "
			    "0x%lx-0x%lx\n", atop(size), low, high);
		} else
			return 0;
	}
	/* UNREACHABLE */
}

/*
 * Wake up uvm_pmalloc sleepers.
 */
void
uvm_wakeup_pla(paddr_t low, psize_t len)
{
	struct uvm_pmalloc *pma, *pma_next;
	paddr_t high;

	high = low + len;

	/*
	 * Wake specific allocations waiting for this memory.
	 */
	for (pma = TAILQ_FIRST(&uvm.pmr_control.allocs); pma != NULL;
	    pma = pma_next) {
		pma_next = TAILQ_NEXT(pma, pmq);

		if (low < pma->pm_constraint.ucr_high &&
		    high > pma->pm_constraint.ucr_low) {
			pma->pm_flags |= UVM_PMA_FREED;
			if (!(pma->pm_flags & UVM_PMA_BUSY)) {
				pma->pm_flags &= ~UVM_PMA_LINKED;
				TAILQ_REMOVE(&uvm.pmr_control.allocs, pma,
				    pmq);
				wakeup(pma);
			}
		}
	}
}

#ifndef SMALL_KERNEL
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
 * Allocate the biggest contig chunk of memory.
 */
int
uvm_pmr_alloc_pig(paddr_t *addr, psize_t *sz)
{
	struct uvm_pmemrange	*pig_pmr, *pmr;
	struct vm_page		*pig_pg, *pg;
	int			 memtype;

	uvm_lock_fpageq();
	pig_pg = NULL;
	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		for (memtype = 0; memtype < UVM_PMR_MEMTYPE_MAX; memtype++) {
			/* Find biggest page in this memtype pmr. */
			pg = RB_MAX(uvm_pmr_size, &pmr->size[memtype]);
			if (pg == NULL)
				pg = TAILQ_FIRST(&pmr->single[memtype]);
			else
				pg--;

			if (pig_pg == NULL || (pg != NULL && pig_pg != NULL &&
			    pig_pg->fpgsz < pg->fpgsz)) {
				pig_pmr = pmr;
				pig_pg = pg;
			}
		}
	}

	/* Remove page from freelist. */
	if (pig_pg != NULL) {
		uvm_pmr_remove(pig_pmr, pig_pg);
		uvmexp.free -= pig_pg->fpgsz;
		if (pig_pg->pg_flags & PG_ZERO)
			uvmexp.zeropages -= pig_pg->fpgsz;
		*addr = VM_PAGE_TO_PHYS(pig_pg);
		*sz = pig_pg->fpgsz;
	}
	uvm_unlock_fpageq();

	/* Return. */
	return (pig_pg != NULL ? 0 : ENOMEM);
}
#endif /* SMALL_KERNEL */
