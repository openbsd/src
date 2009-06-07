/*	$OpenBSD: uvm_pmemrange.c,v 1.3 2009/06/07 02:01:54 oga Exp $	*/

/*
 * Copyright (c) 2009 Ariane van der Steldt <ariane@stack.nl>
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
#include <uvm/uvm.h>
#include <sys/malloc.h>

/*
 * 2 trees: addr tree and size tree.
 *
 * addr tree is vm_page[0].fq.free.tree
 * size tree is vm_page[1].fq.free.tree
 *
 * The size tree is not used for memory ranges of 1 page, instead,
 * single queue is vm_page[0].pageq
 *
 * uvm_page_init guarantees that every vm_physseg contains an array of
 * struct vm_page. Also, uvm_page_physload allocates an array of struct
 * vm_page. This code depends on that array.
 */

/* Tree comparators. */
int	uvm_pmemrange_addr_cmp(struct uvm_pmemrange *, struct uvm_pmemrange *);
int	uvm_pmemrange_use_cmp(struct uvm_pmemrange *, struct uvm_pmemrange *);
int	uvm_pmr_addr_cmp(struct vm_page *, struct vm_page *);
int	uvm_pmr_size_cmp(struct vm_page *, struct vm_page *);

/* Memory types. The page flags are used to derive what the current memory
 * type of a page is. */
static __inline int
uvm_pmr_pg_to_memtype(struct vm_page *pg)
{
	if (pg->pg_flags & PG_ZERO)
		return UVM_PMR_MEMTYPE_ZERO;
	/* Default: dirty memory. */
	return UVM_PMR_MEMTYPE_DIRTY;
}

/* Trees. */
RB_PROTOTYPE(uvm_pmr_addr, vm_page, fq.free.tree, uvm_pmr_addr_cmp);
RB_PROTOTYPE(uvm_pmr_size, vm_page, fq.free.tree, uvm_pmr_size_cmp);
RB_PROTOTYPE(uvm_pmemrange_addr, uvm_pmemrange, pmr_addr,
    uvm_pmemrange_addr_cmp);
RB_GENERATE(uvm_pmr_addr, vm_page, fq.free.tree, uvm_pmr_addr_cmp);
RB_GENERATE(uvm_pmr_size, vm_page, fq.free.tree, uvm_pmr_size_cmp);
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
struct vm_page		*uvm_pmr_insert(struct uvm_pmemrange *,
			    struct vm_page *, int);
void			 uvm_pmr_remove(struct uvm_pmemrange *,
			    struct vm_page *);
psize_t			 uvm_pmr_remove_1strange(struct pglist *, paddr_t,
			    struct vm_page **);
void			 uvm_pmr_split(paddr_t);
struct uvm_pmemrange	*uvm_pmemrange_find(paddr_t);
struct uvm_pmemrange	*uvm_pmemrange_use_insert(struct uvm_pmemrange_use *,
			    struct uvm_pmemrange *);
struct vm_page		*uvm_pmr_extract_range(struct uvm_pmemrange *,
			    struct vm_page *, paddr_t, paddr_t,
			    struct pglist *);

/*
 * Computes num/denom and rounds it up to the next power-of-2.
 */
static __inline psize_t
pow2divide(psize_t num, psize_t denom)
{
	int rshift = 0;

	while (num > (denom << rshift))
		rshift++;
	return (paddr_t)1 << rshift;
}

/*
 * Predicate: lhs is a subrange or rhs.
 */
#define PMR_IS_SUBRANGE_OF(lhs_low, lhs_high, rhs_low, rhs_high)	\
	((lhs_low) >= (rhs_low) && (lhs_high <= rhs_high))

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
	lhs_size = (lhs - 1)->fq.free.pages;
	rhs_size = (rhs - 1)->fq.free.pages;

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
		if ((node - 1)->fq.free.pages >= sz) {
			best = (node - 1);
			node = RB_LEFT(node, fq.free.tree);
		} else
			node = RB_RIGHT(node, fq.free.tree);
	}
	return best;
}

/*
 * Finds the next range. The next range has a size >= pg->fq.free.pages.
 * Returns NULL if no more ranges are available.
 */
struct vm_page *
uvm_pmr_nextsz(struct uvm_pmemrange *pmr, struct vm_page *pg, int mt)
{
	struct vm_page *npg;

	KASSERT(pmr != NULL && pg != NULL);
	if (pg->fq.free.pages == 1) {
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
 * *pg_next == NULL if no previous range is available, that can join with
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

	/* Reset if not contig. */
	if (*pg_prev != NULL &&
	    (atop(VM_PAGE_TO_PHYS(*pg_prev)) + (*pg_prev)->fq.free.pages
	    != atop(VM_PAGE_TO_PHYS(pg)) ||
	    uvm_pmr_pg_to_memtype(*pg_prev) != uvm_pmr_pg_to_memtype(pg)))
		*pg_prev = NULL;
	if (*pg_next != NULL &&
	    (atop(VM_PAGE_TO_PHYS(pg)) + pg->fq.free.pages
	    != atop(VM_PAGE_TO_PHYS(*pg_next)) ||
	    uvm_pmr_pg_to_memtype(*pg_next) != uvm_pmr_pg_to_memtype(pg)))
		*pg_next = NULL;
	return;
}

/*
 * Remove a range from the address tree.
 * Address tree maintains pmr counters.
 */
static __inline void
uvm_pmr_remove_addr(struct uvm_pmemrange *pmr, struct vm_page *pg)
{
	KDASSERT(RB_FIND(uvm_pmr_addr, &pmr->addr, pg) == pg);
	KASSERT(pg->pg_flags & PQ_FREE);
	RB_REMOVE(uvm_pmr_addr, &pmr->addr, pg);

	pmr->nsegs--;
}
/*
 * Remove a range from the size tree.
 */
static __inline void
uvm_pmr_remove_size(struct uvm_pmemrange *pmr, struct vm_page *pg)
{
	int memtype;
#ifdef DEBUG
	struct vm_page *i;
#endif

	KASSERT(pg->pg_flags & PQ_FREE);
	memtype = uvm_pmr_pg_to_memtype(pg);

	if (pg->fq.free.pages == 1) {
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
static __inline struct vm_page *
uvm_pmr_insert_addr(struct uvm_pmemrange *pmr, struct vm_page *pg, int no_join)
{
	struct vm_page *prev, *next;

#ifdef DEBUG
	struct vm_page *i;
	int mt;

	for (mt = 0; mt < UVM_PMR_MEMTYPE_MAX; mt++) {
		TAILQ_FOREACH(i, &pmr->single[mt], pageq)
			KDASSERT(i != pg);
		if (pg->fq.free.pages > 1) {
			KDASSERT(RB_FIND(uvm_pmr_size, &pmr->size[mt],
			    pg + 1) == NULL);
		}
		KDASSERT(RB_FIND(uvm_pmr_addr, &pmr->addr, pg) == NULL);
	}
#endif

	KASSERT(pg->pg_flags & PQ_FREE);
	KASSERT(pg->fq.free.pages >= 1);

	if (!no_join) {
		uvm_pmr_pnaddr(pmr, pg, &prev, &next);
		if (next != NULL) {
			uvm_pmr_remove_size(pmr, next);
			uvm_pmr_remove_addr(pmr, next);
			pg->fq.free.pages += next->fq.free.pages;
			next->fq.free.pages = 0;
		}
		if (prev != NULL) {
			uvm_pmr_remove_size(pmr, prev);
			prev->fq.free.pages += pg->fq.free.pages;
			pg->fq.free.pages = 0;
			return prev;
		}
	}
#ifdef DEBUG
	else {
		uvm_pmr_pnaddr(pmr, pg, &prev, &next);
		KDASSERT(prev == NULL && next == NULL);
	}
#endif /* DEBUG */

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
static __inline void
uvm_pmr_insert_size(struct uvm_pmemrange *pmr, struct vm_page *pg)
{
	int memtype;
#ifdef DEBUG
	struct vm_page *i;
	int mti;
#endif

	memtype = uvm_pmr_pg_to_memtype(pg);
#ifdef DEBUG
	for (mti = 0; mti < UVM_PMR_MEMTYPE_MAX; mti++) {
		TAILQ_FOREACH(i, &pmr->single[mti], pageq)
			KDASSERT(i != pg);
		if (pg->fq.free.pages > 1) {
			KDASSERT(RB_FIND(uvm_pmr_size, &pmr->size[mti],
			    pg + 1) == NULL);
		}
		KDASSERT(RB_FIND(uvm_pmr_addr, &pmr->addr, pg) == pg);
	}
	for (i = pg; i < pg + pg->fq.free.pages; i++)
		KASSERT(uvm_pmr_pg_to_memtype(i) == memtype);
#endif

	KASSERT(pg->pg_flags & PQ_FREE);
	KASSERT(pg->fq.free.pages >= 1);

	if (pg->fq.free.pages == 1)
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
 * Remove the first segment of contiguous pages from pgl.
 * A segment ends if it crosses boundary (unless boundary = 0) or
 * if it would enter a different uvm_pmemrange.
 *
 * Work: the page range that the caller is currently working with.
 * May be null.
 */
psize_t
uvm_pmr_remove_1strange(struct pglist *pgl, paddr_t boundary,
    struct vm_page **work)
{
	struct vm_page *pg, *pre_last, *last, *inserted;
	psize_t count;
	struct uvm_pmemrange *pmr;
	paddr_t first_boundary;

	KASSERT(!TAILQ_EMPTY(pgl));

	pg = TAILQ_FIRST(pgl);
	pmr = uvm_pmemrange_find(atop(VM_PAGE_TO_PHYS(pg)));
	KDASSERT(pmr != NULL);
	if (boundary != 0) {
		first_boundary =
		    PMR_ALIGN(atop(VM_PAGE_TO_PHYS(pg)) + 1, boundary);
	} else
		first_boundary = 0;

	/* Remove all pages in the first segment. */
	pre_last = pg;
	last = TAILQ_NEXT(pre_last, pageq);
	TAILQ_REMOVE(pgl, pre_last, pageq);
	count = 1;
	/*
	 * While loop checks the following:
	 * - last != NULL	we have not reached the end of pgs
	 * - boundary == 0 || last < first_boundary
	 * 			we do not cross a boundary
	 * - atop(pre_last) + 1 == atop(last)
	 * 			still in the same segment
	 * - low <= last
	 * - high > last  	still testing the same memory range
	 *
	 * At the end of the loop, last points at the next segment
	 * and each page [pg, pre_last] (inclusive range) has been removed
	 * and count is the number of pages that have been removed.
	 */
	while (last != NULL &&
	    (boundary == 0 || atop(VM_PAGE_TO_PHYS(last)) < first_boundary) &&
	    atop(VM_PAGE_TO_PHYS(pre_last)) + 1 ==
	     atop(VM_PAGE_TO_PHYS(last)) &&
	    pmr->low <= atop(VM_PAGE_TO_PHYS(last)) &&
	    pmr->high > atop(VM_PAGE_TO_PHYS(last))) {
		count++;
		pre_last = last;
		last = TAILQ_NEXT(last, pageq);
		TAILQ_REMOVE(pgl, pre_last, pageq);
	}
	KDASSERT(TAILQ_FIRST(pgl) == last);
	KDASSERT(pg + (count - 1) == pre_last);

	pg->fq.free.pages = count;
	inserted = uvm_pmr_insert(pmr, pg, 0);

	if (work != NULL && *work != NULL &&
	    atop(VM_PAGE_TO_PHYS(inserted)) <= atop(VM_PAGE_TO_PHYS(*work)) &&
	    atop(VM_PAGE_TO_PHYS(inserted)) + inserted->fq.free.pages >
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

	KASSERT(end > start);
	KASSERT(pmr->low <= atop(VM_PAGE_TO_PHYS(pg)));
	KASSERT(pmr->high >= atop(VM_PAGE_TO_PHYS(pg)) + pg->fq.free.pages);
	KASSERT(atop(VM_PAGE_TO_PHYS(pg)) <= start);
	KASSERT(atop(VM_PAGE_TO_PHYS(pg)) + pg->fq.free.pages >= end);

	before_sz = start - atop(VM_PAGE_TO_PHYS(pg));
	after_sz = atop(VM_PAGE_TO_PHYS(pg)) + pg->fq.free.pages - end;
	KDASSERT(before_sz + after_sz + (end - start) == pg->fq.free.pages);
	uvm_pmr_assertvalid(pmr);

	uvm_pmr_remove_size(pmr, pg);
	if (before_sz == 0)
		uvm_pmr_remove_addr(pmr, pg);

	/* Add selected pages to result. */
	for (pg_i = pg + before_sz; atop(VM_PAGE_TO_PHYS(pg_i)) < end;
	    pg_i++) {
		pg_i->fq.free.pages = 0;
		TAILQ_INSERT_TAIL(result, pg_i, pageq);
		KDASSERT(pg_i->pg_flags & PQ_FREE);
	}

	/* Before handling. */
	if (before_sz > 0) {
		pg->fq.free.pages = before_sz;
		uvm_pmr_insert_size(pmr, pg);
	}

	/* After handling. */
	after = NULL;
	if (after_sz > 0) {
		after = pg + before_sz + (end - start);
#ifdef DEBUG
		for (i = 0; i < after_sz; i++) {
			KASSERT(!uvm_pmr_isfree(after + i));
		}
#endif
		KDASSERT(atop(VM_PAGE_TO_PHYS(after)) == end);
		after->fq.free.pages = after_sz;
		after = uvm_pmr_insert_addr(pmr, after, 1);
		uvm_pmr_insert_size(pmr, after);
	}

	uvm_pmr_assertvalid(pmr);
	return after;
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
	psize_t	search[2];
	paddr_t	fstart, fend;		/* Pages to be taken from found. */
	int	memtype;		/* Requested memtype. */
	int	desperate;		/* True if allocation failed. */

	/* Validate arguments. */
	KASSERT(count > 0);
	KASSERT((start == 0 && end == 0) || (start < end));
	KASSERT(align >= 1 && powerof2(align));
	KASSERT(maxseg > 0);
	KASSERT(boundary == 0 || powerof2(boundary));
	KDASSERT(boundary == 0 || maxseg * boundary >= count);
	KASSERT(TAILQ_EMPTY(result));

	/* Configure search. If start_try == 0, search[0] should be faster
	 * (because it will have to throw away less segments).
	 * search[1] is the worst case: start searching at the smallest
	 * possible range instead of starting at the range most likely to
	 * fulfill the allocation. */
	start_try = 0;
	search[0] = (flags & UVM_PLA_TRY_CONTIG ? count :
	    pow2divide(count, maxseg));
	search[1] = 1;
	if (maxseg == 1) {
		start_try = 1;
		search[1] = count;
	} else if (search[1] >= search[0])
		start_try = 1;

ReTry:		/* Return point after sleeping. */
	fcount = 0;
	fnsegs = 0;

	/* Memory type: if zeroed memory is requested, traverse the zero set.
	 * Otherwise, traverse the dirty set. */
	if (flags & UVM_PLA_ZERO)
		memtype = UVM_PMR_MEMTYPE_ZERO;
	else
		memtype = UVM_PMR_MEMTYPE_DIRTY;
	desperate = 0;

	uvm_lock_fpageq();

ReTryDesperate:
	/*
	 * If we just want any page(s), go for the really fast option.
	 */
	if (count <= maxseg && align == 1 && boundary == 0 &&
	    (flags & UVM_PLA_TRY_CONTIG) == 0) {
		if (!desperate) {
			KASSERT(fcount == 0);
			fcount += uvm_pmr_get1page(count, memtype, result,
			    start, end);
		} else {
			for (memtype = 0; memtype < UVM_PMR_MEMTYPE_MAX &&
			    fcount < count; memtype++) {
				fcount += uvm_pmr_get1page(count - fcount,
				    memtype, result, start, end);
			}
		}

		if (fcount == count)
			goto Out;
		else
			goto Fail;
	}

	TAILQ_FOREACH(pmr, &uvm.pmr_control.use, pmr_use) {
		/* Empty range. */
		if (pmr->nsegs == 0)
			continue;

		/* Outside requested range. */
		if (!(start == 0 && end == 0) &&
		    !PMR_IS_SUBRANGE_OF(pmr->low, pmr->high, start, end))
			continue;

		try = start_try;
ReScan:		/* Return point at try++. */

		for (found = uvm_pmr_nfindsz(pmr, search[try], memtype);
		    found != NULL;
		    found = f_next) {
			f_next = uvm_pmr_nextsz(pmr, found, memtype);

			fstart = atop(VM_PAGE_TO_PHYS(found));
DrainFound:
			/* Throw away the first segment if fnsegs == maxseg */
			if (fnsegs == maxseg) {
				fnsegs--;
				fcount -=
				    uvm_pmr_remove_1strange(result, boundary,
				    &found);
			}

			fstart = PMR_ALIGN(fstart, align);
			fend = atop(VM_PAGE_TO_PHYS(found)) +
			    found->fq.free.pages;
			if (fstart >= fend)
				continue;
			if (boundary != 0) {
				fend =
				    MIN(fend, PMR_ALIGN(fstart + 1, boundary));
			}
			if (fend - fstart > count - fcount)
				fend = fstart + (count - fcount);

			fcount += fend - fstart;
			fnsegs++;
			found = uvm_pmr_extract_range(pmr, found,
			    fstart, fend, result);

			if (fcount == count)
				goto Out;

			/* If there's still space left in found, try to
			 * fully drain it prior to continueing. */
			if (found != NULL) {
				fstart = fend;
				goto DrainFound;
			}
		}

		if (++try < nitems(search))
			goto ReScan;
	}

	/*
	 * Not enough memory of the requested type available. Fall back to
	 * less good memory that we'll clean up better later.
	 *
	 * This algorithm is not very smart though, it just starts scanning
	 * a different typed range, but the nicer ranges of the previous
	 * iteration may fall out.
	 */
	if (!desperate) {
		desperate = 1;
		memtype = 0;
		goto ReTryDesperate;
	} else if (++memtype < UVM_PMR_MEMTYPE_MAX)
		goto ReTryDesperate;

Fail:
	/*
	 * Allocation failed.
	 */

	/* XXX: claim from memory reserve here */

	while (!TAILQ_EMPTY(result))
		uvm_pmr_remove_1strange(result, 0, NULL);
	uvm_unlock_fpageq();

	if (flags & UVM_PLA_WAITOK) {
		uvm_wait("uvm_pmr_getpages");
		goto ReTry;
	} else
		wakeup(&uvm.pagedaemon_proc);

	return ENOMEM;

Out:

	/*
	 * Allocation succesful.
	 */

	uvmexp.free -= fcount;

	uvm_unlock_fpageq();

	/* Update statistics and zero pages if UVM_PLA_ZERO. */
	TAILQ_FOREACH(found, result, pageq) {
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
		atomic_clearbits_int(&found->pg_flags, PG_ZERO | PQ_FREE);

		found->uobject = NULL;
		found->uanon = NULL;
		found->pg_version++;
	}

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

	uvm_lock_fpageq();

	for (i = 0; i < count; i++) {
		KASSERT((pg->pg_flags & PG_DEV) == 0);
		atomic_clearbits_int(&pg[i].pg_flags, pg[i].pg_flags);
		atomic_setbits_int(&pg[i].pg_flags, PQ_FREE);
	}

	while (count > 0) {
		pmr = uvm_pmemrange_find(atop(VM_PAGE_TO_PHYS(pg)));
		KASSERT(pmr != NULL);

		pmr_count = MIN(count, pmr->high - atop(VM_PAGE_TO_PHYS(pg)));
		pg->fq.free.pages = pmr_count;
		uvm_pmr_insert(pmr, pg, 0);

		uvmexp.free += pmr_count;
		count -= pmr_count;
		pg += pmr_count;
	}
	wakeup(&uvmexp.free);

	uvm_unlock_fpageq();
}

/*
 * Free all pages in the queue.
 */
void
uvm_pmr_freepageq(struct pglist *pgl)
{
	struct vm_page *pg;

	TAILQ_FOREACH(pg, pgl, pageq) {
		KASSERT((pg->pg_flags & PG_DEV) == 0);
		atomic_clearbits_int(&pg->pg_flags, pg->pg_flags);
		atomic_setbits_int(&pg->pg_flags, PQ_FREE);
	}

	uvm_lock_fpageq();
	while (!TAILQ_EMPTY(pgl))
		uvmexp.free += uvm_pmr_remove_1strange(pgl, 0, NULL);
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
	if (cmp == 0)
		return iter;

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

	/* Validate address tree. */
	RB_FOREACH(i, uvm_pmr_addr, &pmr->addr) {
		/* Validate the range. */
		KASSERT(i->fq.free.pages > 0);
		KASSERT(atop(VM_PAGE_TO_PHYS(i)) >= pmr->low);
		KASSERT(atop(VM_PAGE_TO_PHYS(i)) + i->fq.free.pages
		    <= pmr->high);

		/* Validate each page in this range. */
		for (lcv = 0; lcv < i->fq.free.pages; lcv++) {
			KASSERT(lcv == 0 || i[lcv].fq.free.pages == 0);
			/* Flag check:
			 * - PG_ZERO: page is zeroed.
			 * - PQ_FREE: page is free.
			 * Any other flag is a mistake. */
			if (i[lcv].pg_flags !=
			    (i[lcv].pg_flags & (PG_ZERO | PQ_FREE))) {
				panic("i[%lu].pg_flags = %x, should be %x\n",
				    lcv, i[lcv].pg_flags, PG_ZERO | PQ_FREE);
			}
			/* Free pages are:
			 * - not wired
			 * - not loaned
			 * - have no vm_anon
			 * - have no uvm_object */
			KASSERT(i[lcv].wire_count == 0);
			KASSERT(i[lcv].loan_count == 0);
			KASSERT(i[lcv].uanon == NULL);
			KASSERT(i[lcv].uobject == NULL);
			/* Pages in a single range always have the same
			 * memtype. */
			KASSERT(uvm_pmr_pg_to_memtype(&i[0]) ==
			    uvm_pmr_pg_to_memtype(&i[lcv]));
		}

		/* Check that it shouldn't be joined with its predecessor. */
		prev = RB_PREV(uvm_pmr_addr, &pmr->addr, i);
		if (prev != NULL) {
			KASSERT(uvm_pmr_pg_to_memtype(&i[0]) !=
			    uvm_pmr_pg_to_memtype(&i[lcv]) ||
			    atop(VM_PAGE_TO_PHYS(i)) >
			    atop(VM_PAGE_TO_PHYS(prev)) + prev->fq.free.pages);
		}

		/* Assert i is in the size tree as well. */
		if (i->fq.free.pages == 1) {
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
				KASSERT(i->fq.free.pages <=
				    next->fq.free.pages);
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

	drain = uvm_pmr_allocpmr();
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
	    atop(VM_PAGE_TO_PHYS(prev)) + prev->fq.free.pages > pageno) {
		psize_t before, after;

		KASSERT(atop(VM_PAGE_TO_PHYS(prev)) < pageno);

		uvm_pmr_remove(pmr, prev);
		prev_sz = prev->fq.free.pages;
		before = pageno - atop(VM_PAGE_TO_PHYS(prev));
		after = atop(VM_PAGE_TO_PHYS(prev)) + prev_sz - pageno;

		KASSERT(before > 0);
		KASSERT(after > 0);

		prev->fq.free.pages = before;
		uvm_pmr_insert(pmr, prev, 1);
		(prev + before)->fq.free.pages = after;
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

	/*
	 * If high+1 == 0, then you are increasing use of the whole address
	 * space, which won't make any difference. Skip in that case.
	 */
	high++;
	if (high == 0)
		return;

	/*
	 * pmr uses page numbers, translate low and high.
	 */
	low = atop(round_page(low));
	high = atop(trunc_page(high));
	uvm_pmr_split(low);
	uvm_pmr_split(high);

	uvm_lock_fpageq();

	/* Increase use count on segments in range. */
	RB_FOREACH(pmr, uvm_pmemrange_addr, &uvm.pmr_control.addr) {
		if (PMR_IS_SUBRANGE_OF(pmr->low, pmr->high, low, high)) {
			TAILQ_REMOVE(&uvm.pmr_control.use, pmr, pmr_use);
			pmr->use++;
			uvm_pmemrange_use_insert(&uvm.pmr_control.use, pmr);
		}
		uvm_pmr_assertvalid(pmr);
	}

	uvm_unlock_fpageq();
}

/*
 * Allocate a pmemrange.
 *
 * If called from uvm_page_init, the uvm_pageboot_alloc is used.
 * If called after uvm_init, malloc is used.
 * (And if called in between, you're dead.)
 */
struct uvm_pmemrange *
uvm_pmr_allocpmr()
{
	struct uvm_pmemrange *nw;
	int i;

	if (!uvm.page_init_done) {
		nw = (struct uvm_pmemrange *)
		    uvm_pageboot_alloc(sizeof(struct uvm_pmemrange));
		bzero(nw, sizeof(struct uvm_pmemrange));
	} else {
		nw = malloc(sizeof(struct uvm_pmemrange),
		    M_VMMAP, M_NOWAIT | M_ZERO);
	}
	RB_INIT(&nw->addr);
	for (i = 0; i < UVM_PMR_MEMTYPE_MAX; i++) {
		RB_INIT(&nw->size[i]);
		TAILQ_INIT(&nw->single[i]);
	}
	return nw;
}

static const struct uvm_io_ranges uvm_io_ranges[] = UVM_IO_RANGES;

/*
 * Initialization of pmr.
 * Called by uvm_page_init.
 *
 * Sets up pmemranges that maps the vm_physmem data.
 */
void
uvm_pmr_init(void)
{
	struct uvm_pmemrange *new_pmr;
	int i;

	TAILQ_INIT(&uvm.pmr_control.use);
	RB_INIT(&uvm.pmr_control.addr);

	for (i = 0 ; i < vm_nphysseg ; i++) {
		new_pmr = uvm_pmr_allocpmr();

		new_pmr->low = vm_physmem[i].start;
		new_pmr->high = vm_physmem[i].end;

		RB_INSERT(uvm_pmemrange_addr, &uvm.pmr_control.addr, new_pmr);
		uvm_pmemrange_use_insert(&uvm.pmr_control.use, new_pmr);
	}

	for (i = 0; i < nitems(uvm_io_ranges); i++)
		uvm_pmr_use_inc(uvm_io_ranges[i].low, uvm_io_ranges[i].high);
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
	return atop(VM_PAGE_TO_PHYS(r)) + r->fq.free.pages >
	    atop(VM_PAGE_TO_PHYS(pg));
}
#endif /* DEBUG */

/*
 * Allocate any page, the fastest way. No constraints.
 */
int
uvm_pmr_get1page(psize_t count, int memtype, struct pglist *result,
    paddr_t start, paddr_t end)
{
	struct	uvm_pmemrange *pmr;
	struct	vm_page *found;
	psize_t	fcount;

	fcount = 0;
	pmr = TAILQ_FIRST(&uvm.pmr_control.use);
	while (pmr != NULL && fcount != count) {
		/* Outside requested range. */
		if (!(start == 0 && end == 0) &&
		    !PMR_IS_SUBRANGE_OF(pmr->low, pmr->high, start, end)) {
			pmr = TAILQ_NEXT(pmr, pmr_use);
			continue;
		}

		found = TAILQ_FIRST(&pmr->single[memtype]);
		if (found == NULL) {
			found = RB_ROOT(&pmr->size[memtype]);
			/* Size tree gives pg[1] instead of pg[0] */
			if (found != NULL)
				found--;
		}
		if (found == NULL) {
			pmr = TAILQ_NEXT(pmr, pmr_use);
			continue;
		}

		uvm_pmr_assertvalid(pmr);
		uvm_pmr_remove_size(pmr, found);
		while (found->fq.free.pages > 0 && fcount < count) {
			found->fq.free.pages--;
			fcount++;
			TAILQ_INSERT_HEAD(result,
			    &found[found->fq.free.pages], pageq);
		}
		if (found->fq.free.pages > 0) {
			uvm_pmr_insert_size(pmr, found);
			KASSERT(fcount == count);
			uvm_pmr_assertvalid(pmr);
			return fcount;
		} else
			uvm_pmr_remove_addr(pmr, found);
		uvm_pmr_assertvalid(pmr);
	}

	/* Ran out of ranges before enough pages were gathered. */
	return fcount;
}
