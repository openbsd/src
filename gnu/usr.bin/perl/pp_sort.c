/*    pp_sort.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *   ...they shuffled back towards the rear of the line. 'No, not at the
 *   rear!'  the slave-driver shouted. 'Three files up. And stay there...
 */

#include "EXTERN.h"
#define PERL_IN_PP_SORT_C
#include "perl.h"

#if defined(UNDER_CE)
/* looks like 'small' is reserved word for WINCE (or somesuch)*/
#define	small xsmall
#endif

static I32 sortcv(pTHX_ SV *a, SV *b);
static I32 sortcv_stacked(pTHX_ SV *a, SV *b);
static I32 sortcv_xsub(pTHX_ SV *a, SV *b);
static I32 sv_ncmp(pTHX_ SV *a, SV *b);
static I32 sv_i_ncmp(pTHX_ SV *a, SV *b);
static I32 amagic_ncmp(pTHX_ SV *a, SV *b);
static I32 amagic_i_ncmp(pTHX_ SV *a, SV *b);
static I32 amagic_cmp(pTHX_ SV *a, SV *b);
static I32 amagic_cmp_locale(pTHX_ SV *a, SV *b);

#define sv_cmp_static Perl_sv_cmp
#define sv_cmp_locale_static Perl_sv_cmp_locale

#define SORTHINTS(hintsv) \
    (((hintsv) = GvSV(gv_fetchpv("sort::hints", GV_ADDMULTI, SVt_IV))), \
    (SvIOK(hintsv) ? ((I32)SvIV(hintsv)) : 0))

#ifndef SMALLSORT
#define	SMALLSORT (200)
#endif

/*
 * The mergesort implementation is by Peter M. Mcilroy <pmcilroy@lucent.com>.
 *
 * The original code was written in conjunction with BSD Computer Software
 * Research Group at University of California, Berkeley.
 *
 * See also: "Optimistic Merge Sort" (SODA '92)
 *
 * The integration to Perl is by John P. Linderman <jpl@research.att.com>.
 *
 * The code can be distributed under the same terms as Perl itself.
 *
 */


typedef char * aptr;		/* pointer for arithmetic on sizes */
typedef SV * gptr;		/* pointers in our lists */

/* Binary merge internal sort, with a few special mods
** for the special perl environment it now finds itself in.
**
** Things that were once options have been hotwired
** to values suitable for this use.  In particular, we'll always
** initialize looking for natural runs, we'll always produce stable
** output, and we'll always do Peter McIlroy's binary merge.
*/

/* Pointer types for arithmetic and storage and convenience casts */

#define	APTR(P)	((aptr)(P))
#define	GPTP(P)	((gptr *)(P))
#define GPPP(P) ((gptr **)(P))


/* byte offset from pointer P to (larger) pointer Q */
#define	BYTEOFF(P, Q) (APTR(Q) - APTR(P))

#define PSIZE sizeof(gptr)

/* If PSIZE is power of 2, make PSHIFT that power, if that helps */

#ifdef	PSHIFT
#define	PNELEM(P, Q)	(BYTEOFF(P,Q) >> (PSHIFT))
#define	PNBYTE(N)	((N) << (PSHIFT))
#define	PINDEX(P, N)	(GPTP(APTR(P) + PNBYTE(N)))
#else
/* Leave optimization to compiler */
#define	PNELEM(P, Q)	(GPTP(Q) - GPTP(P))
#define	PNBYTE(N)	((N) * (PSIZE))
#define	PINDEX(P, N)	(GPTP(P) + (N))
#endif

/* Pointer into other corresponding to pointer into this */
#define	POTHER(P, THIS, OTHER) GPTP(APTR(OTHER) + BYTEOFF(THIS,P))

#define FROMTOUPTO(src, dst, lim) do *dst++ = *src++; while(src<lim)


/* Runs are identified by a pointer in the auxilliary list.
** The pointer is at the start of the list,
** and it points to the start of the next list.
** NEXT is used as an lvalue, too.
*/

#define	NEXT(P)		(*GPPP(P))


/* PTHRESH is the minimum number of pairs with the same sense to justify
** checking for a run and extending it.  Note that PTHRESH counts PAIRS,
** not just elements, so PTHRESH == 8 means a run of 16.
*/

#define	PTHRESH (8)

/* RTHRESH is the number of elements in a run that must compare low
** to the low element from the opposing run before we justify
** doing a binary rampup instead of single stepping.
** In random input, N in a row low should only happen with
** probability 2^(1-N), so we can risk that we are dealing
** with orderly input without paying much when we aren't.
*/

#define RTHRESH (6)


/*
** Overview of algorithm and variables.
** The array of elements at list1 will be organized into runs of length 2,
** or runs of length >= 2 * PTHRESH.  We only try to form long runs when
** PTHRESH adjacent pairs compare in the same way, suggesting overall order.
**
** Unless otherwise specified, pair pointers address the first of two elements.
**
** b and b+1 are a pair that compare with sense ``sense''.
** b is the ``bottom'' of adjacent pairs that might form a longer run.
**
** p2 parallels b in the list2 array, where runs are defined by
** a pointer chain.
**
** t represents the ``top'' of the adjacent pairs that might extend
** the run beginning at b.  Usually, t addresses a pair
** that compares with opposite sense from (b,b+1).
** However, it may also address a singleton element at the end of list1,
** or it may be equal to ``last'', the first element beyond list1.
**
** r addresses the Nth pair following b.  If this would be beyond t,
** we back it off to t.  Only when r is less than t do we consider the
** run long enough to consider checking.
**
** q addresses a pair such that the pairs at b through q already form a run.
** Often, q will equal b, indicating we only are sure of the pair itself.
** However, a search on the previous cycle may have revealed a longer run,
** so q may be greater than b.
**
** p is used to work back from a candidate r, trying to reach q,
** which would mean b through r would be a run.  If we discover such a run,
** we start q at r and try to push it further towards t.
** If b through r is NOT a run, we detect the wrong order at (p-1,p).
** In any event, after the check (if any), we have two main cases.
**
** 1) Short run.  b <= q < p <= r <= t.
**	b through q is a run (perhaps trivial)
**	q through p are uninteresting pairs
**	p through r is a run
**
** 2) Long run.  b < r <= q < t.
**	b through q is a run (of length >= 2 * PTHRESH)
**
** Note that degenerate cases are not only possible, but likely.
** For example, if the pair following b compares with opposite sense,
** then b == q < p == r == t.
*/


static IV
dynprep(pTHX_ gptr *list1, gptr *list2, size_t nmemb, SVCOMPARE_t cmp)
{
    I32 sense;
    register gptr *b, *p, *q, *t, *p2;
    register gptr c, *last, *r;
    gptr *savep;
    IV runs = 0;

    b = list1;
    last = PINDEX(b, nmemb);
    sense = (cmp(aTHX_ *b, *(b+1)) > 0);
    for (p2 = list2; b < last; ) {
	/* We just started, or just reversed sense.
	** Set t at end of pairs with the prevailing sense.
	*/
	for (p = b+2, t = p; ++p < last; t = ++p) {
	    if ((cmp(aTHX_ *t, *p) > 0) != sense) break;
	}
	q = b;
	/* Having laid out the playing field, look for long runs */
	do {
	    p = r = b + (2 * PTHRESH);
	    if (r >= t) p = r = t;	/* too short to care about */
	    else {
		while (((cmp(aTHX_ *(p-1), *p) > 0) == sense) &&
		       ((p -= 2) > q));
		if (p <= q) {
		    /* b through r is a (long) run.
		    ** Extend it as far as possible.
		    */
		    p = q = r;
		    while (((p += 2) < t) &&
			   ((cmp(aTHX_ *(p-1), *p) > 0) == sense)) q = p;
		    r = p = q + 2;	/* no simple pairs, no after-run */
		}
	    }
	    if (q > b) {		/* run of greater than 2 at b */
		savep = p;
		p = q += 2;
		/* pick up singleton, if possible */
		if ((p == t) &&
		    ((t + 1) == last) &&
		    ((cmp(aTHX_ *(p-1), *p) > 0) == sense))
		    savep = r = p = q = last;
		p2 = NEXT(p2) = p2 + (p - b); ++runs;
		if (sense) while (b < --p) {
		    c = *b;
		    *b++ = *p;
		    *p = c;
		}
		p = savep;
	    }
	    while (q < p) {		/* simple pairs */
		p2 = NEXT(p2) = p2 + 2; ++runs;
		if (sense) {
		    c = *q++;
		    *(q-1) = *q;
		    *q++ = c;
		} else q += 2;
	    }
	    if (((b = p) == t) && ((t+1) == last)) {
		NEXT(p2) = p2 + 1; ++runs;
		b++;
	    }
	    q = r;
	} while (b < t);
	sense = !sense;
    }
    return runs;
}


/* The original merge sort, in use since 5.7, was as fast as, or faster than,
 * qsort on many platforms, but slower than qsort, conspicuously so,
 * on others.  The most likely explanation was platform-specific
 * differences in cache sizes and relative speeds.
 *
 * The quicksort divide-and-conquer algorithm guarantees that, as the
 * problem is subdivided into smaller and smaller parts, the parts
 * fit into smaller (and faster) caches.  So it doesn't matter how
 * many levels of cache exist, quicksort will "find" them, and,
 * as long as smaller is faster, take advanatge of them.
 *
 * By contrast, consider how the original mergesort algorithm worked.
 * Suppose we have five runs (each typically of length 2 after dynprep).
 * 
 * pass               base                        aux
 *  0              1 2 3 4 5
 *  1                                           12 34 5
 *  2                1234 5
 *  3                                            12345
 *  4                 12345
 *
 * Adjacent pairs are merged in "grand sweeps" through the input.
 * This means, on pass 1, the records in runs 1 and 2 aren't revisited until
 * runs 3 and 4 are merged and the runs from run 5 have been copied.
 * The only cache that matters is one large enough to hold *all* the input.
 * On some platforms, this may be many times slower than smaller caches.
 *
 * The following pseudo-code uses the same basic merge algorithm,
 * but in a divide-and-conquer way.
 *
 * # merge $runs runs at offset $offset of list $list1 into $list2.
 * # all unmerged runs ($runs == 1) originate in list $base.
 * sub mgsort2 {
 *     my ($offset, $runs, $base, $list1, $list2) = @_;
 *
 *     if ($runs == 1) {
 *         if ($list1 is $base) copy run to $list2
 *         return offset of end of list (or copy)
 *     } else {
 *         $off2 = mgsort2($offset, $runs-($runs/2), $base, $list2, $list1)
 *         mgsort2($off2, $runs/2, $base, $list2, $list1)
 *         merge the adjacent runs at $offset of $list1 into $list2
 *         return the offset of the end of the merged runs
 *     }
 * }
 * mgsort2(0, $runs, $base, $aux, $base);
 *
 * For our 5 runs, the tree of calls looks like 
 *
 *           5
 *      3        2
 *   2     1   1   1
 * 1   1
 *
 * 1   2   3   4   5
 *
 * and the corresponding activity looks like
 *
 * copy runs 1 and 2 from base to aux
 * merge runs 1 and 2 from aux to base
 * (run 3 is where it belongs, no copy needed)
 * merge runs 12 and 3 from base to aux
 * (runs 4 and 5 are where they belong, no copy needed)
 * merge runs 4 and 5 from base to aux
 * merge runs 123 and 45 from aux to base
 *
 * Note that we merge runs 1 and 2 immediately after copying them,
 * while they are still likely to be in fast cache.  Similarly,
 * run 3 is merged with run 12 while it still may be lingering in cache.
 * This implementation should therefore enjoy much of the cache-friendly
 * behavior that quicksort does.  In addition, it does less copying
 * than the original mergesort implementation (only runs 1 and 2 are copied)
 * and the "balancing" of merges is better (merged runs comprise more nearly
 * equal numbers of original runs).
 *
 * The actual cache-friendly implementation will use a pseudo-stack
 * to avoid recursion, and will unroll processing of runs of length 2,
 * but it is otherwise similar to the recursive implementation.
 */

typedef struct {
    IV	offset;		/* offset of 1st of 2 runs at this level */
    IV	runs;		/* how many runs must be combined into 1 */
} off_runs;		/* pseudo-stack element */

STATIC void
S_mergesortsv(pTHX_ gptr *base, size_t nmemb, SVCOMPARE_t cmp)
{
    IV i, run, runs, offset;
    I32 sense, level;
    int iwhich;
    register gptr *f1, *f2, *t, *b, *p, *tp2, *l1, *l2, *q;
    gptr *aux, *list1, *list2;
    gptr *p1;
    gptr small[SMALLSORT];
    gptr *which[3];
    off_runs stack[60], *stackp;

    if (nmemb <= 1) return;			/* sorted trivially */
    if (nmemb <= SMALLSORT) aux = small;	/* use stack for aux array */
    else { New(799,aux,nmemb,gptr); }		/* allocate auxilliary array */
    level = 0;
    stackp = stack;
    stackp->runs = dynprep(aTHX_ base, aux, nmemb, cmp);
    stackp->offset = offset = 0;
    which[0] = which[2] = base;
    which[1] = aux;
    for (;;) {
	/* On levels where both runs have be constructed (stackp->runs == 0),
	 * merge them, and note the offset of their end, in case the offset
	 * is needed at the next level up.  Hop up a level, and,
	 * as long as stackp->runs is 0, keep merging.
	 */
	if ((runs = stackp->runs) == 0) {
	    iwhich = level & 1;
	    list1 = which[iwhich];		/* area where runs are now */
	    list2 = which[++iwhich];		/* area for merged runs */
	    do {
		offset = stackp->offset;
		f1 = p1 = list1 + offset;		/* start of first run */
		p = tp2 = list2 + offset;	/* where merged run will go */
		t = NEXT(p);			/* where first run ends */
		f2 = l1 = POTHER(t, list2, list1); /* ... on the other side */
		t = NEXT(t);			/* where second runs ends */
		l2 = POTHER(t, list2, list1);	/* ... on the other side */
		offset = PNELEM(list2, t);
		while (f1 < l1 && f2 < l2) {
		    /* If head 1 is larger than head 2, find ALL the elements
		    ** in list 2 strictly less than head1, write them all,
		    ** then head 1.  Then compare the new heads, and repeat,
		    ** until one or both lists are exhausted.
		    **
		    ** In all comparisons (after establishing
		    ** which head to merge) the item to merge
		    ** (at pointer q) is the first operand of
		    ** the comparison.  When we want to know
		    ** if ``q is strictly less than the other'',
		    ** we can't just do
		    **    cmp(q, other) < 0
		    ** because stability demands that we treat equality
		    ** as high when q comes from l2, and as low when
		    ** q was from l1.  So we ask the question by doing
		    **    cmp(q, other) <= sense
		    ** and make sense == 0 when equality should look low,
		    ** and -1 when equality should look high.
		    */


		    if (cmp(aTHX_ *f1, *f2) <= 0) {
			q = f2; b = f1; t = l1;
			sense = -1;
		    } else {
			q = f1; b = f2; t = l2;
			sense = 0;
		    }


		    /* ramp up
		    **
		    ** Leave t at something strictly
		    ** greater than q (or at the end of the list),
		    ** and b at something strictly less than q.
		    */
		    for (i = 1, run = 0 ;;) {
			if ((p = PINDEX(b, i)) >= t) {
			    /* off the end */
			    if (((p = PINDEX(t, -1)) > b) &&
				(cmp(aTHX_ *q, *p) <= sense))
				 t = p;
			    else b = p;
			    break;
			} else if (cmp(aTHX_ *q, *p) <= sense) {
			    t = p;
			    break;
			} else b = p;
			if (++run >= RTHRESH) i += i;
		    }


		    /* q is known to follow b and must be inserted before t.
		    ** Increment b, so the range of possibilities is [b,t).
		    ** Round binary split down, to favor early appearance.
		    ** Adjust b and t until q belongs just before t.
		    */

		    b++;
		    while (b < t) {
			p = PINDEX(b, (PNELEM(b, t) - 1) / 2);
			if (cmp(aTHX_ *q, *p) <= sense) {
			    t = p;
			} else b = p + 1;
		    }


		    /* Copy all the strictly low elements */

		    if (q == f1) {
			FROMTOUPTO(f2, tp2, t);
			*tp2++ = *f1++;
		    } else {
			FROMTOUPTO(f1, tp2, t);
			*tp2++ = *f2++;
		    }
		}


		/* Run out remaining list */
		if (f1 == l1) {
		       if (f2 < l2) FROMTOUPTO(f2, tp2, l2);
		} else              FROMTOUPTO(f1, tp2, l1);
		p1 = NEXT(p1) = POTHER(tp2, list2, list1);

		if (--level == 0) goto done;
		--stackp;
		t = list1; list1 = list2; list2 = t;	/* swap lists */
	    } while ((runs = stackp->runs) == 0);
	}


	stackp->runs = 0;		/* current run will finish level */
	/* While there are more than 2 runs remaining,
	 * turn them into exactly 2 runs (at the "other" level),
	 * each made up of approximately half the runs.
	 * Stack the second half for later processing,
	 * and set about producing the first half now.
	 */
	while (runs > 2) {
	    ++level;
	    ++stackp;
	    stackp->offset = offset;
	    runs -= stackp->runs = runs / 2;
	}
	/* We must construct a single run from 1 or 2 runs.
	 * All the original runs are in which[0] == base.
	 * The run we construct must end up in which[level&1].
	 */
	iwhich = level & 1;
	if (runs == 1) {
	    /* Constructing a single run from a single run.
	     * If it's where it belongs already, there's nothing to do.
	     * Otherwise, copy it to where it belongs.
	     * A run of 1 is either a singleton at level 0,
	     * or the second half of a split 3.  In neither event
	     * is it necessary to set offset.  It will be set by the merge
	     * that immediately follows.
	     */
	    if (iwhich) {	/* Belongs in aux, currently in base */
		f1 = b = PINDEX(base, offset);	/* where list starts */
		f2 = PINDEX(aux, offset);	/* where list goes */
		t = NEXT(f2);			/* where list will end */
		offset = PNELEM(aux, t);	/* offset thereof */
		t = PINDEX(base, offset);	/* where it currently ends */
		FROMTOUPTO(f1, f2, t);		/* copy */
		NEXT(b) = t;			/* set up parallel pointer */
	    } else if (level == 0) goto done;	/* single run at level 0 */
	} else {
	    /* Constructing a single run from two runs.
	     * The merge code at the top will do that.
	     * We need only make sure the two runs are in the "other" array,
	     * so they'll end up in the correct array after the merge.
	     */
	    ++level;
	    ++stackp;
	    stackp->offset = offset;
	    stackp->runs = 0;	/* take care of both runs, trigger merge */
	    if (!iwhich) {	/* Merged runs belong in aux, copy 1st */
		f1 = b = PINDEX(base, offset);	/* where first run starts */
		f2 = PINDEX(aux, offset);	/* where it will be copied */
		t = NEXT(f2);			/* where first run will end */
		offset = PNELEM(aux, t);	/* offset thereof */
		p = PINDEX(base, offset);	/* end of first run */
		t = NEXT(t);			/* where second run will end */
		t = PINDEX(base, PNELEM(aux, t)); /* where it now ends */
		FROMTOUPTO(f1, f2, t);		/* copy both runs */
		NEXT(b) = p;			/* paralled pointer for 1st */
		NEXT(p) = t;			/* ... and for second */
	    }
	}
    }
done:
    if (aux != small) Safefree(aux);	/* free iff allocated */
    return;
}

/*
 * The quicksort implementation was derived from source code contributed
 * by Tom Horsley.
 *
 * NOTE: this code was derived from Tom Horsley's qsort replacement
 * and should not be confused with the original code.
 */

/* Copyright (C) Tom Horsley, 1997. All rights reserved.

   Permission granted to distribute under the same terms as perl which are
   (briefly):

    This program is free software; you can redistribute it and/or modify
    it under the terms of either:

	a) the GNU General Public License as published by the Free
	Software Foundation; either version 1, or (at your option) any
	later version, or

	b) the "Artistic License" which comes with this Kit.

   Details on the perl license can be found in the perl source code which
   may be located via the www.perl.com web page.

   This is the most wonderfulest possible qsort I can come up with (and
   still be mostly portable) My (limited) tests indicate it consistently
   does about 20% fewer calls to compare than does the qsort in the Visual
   C++ library, other vendors may vary.

   Some of the ideas in here can be found in "Algorithms" by Sedgewick,
   others I invented myself (or more likely re-invented since they seemed
   pretty obvious once I watched the algorithm operate for a while).

   Most of this code was written while watching the Marlins sweep the Giants
   in the 1997 National League Playoffs - no Braves fans allowed to use this
   code (just kidding :-).

   I realize that if I wanted to be true to the perl tradition, the only
   comment in this file would be something like:

   ...they shuffled back towards the rear of the line. 'No, not at the
   rear!'  the slave-driver shouted. 'Three files up. And stay there...

   However, I really needed to violate that tradition just so I could keep
   track of what happens myself, not to mention some poor fool trying to
   understand this years from now :-).
*/

/* ********************************************************** Configuration */

#ifndef QSORT_ORDER_GUESS
#define QSORT_ORDER_GUESS 2	/* Select doubling version of the netBSD trick */
#endif

/* QSORT_MAX_STACK is the largest number of partitions that can be stacked up for
   future processing - a good max upper bound is log base 2 of memory size
   (32 on 32 bit machines, 64 on 64 bit machines, etc). In reality can
   safely be smaller than that since the program is taking up some space and
   most operating systems only let you grab some subset of contiguous
   memory (not to mention that you are normally sorting data larger than
   1 byte element size :-).
*/
#ifndef QSORT_MAX_STACK
#define QSORT_MAX_STACK 32
#endif

/* QSORT_BREAK_EVEN is the size of the largest partition we should insertion sort.
   Anything bigger and we use qsort. If you make this too small, the qsort
   will probably break (or become less efficient), because it doesn't expect
   the middle element of a partition to be the same as the right or left -
   you have been warned).
*/
#ifndef QSORT_BREAK_EVEN
#define QSORT_BREAK_EVEN 6
#endif

/* QSORT_PLAY_SAFE is the size of the largest partition we're willing
   to go quadratic on.  We innoculate larger partitions against
   quadratic behavior by shuffling them before sorting.  This is not
   an absolute guarantee of non-quadratic behavior, but it would take
   staggeringly bad luck to pick extreme elements as the pivot
   from randomized data.
*/
#ifndef QSORT_PLAY_SAFE
#define QSORT_PLAY_SAFE 255
#endif

/* ************************************************************* Data Types */

/* hold left and right index values of a partition waiting to be sorted (the
   partition includes both left and right - right is NOT one past the end or
   anything like that).
*/
struct partition_stack_entry {
   int left;
   int right;
#ifdef QSORT_ORDER_GUESS
   int qsort_break_even;
#endif
};

/* ******************************************************* Shorthand Macros */

/* Note that these macros will be used from inside the qsort function where
   we happen to know that the variable 'elt_size' contains the size of an
   array element and the variable 'temp' points to enough space to hold a
   temp element and the variable 'array' points to the array being sorted
   and 'compare' is the pointer to the compare routine.

   Also note that there are very many highly architecture specific ways
   these might be sped up, but this is simply the most generally portable
   code I could think of.
*/

/* Return < 0 == 0 or > 0 as the value of elt1 is < elt2, == elt2, > elt2
*/
#define qsort_cmp(elt1, elt2) \
   ((*compare)(aTHX_ array[elt1], array[elt2]))

#ifdef QSORT_ORDER_GUESS
#define QSORT_NOTICE_SWAP swapped++;
#else
#define QSORT_NOTICE_SWAP
#endif

/* swaps contents of array elements elt1, elt2.
*/
#define qsort_swap(elt1, elt2) \
   STMT_START { \
      QSORT_NOTICE_SWAP \
      temp = array[elt1]; \
      array[elt1] = array[elt2]; \
      array[elt2] = temp; \
   } STMT_END

/* rotate contents of elt1, elt2, elt3 such that elt1 gets elt2, elt2 gets
   elt3 and elt3 gets elt1.
*/
#define qsort_rotate(elt1, elt2, elt3) \
   STMT_START { \
      QSORT_NOTICE_SWAP \
      temp = array[elt1]; \
      array[elt1] = array[elt2]; \
      array[elt2] = array[elt3]; \
      array[elt3] = temp; \
   } STMT_END

/* ************************************************************ Debug stuff */

#ifdef QSORT_DEBUG

static void
break_here()
{
   return; /* good place to set a breakpoint */
}

#define qsort_assert(t) (void)( (t) || (break_here(), 0) )

static void
doqsort_all_asserts(
   void * array,
   size_t num_elts,
   size_t elt_size,
   int (*compare)(const void * elt1, const void * elt2),
   int pc_left, int pc_right, int u_left, int u_right)
{
   int i;

   qsort_assert(pc_left <= pc_right);
   qsort_assert(u_right < pc_left);
   qsort_assert(pc_right < u_left);
   for (i = u_right + 1; i < pc_left; ++i) {
      qsort_assert(qsort_cmp(i, pc_left) < 0);
   }
   for (i = pc_left; i < pc_right; ++i) {
      qsort_assert(qsort_cmp(i, pc_right) == 0);
   }
   for (i = pc_right + 1; i < u_left; ++i) {
      qsort_assert(qsort_cmp(pc_right, i) < 0);
   }
}

#define qsort_all_asserts(PC_LEFT, PC_RIGHT, U_LEFT, U_RIGHT) \
   doqsort_all_asserts(array, num_elts, elt_size, compare, \
                 PC_LEFT, PC_RIGHT, U_LEFT, U_RIGHT)

#else

#define qsort_assert(t) ((void)0)

#define qsort_all_asserts(PC_LEFT, PC_RIGHT, U_LEFT, U_RIGHT) ((void)0)

#endif

/* ****************************************************************** qsort */

STATIC void /* the standard unstable (u) quicksort (qsort) */
S_qsortsvu(pTHX_ SV ** array, size_t num_elts, SVCOMPARE_t compare)
{
   register SV * temp;

   struct partition_stack_entry partition_stack[QSORT_MAX_STACK];
   int next_stack_entry = 0;

   int part_left;
   int part_right;
#ifdef QSORT_ORDER_GUESS
   int qsort_break_even;
   int swapped;
#endif

   /* Make sure we actually have work to do.
   */
   if (num_elts <= 1) {
      return;
   }

   /* Innoculate large partitions against quadratic behavior */
   if (num_elts > QSORT_PLAY_SAFE) {
      register size_t n, j;
      register SV **q;
      for (n = num_elts, q = array; n > 1; ) {
         j = (size_t)(n-- * Drand01());
         temp = q[j];
         q[j] = q[n];
         q[n] = temp;
      }
   }

   /* Setup the initial partition definition and fall into the sorting loop
   */
   part_left = 0;
   part_right = (int)(num_elts - 1);
#ifdef QSORT_ORDER_GUESS
   qsort_break_even = QSORT_BREAK_EVEN;
#else
#define qsort_break_even QSORT_BREAK_EVEN
#endif
   for ( ; ; ) {
      if ((part_right - part_left) >= qsort_break_even) {
         /* OK, this is gonna get hairy, so lets try to document all the
            concepts and abbreviations and variables and what they keep
            track of:

            pc: pivot chunk - the set of array elements we accumulate in the
                middle of the partition, all equal in value to the original
                pivot element selected. The pc is defined by:

                pc_left - the leftmost array index of the pc
                pc_right - the rightmost array index of the pc

                we start with pc_left == pc_right and only one element
                in the pivot chunk (but it can grow during the scan).

            u:  uncompared elements - the set of elements in the partition
                we have not yet compared to the pivot value. There are two
                uncompared sets during the scan - one to the left of the pc
                and one to the right.

                u_right - the rightmost index of the left side's uncompared set
                u_left - the leftmost index of the right side's uncompared set

                The leftmost index of the left sides's uncompared set
                doesn't need its own variable because it is always defined
                by the leftmost edge of the whole partition (part_left). The
                same goes for the rightmost edge of the right partition
                (part_right).

                We know there are no uncompared elements on the left once we
                get u_right < part_left and no uncompared elements on the
                right once u_left > part_right. When both these conditions
                are met, we have completed the scan of the partition.

                Any elements which are between the pivot chunk and the
                uncompared elements should be less than the pivot value on
                the left side and greater than the pivot value on the right
                side (in fact, the goal of the whole algorithm is to arrange
                for that to be true and make the groups of less-than and
                greater-then elements into new partitions to sort again).

            As you marvel at the complexity of the code and wonder why it
            has to be so confusing. Consider some of the things this level
            of confusion brings:

            Once I do a compare, I squeeze every ounce of juice out of it. I
            never do compare calls I don't have to do, and I certainly never
            do redundant calls.

            I also never swap any elements unless I can prove there is a
            good reason. Many sort algorithms will swap a known value with
            an uncompared value just to get things in the right place (or
            avoid complexity :-), but that uncompared value, once it gets
            compared, may then have to be swapped again. A lot of the
            complexity of this code is due to the fact that it never swaps
            anything except compared values, and it only swaps them when the
            compare shows they are out of position.
         */
         int pc_left, pc_right;
         int u_right, u_left;

         int s;

         pc_left = ((part_left + part_right) / 2);
         pc_right = pc_left;
         u_right = pc_left - 1;
         u_left = pc_right + 1;

         /* Qsort works best when the pivot value is also the median value
            in the partition (unfortunately you can't find the median value
            without first sorting :-), so to give the algorithm a helping
            hand, we pick 3 elements and sort them and use the median value
            of that tiny set as the pivot value.

            Some versions of qsort like to use the left middle and right as
            the 3 elements to sort so they can insure the ends of the
            partition will contain values which will stop the scan in the
            compare loop, but when you have to call an arbitrarily complex
            routine to do a compare, its really better to just keep track of
            array index values to know when you hit the edge of the
            partition and avoid the extra compare. An even better reason to
            avoid using a compare call is the fact that you can drop off the
            edge of the array if someone foolishly provides you with an
            unstable compare function that doesn't always provide consistent
            results.

            So, since it is simpler for us to compare the three adjacent
            elements in the middle of the partition, those are the ones we
            pick here (conveniently pointed at by u_right, pc_left, and
            u_left). The values of the left, center, and right elements
            are refered to as l c and r in the following comments.
         */

#ifdef QSORT_ORDER_GUESS
         swapped = 0;
#endif
         s = qsort_cmp(u_right, pc_left);
         if (s < 0) {
            /* l < c */
            s = qsort_cmp(pc_left, u_left);
            /* if l < c, c < r - already in order - nothing to do */
            if (s == 0) {
               /* l < c, c == r - already in order, pc grows */
               ++pc_right;
               qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
            } else if (s > 0) {
               /* l < c, c > r - need to know more */
               s = qsort_cmp(u_right, u_left);
               if (s < 0) {
                  /* l < c, c > r, l < r - swap c & r to get ordered */
                  qsort_swap(pc_left, u_left);
                  qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
               } else if (s == 0) {
                  /* l < c, c > r, l == r - swap c&r, grow pc */
                  qsort_swap(pc_left, u_left);
                  --pc_left;
                  qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
               } else {
                  /* l < c, c > r, l > r - make lcr into rlc to get ordered */
                  qsort_rotate(pc_left, u_right, u_left);
                  qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
               }
            }
         } else if (s == 0) {
            /* l == c */
            s = qsort_cmp(pc_left, u_left);
            if (s < 0) {
               /* l == c, c < r - already in order, grow pc */
               --pc_left;
               qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
            } else if (s == 0) {
               /* l == c, c == r - already in order, grow pc both ways */
               --pc_left;
               ++pc_right;
               qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
            } else {
               /* l == c, c > r - swap l & r, grow pc */
               qsort_swap(u_right, u_left);
               ++pc_right;
               qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
            }
         } else {
            /* l > c */
            s = qsort_cmp(pc_left, u_left);
            if (s < 0) {
               /* l > c, c < r - need to know more */
               s = qsort_cmp(u_right, u_left);
               if (s < 0) {
                  /* l > c, c < r, l < r - swap l & c to get ordered */
                  qsort_swap(u_right, pc_left);
                  qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
               } else if (s == 0) {
                  /* l > c, c < r, l == r - swap l & c, grow pc */
                  qsort_swap(u_right, pc_left);
                  ++pc_right;
                  qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
               } else {
                  /* l > c, c < r, l > r - rotate lcr into crl to order */
                  qsort_rotate(u_right, pc_left, u_left);
                  qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
               }
            } else if (s == 0) {
               /* l > c, c == r - swap ends, grow pc */
               qsort_swap(u_right, u_left);
               --pc_left;
               qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
            } else {
               /* l > c, c > r - swap ends to get in order */
               qsort_swap(u_right, u_left);
               qsort_all_asserts(pc_left, pc_right, u_left + 1, u_right - 1);
            }
         }
         /* We now know the 3 middle elements have been compared and
            arranged in the desired order, so we can shrink the uncompared
            sets on both sides
         */
         --u_right;
         ++u_left;
         qsort_all_asserts(pc_left, pc_right, u_left, u_right);

         /* The above massive nested if was the simple part :-). We now have
            the middle 3 elements ordered and we need to scan through the
            uncompared sets on either side, swapping elements that are on
            the wrong side or simply shuffling equal elements around to get
            all equal elements into the pivot chunk.
         */

         for ( ; ; ) {
            int still_work_on_left;
            int still_work_on_right;

            /* Scan the uncompared values on the left. If I find a value
               equal to the pivot value, move it over so it is adjacent to
               the pivot chunk and expand the pivot chunk. If I find a value
               less than the pivot value, then just leave it - its already
               on the correct side of the partition. If I find a greater
               value, then stop the scan.
            */
            while ((still_work_on_left = (u_right >= part_left))) {
               s = qsort_cmp(u_right, pc_left);
               if (s < 0) {
                  --u_right;
               } else if (s == 0) {
                  --pc_left;
                  if (pc_left != u_right) {
                     qsort_swap(u_right, pc_left);
                  }
                  --u_right;
               } else {
                  break;
               }
               qsort_assert(u_right < pc_left);
               qsort_assert(pc_left <= pc_right);
               qsort_assert(qsort_cmp(u_right + 1, pc_left) <= 0);
               qsort_assert(qsort_cmp(pc_left, pc_right) == 0);
            }

            /* Do a mirror image scan of uncompared values on the right
            */
            while ((still_work_on_right = (u_left <= part_right))) {
               s = qsort_cmp(pc_right, u_left);
               if (s < 0) {
                  ++u_left;
               } else if (s == 0) {
                  ++pc_right;
                  if (pc_right != u_left) {
                     qsort_swap(pc_right, u_left);
                  }
                  ++u_left;
               } else {
                  break;
               }
               qsort_assert(u_left > pc_right);
               qsort_assert(pc_left <= pc_right);
               qsort_assert(qsort_cmp(pc_right, u_left - 1) <= 0);
               qsort_assert(qsort_cmp(pc_left, pc_right) == 0);
            }

            if (still_work_on_left) {
               /* I know I have a value on the left side which needs to be
                  on the right side, but I need to know more to decide
                  exactly the best thing to do with it.
               */
               if (still_work_on_right) {
                  /* I know I have values on both side which are out of
                     position. This is a big win because I kill two birds
                     with one swap (so to speak). I can advance the
                     uncompared pointers on both sides after swapping both
                     of them into the right place.
                  */
                  qsort_swap(u_right, u_left);
                  --u_right;
                  ++u_left;
                  qsort_all_asserts(pc_left, pc_right, u_left, u_right);
               } else {
                  /* I have an out of position value on the left, but the
                     right is fully scanned, so I "slide" the pivot chunk
                     and any less-than values left one to make room for the
                     greater value over on the right. If the out of position
                     value is immediately adjacent to the pivot chunk (there
                     are no less-than values), I can do that with a swap,
                     otherwise, I have to rotate one of the less than values
                     into the former position of the out of position value
                     and the right end of the pivot chunk into the left end
                     (got all that?).
                  */
                  --pc_left;
                  if (pc_left == u_right) {
                     qsort_swap(u_right, pc_right);
                     qsort_all_asserts(pc_left, pc_right-1, u_left, u_right-1);
                  } else {
                     qsort_rotate(u_right, pc_left, pc_right);
                     qsort_all_asserts(pc_left, pc_right-1, u_left, u_right-1);
                  }
                  --pc_right;
                  --u_right;
               }
            } else if (still_work_on_right) {
               /* Mirror image of complex case above: I have an out of
                  position value on the right, but the left is fully
                  scanned, so I need to shuffle things around to make room
                  for the right value on the left.
               */
               ++pc_right;
               if (pc_right == u_left) {
                  qsort_swap(u_left, pc_left);
                  qsort_all_asserts(pc_left+1, pc_right, u_left+1, u_right);
               } else {
                  qsort_rotate(pc_right, pc_left, u_left);
                  qsort_all_asserts(pc_left+1, pc_right, u_left+1, u_right);
               }
               ++pc_left;
               ++u_left;
            } else {
               /* No more scanning required on either side of partition,
                  break out of loop and figure out next set of partitions
               */
               break;
            }
         }

         /* The elements in the pivot chunk are now in the right place. They
            will never move or be compared again. All I have to do is decide
            what to do with the stuff to the left and right of the pivot
            chunk.

            Notes on the QSORT_ORDER_GUESS ifdef code:

            1. If I just built these partitions without swapping any (or
               very many) elements, there is a chance that the elements are
               already ordered properly (being properly ordered will
               certainly result in no swapping, but the converse can't be
               proved :-).

            2. A (properly written) insertion sort will run faster on
               already ordered data than qsort will.

            3. Perhaps there is some way to make a good guess about
               switching to an insertion sort earlier than partition size 6
               (for instance - we could save the partition size on the stack
               and increase the size each time we find we didn't swap, thus
               switching to insertion sort earlier for partitions with a
               history of not swapping).

            4. Naturally, if I just switch right away, it will make
               artificial benchmarks with pure ascending (or descending)
               data look really good, but is that a good reason in general?
               Hard to say...
         */

#ifdef QSORT_ORDER_GUESS
         if (swapped < 3) {
#if QSORT_ORDER_GUESS == 1
            qsort_break_even = (part_right - part_left) + 1;
#endif
#if QSORT_ORDER_GUESS == 2
            qsort_break_even *= 2;
#endif
#if QSORT_ORDER_GUESS == 3
            int prev_break = qsort_break_even;
            qsort_break_even *= qsort_break_even;
            if (qsort_break_even < prev_break) {
               qsort_break_even = (part_right - part_left) + 1;
            }
#endif
         } else {
            qsort_break_even = QSORT_BREAK_EVEN;
         }
#endif

         if (part_left < pc_left) {
            /* There are elements on the left which need more processing.
               Check the right as well before deciding what to do.
            */
            if (pc_right < part_right) {
               /* We have two partitions to be sorted. Stack the biggest one
                  and process the smallest one on the next iteration. This
                  minimizes the stack height by insuring that any additional
                  stack entries must come from the smallest partition which
                  (because it is smallest) will have the fewest
                  opportunities to generate additional stack entries.
               */
               if ((part_right - pc_right) > (pc_left - part_left)) {
                  /* stack the right partition, process the left */
                  partition_stack[next_stack_entry].left = pc_right + 1;
                  partition_stack[next_stack_entry].right = part_right;
#ifdef QSORT_ORDER_GUESS
                  partition_stack[next_stack_entry].qsort_break_even = qsort_break_even;
#endif
                  part_right = pc_left - 1;
               } else {
                  /* stack the left partition, process the right */
                  partition_stack[next_stack_entry].left = part_left;
                  partition_stack[next_stack_entry].right = pc_left - 1;
#ifdef QSORT_ORDER_GUESS
                  partition_stack[next_stack_entry].qsort_break_even = qsort_break_even;
#endif
                  part_left = pc_right + 1;
               }
               qsort_assert(next_stack_entry < QSORT_MAX_STACK);
               ++next_stack_entry;
            } else {
               /* The elements on the left are the only remaining elements
                  that need sorting, arrange for them to be processed as the
                  next partition.
               */
               part_right = pc_left - 1;
            }
         } else if (pc_right < part_right) {
            /* There is only one chunk on the right to be sorted, make it
               the new partition and loop back around.
            */
            part_left = pc_right + 1;
         } else {
            /* This whole partition wound up in the pivot chunk, so
               we need to get a new partition off the stack.
            */
            if (next_stack_entry == 0) {
               /* the stack is empty - we are done */
               break;
            }
            --next_stack_entry;
            part_left = partition_stack[next_stack_entry].left;
            part_right = partition_stack[next_stack_entry].right;
#ifdef QSORT_ORDER_GUESS
            qsort_break_even = partition_stack[next_stack_entry].qsort_break_even;
#endif
         }
      } else {
         /* This partition is too small to fool with qsort complexity, just
            do an ordinary insertion sort to minimize overhead.
         */
         int i;
         /* Assume 1st element is in right place already, and start checking
            at 2nd element to see where it should be inserted.
         */
         for (i = part_left + 1; i <= part_right; ++i) {
            int j;
            /* Scan (backwards - just in case 'i' is already in right place)
               through the elements already sorted to see if the ith element
               belongs ahead of one of them.
            */
            for (j = i - 1; j >= part_left; --j) {
               if (qsort_cmp(i, j) >= 0) {
                  /* i belongs right after j
                  */
                  break;
               }
            }
            ++j;
            if (j != i) {
               /* Looks like we really need to move some things
               */
	       int k;
	       temp = array[i];
	       for (k = i - 1; k >= j; --k)
		  array[k + 1] = array[k];
               array[j] = temp;
            }
         }

         /* That partition is now sorted, grab the next one, or get out
            of the loop if there aren't any more.
         */

         if (next_stack_entry == 0) {
            /* the stack is empty - we are done */
            break;
         }
         --next_stack_entry;
         part_left = partition_stack[next_stack_entry].left;
         part_right = partition_stack[next_stack_entry].right;
#ifdef QSORT_ORDER_GUESS
         qsort_break_even = partition_stack[next_stack_entry].qsort_break_even;
#endif
      }
   }

   /* Believe it or not, the array is sorted at this point! */
}

/* Stabilize what is, presumably, an otherwise unstable sort method.
 * We do that by allocating (or having on hand) an array of pointers
 * that is the same size as the original array of elements to be sorted.
 * We initialize this parallel array with the addresses of the original
 * array elements.  This indirection can make you crazy.
 * Some pictures can help.  After initializing, we have
 *
 *  indir                  list1
 * +----+                 +----+
 * |    | --------------> |    | ------> first element to be sorted
 * +----+                 +----+
 * |    | --------------> |    | ------> second element to be sorted
 * +----+                 +----+
 * |    | --------------> |    | ------> third element to be sorted
 * +----+                 +----+
 *  ...
 * +----+                 +----+
 * |    | --------------> |    | ------> n-1st element to be sorted
 * +----+                 +----+
 * |    | --------------> |    | ------> n-th element to be sorted
 * +----+                 +----+
 *
 * During the sort phase, we leave the elements of list1 where they are,
 * and sort the pointers in the indirect array in the same order determined
 * by the original comparison routine on the elements pointed to.
 * Because we don't move the elements of list1 around through
 * this phase, we can break ties on elements that compare equal
 * using their address in the list1 array, ensuring stabilty.
 * This leaves us with something looking like
 *
 *  indir                  list1
 * +----+                 +----+
 * |    | --+       +---> |    | ------> first element to be sorted
 * +----+   |       |     +----+
 * |    | --|-------|---> |    | ------> second element to be sorted
 * +----+   |       |     +----+
 * |    | --|-------+ +-> |    | ------> third element to be sorted
 * +----+   |         |   +----+
 *  ...
 * +----+    | |   | |    +----+
 * |    | ---|-+   | +--> |    | ------> n-1st element to be sorted
 * +----+    |     |      +----+
 * |    | ---+     +----> |    | ------> n-th element to be sorted
 * +----+                 +----+
 *
 * where the i-th element of the indirect array points to the element
 * that should be i-th in the sorted array.  After the sort phase,
 * we have to put the elements of list1 into the places
 * dictated by the indirect array.
 */


static I32
cmpindir(pTHX_ gptr a, gptr b)
{
    I32 sense;
    gptr *ap = (gptr *)a;
    gptr *bp = (gptr *)b;

    if ((sense = PL_sort_RealCmp(aTHX_ *ap, *bp)) == 0)
	 sense = (ap > bp) ? 1 : ((ap < bp) ? -1 : 0);
    return sense;
}

STATIC void
S_qsortsv(pTHX_ gptr *list1, size_t nmemb, SVCOMPARE_t cmp)
{
    SV *hintsv;

    if (SORTHINTS(hintsv) & HINT_SORT_STABLE) {
	 register gptr **pp, *q;
	 register size_t n, j, i;
	 gptr *small[SMALLSORT], **indir, tmp;
	 SVCOMPARE_t savecmp;
	 if (nmemb <= 1) return;     /* sorted trivially */

	 /* Small arrays can use the stack, big ones must be allocated */
	 if (nmemb <= SMALLSORT) indir = small;
	 else { New(1799, indir, nmemb, gptr *); }

	 /* Copy pointers to original array elements into indirect array */
	 for (n = nmemb, pp = indir, q = list1; n--; ) *pp++ = q++;

	 savecmp = PL_sort_RealCmp;	/* Save current comparison routine, if any */
	 PL_sort_RealCmp = cmp;	/* Put comparison routine where cmpindir can find it */

	 /* sort, with indirection */
	 S_qsortsvu(aTHX_ (gptr *)indir, nmemb, cmpindir);

	 pp = indir;
	 q = list1;
	 for (n = nmemb; n--; ) {
	      /* Assert A: all elements of q with index > n are already
	       * in place.  This is vacuosly true at the start, and we
	       * put element n where it belongs below (if it wasn't
	       * already where it belonged). Assert B: we only move
	       * elements that aren't where they belong,
	       * so, by A, we never tamper with elements above n.
	       */
	      j = pp[n] - q;		/* This sets j so that q[j] is
					 * at pp[n].  *pp[j] belongs in
					 * q[j], by construction.
					 */
	      if (n != j) {		/* all's well if n == j */
		   tmp = q[j];		/* save what's in q[j] */
		   do {
			q[j] = *pp[j];	/* put *pp[j] where it belongs */
			i = pp[j] - q;	/* the index in q of the element
					 * just moved */
			pp[j] = q + j;	/* this is ok now */
		   } while ((j = i) != n);
		   /* There are only finitely many (nmemb) addresses
		    * in the pp array.
		    * So we must eventually revisit an index we saw before.
		    * Suppose the first revisited index is k != n.
		    * An index is visited because something else belongs there.
		    * If we visit k twice, then two different elements must
		    * belong in the same place, which cannot be.
		    * So j must get back to n, the loop terminates,
		    * and we put the saved element where it belongs.
		    */
		   q[n] = tmp;		/* put what belongs into
					 * the n-th element */
	      }
	 }

	/* free iff allocated */
	 if (indir != small) { Safefree(indir); }
	 /* restore prevailing comparison routine */
	 PL_sort_RealCmp = savecmp;
    } else {
	 S_qsortsvu(aTHX_ list1, nmemb, cmp);
    }
}

/*
=head1 Array Manipulation Functions

=for apidoc sortsv

Sort an array. Here is an example:

    sortsv(AvARRAY(av), av_len(av)+1, Perl_sv_cmp_locale);

See lib/sort.pm for details about controlling the sorting algorithm.

=cut
*/

void
Perl_sortsv(pTHX_ SV **array, size_t nmemb, SVCOMPARE_t cmp)
{
    void (*sortsvp)(pTHX_ SV **array, size_t nmemb, SVCOMPARE_t cmp) =
        S_mergesortsv;
    SV *hintsv;
    I32 hints;

    /*  Sun's Compiler (cc: WorkShop Compilers 4.2 30 Oct 1996 C 4.2) used 
	to miscompile this function under optimization -O.  If you get test 
	errors related to picking the correct sort() function, try recompiling 
	this file without optimiziation.  -- A.D.  4/2002.
    */
    hints = SORTHINTS(hintsv);
    if (hints & HINT_SORT_QUICKSORT) {
	sortsvp = S_qsortsv;
    }
    else {
	/* The default as of 5.8.0 is mergesort */
	sortsvp = S_mergesortsv;
    }

    sortsvp(aTHX_ array, nmemb, cmp);
}

PP(pp_sort)
{
    dSP; dMARK; dORIGMARK;
    register SV **up;
    SV **myorigmark = ORIGMARK;
    register I32 max;
    HV *stash;
    GV *gv;
    CV *cv = 0;
    I32 gimme = GIMME;
    OP* nextop = PL_op->op_next;
    I32 overloading = 0;
    bool hasargs = FALSE;
    I32 is_xsub = 0;

    if (gimme != G_ARRAY) {
	SP = MARK;
	RETPUSHUNDEF;
    }

    ENTER;
    SAVEVPTR(PL_sortcop);
    if (PL_op->op_flags & OPf_STACKED) {
	if (PL_op->op_flags & OPf_SPECIAL) {
	    OP *kid = cLISTOP->op_first->op_sibling;	/* pass pushmark */
	    kid = kUNOP->op_first;			/* pass rv2gv */
	    kid = kUNOP->op_first;			/* pass leave */
	    PL_sortcop = kid->op_next;
	    stash = CopSTASH(PL_curcop);
	}
	else {
	    cv = sv_2cv(*++MARK, &stash, &gv, 0);
	    if (cv && SvPOK(cv)) {
		STRLEN n_a;
		char *proto = SvPV((SV*)cv, n_a);
		if (proto && strEQ(proto, "$$")) {
		    hasargs = TRUE;
		}
	    }
	    if (!(cv && CvROOT(cv))) {
		if (cv && CvXSUB(cv)) {
		    is_xsub = 1;
		}
		else if (gv) {
		    SV *tmpstr = sv_newmortal();
		    gv_efullname3(tmpstr, gv, Nullch);
		    DIE(aTHX_ "Undefined sort subroutine \"%"SVf"\" called",
			tmpstr);
		}
		else {
		    DIE(aTHX_ "Undefined subroutine in sort");
		}
	    }

	    if (is_xsub)
		PL_sortcop = (OP*)cv;
	    else {
		PL_sortcop = CvSTART(cv);
		SAVEVPTR(CvROOT(cv)->op_ppaddr);
		CvROOT(cv)->op_ppaddr = PL_ppaddr[OP_NULL];

		PAD_SET_CUR(CvPADLIST(cv), 1);
            }
	}
    }
    else {
	PL_sortcop = Nullop;
	stash = CopSTASH(PL_curcop);
    }

    up = myorigmark + 1;
    while (MARK < SP) {	/* This may or may not shift down one here. */
	/*SUPPRESS 560*/
	if ((*up = *++MARK)) {			/* Weed out nulls. */
	    SvTEMP_off(*up);
	    if (!PL_sortcop && !SvPOK(*up)) {
		STRLEN n_a;
	        if (SvAMAGIC(*up))
	            overloading = 1;
	        else
		    (void)sv_2pv(*up, &n_a);
	    }
	    up++;
	}
    }
    max = --up - myorigmark;
    if (PL_sortcop) {
	if (max > 1) {
	    PERL_CONTEXT *cx;
	    SV** newsp;
	    bool oldcatch = CATCH_GET;

	    SAVETMPS;
	    SAVEOP();

	    CATCH_SET(TRUE);
	    PUSHSTACKi(PERLSI_SORT);
	    if (!hasargs && !is_xsub) {
		if (PL_sortstash != stash || !PL_firstgv || !PL_secondgv) {
		    SAVESPTR(PL_firstgv);
		    SAVESPTR(PL_secondgv);
		    PL_firstgv = gv_fetchpv("a", TRUE, SVt_PV);
		    PL_secondgv = gv_fetchpv("b", TRUE, SVt_PV);
		    PL_sortstash = stash;
		}
#ifdef USE_5005THREADS
		sv_lock((SV *)PL_firstgv);
		sv_lock((SV *)PL_secondgv);
#endif
		SAVESPTR(GvSV(PL_firstgv));
		SAVESPTR(GvSV(PL_secondgv));
	    }

	    PUSHBLOCK(cx, CXt_NULL, PL_stack_base);
	    if (!(PL_op->op_flags & OPf_SPECIAL)) {
		cx->cx_type = CXt_SUB;
		cx->blk_gimme = G_SCALAR;
		PUSHSUB(cx);
		if (!CvDEPTH(cv))
		    (void)SvREFCNT_inc(cv); /* in preparation for POPSUB */
	    }
	    PL_sortcxix = cxstack_ix;

	    if (hasargs && !is_xsub) {
		/* This is mostly copied from pp_entersub */
		AV *av = (AV*)PAD_SVl(0);

#ifndef USE_5005THREADS
		cx->blk_sub.savearray = GvAV(PL_defgv);
		GvAV(PL_defgv) = (AV*)SvREFCNT_inc(av);
#endif /* USE_5005THREADS */
		CX_CURPAD_SAVE(cx->blk_sub);
		cx->blk_sub.argarray = av;
	    }
           sortsv((myorigmark+1), max,
                  is_xsub ? sortcv_xsub : hasargs ? sortcv_stacked : sortcv);

	    POPBLOCK(cx,PL_curpm);
	    PL_stack_sp = newsp;
	    POPSTACK;
	    CATCH_SET(oldcatch);
	}
    }
    else {
	if (max > 1) {
	    MEXTEND(SP, 20);	/* Can't afford stack realloc on signal. */
	    sortsv(ORIGMARK+1, max,
                  (PL_op->op_private & OPpSORT_NUMERIC)
			? ( (PL_op->op_private & OPpSORT_INTEGER)
			    ? ( overloading ? amagic_i_ncmp : sv_i_ncmp)
			    : ( overloading ? amagic_ncmp : sv_ncmp))
			: ( IN_LOCALE_RUNTIME
			    ? ( overloading
				? amagic_cmp_locale
				: sv_cmp_locale_static)
			    : ( overloading ? amagic_cmp : sv_cmp_static)));
	    if (PL_op->op_private & OPpSORT_REVERSE) {
		SV **p = ORIGMARK+1;
		SV **q = ORIGMARK+max;
		while (p < q) {
		    SV *tmp = *p;
		    *p++ = *q;
		    *q-- = tmp;
		}
	    }
	}
    }
    LEAVE;
    PL_stack_sp = ORIGMARK + max;
    return nextop;
}

static I32
sortcv(pTHX_ SV *a, SV *b)
{
    I32 oldsaveix = PL_savestack_ix;
    I32 oldscopeix = PL_scopestack_ix;
    I32 result;
    GvSV(PL_firstgv) = a;
    GvSV(PL_secondgv) = b;
    PL_stack_sp = PL_stack_base;
    PL_op = PL_sortcop;
    CALLRUNOPS(aTHX);
    if (PL_stack_sp != PL_stack_base + 1)
	Perl_croak(aTHX_ "Sort subroutine didn't return single value");
    if (!SvNIOKp(*PL_stack_sp))
	Perl_croak(aTHX_ "Sort subroutine didn't return a numeric value");
    result = SvIV(*PL_stack_sp);
    while (PL_scopestack_ix > oldscopeix) {
	LEAVE;
    }
    leave_scope(oldsaveix);
    return result;
}

static I32
sortcv_stacked(pTHX_ SV *a, SV *b)
{
    I32 oldsaveix = PL_savestack_ix;
    I32 oldscopeix = PL_scopestack_ix;
    I32 result;
    AV *av;

#ifdef USE_5005THREADS
    av = (AV*)PAD_SVl(0);
#else
    av = GvAV(PL_defgv);
#endif

    if (AvMAX(av) < 1) {
	SV** ary = AvALLOC(av);
	if (AvARRAY(av) != ary) {
	    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
	    SvPVX(av) = (char*)ary;
	}
	if (AvMAX(av) < 1) {
	    AvMAX(av) = 1;
	    Renew(ary,2,SV*);
	    SvPVX(av) = (char*)ary;
	}
    }
    AvFILLp(av) = 1;

    AvARRAY(av)[0] = a;
    AvARRAY(av)[1] = b;
    PL_stack_sp = PL_stack_base;
    PL_op = PL_sortcop;
    CALLRUNOPS(aTHX);
    if (PL_stack_sp != PL_stack_base + 1)
	Perl_croak(aTHX_ "Sort subroutine didn't return single value");
    if (!SvNIOKp(*PL_stack_sp))
	Perl_croak(aTHX_ "Sort subroutine didn't return a numeric value");
    result = SvIV(*PL_stack_sp);
    while (PL_scopestack_ix > oldscopeix) {
	LEAVE;
    }
    leave_scope(oldsaveix);
    return result;
}

static I32
sortcv_xsub(pTHX_ SV *a, SV *b)
{
    dSP;
    I32 oldsaveix = PL_savestack_ix;
    I32 oldscopeix = PL_scopestack_ix;
    I32 result;
    CV *cv=(CV*)PL_sortcop;

    SP = PL_stack_base;
    PUSHMARK(SP);
    EXTEND(SP, 2);
    *++SP = a;
    *++SP = b;
    PUTBACK;
    (void)(*CvXSUB(cv))(aTHX_ cv);
    if (PL_stack_sp != PL_stack_base + 1)
	Perl_croak(aTHX_ "Sort subroutine didn't return single value");
    if (!SvNIOKp(*PL_stack_sp))
	Perl_croak(aTHX_ "Sort subroutine didn't return a numeric value");
    result = SvIV(*PL_stack_sp);
    while (PL_scopestack_ix > oldscopeix) {
	LEAVE;
    }
    leave_scope(oldsaveix);
    return result;
}


static I32
sv_ncmp(pTHX_ SV *a, SV *b)
{
    NV nv1 = SvNV(a);
    NV nv2 = SvNV(b);
    return nv1 < nv2 ? -1 : nv1 > nv2 ? 1 : 0;
}

static I32
sv_i_ncmp(pTHX_ SV *a, SV *b)
{
    IV iv1 = SvIV(a);
    IV iv2 = SvIV(b);
    return iv1 < iv2 ? -1 : iv1 > iv2 ? 1 : 0;
}
#define tryCALL_AMAGICbin(left,right,meth,svp) STMT_START { \
	  *svp = Nullsv;				\
          if (PL_amagic_generation) { \
	    if (SvAMAGIC(left)||SvAMAGIC(right))\
		*svp = amagic_call(left, \
				   right, \
				   CAT2(meth,_amg), \
				   0); \
	  } \
	} STMT_END

static I32
amagic_ncmp(pTHX_ register SV *a, register SV *b)
{
    SV *tmpsv;
    tryCALL_AMAGICbin(a,b,ncmp,&tmpsv);
    if (tmpsv) {
    	NV d;
 
        if (SvIOK(tmpsv)) {
            I32 i = SvIVX(tmpsv);
            if (i > 0)
               return 1;
            return i? -1 : 0;
        }
        d = SvNV(tmpsv);
        if (d > 0)
           return 1;
        return d? -1 : 0;
     }
     return sv_ncmp(aTHX_ a, b);
}

static I32
amagic_i_ncmp(pTHX_ register SV *a, register SV *b)
{
    SV *tmpsv;
    tryCALL_AMAGICbin(a,b,ncmp,&tmpsv);
    if (tmpsv) {
    	NV d;

        if (SvIOK(tmpsv)) {
            I32 i = SvIVX(tmpsv);
            if (i > 0)
               return 1;
            return i? -1 : 0;
        }
        d = SvNV(tmpsv);
        if (d > 0)
           return 1;
        return d? -1 : 0;
    }
    return sv_i_ncmp(aTHX_ a, b);
}

static I32
amagic_cmp(pTHX_ register SV *str1, register SV *str2)
{
    SV *tmpsv;
    tryCALL_AMAGICbin(str1,str2,scmp,&tmpsv);
    if (tmpsv) {
    	NV d;
 
        if (SvIOK(tmpsv)) {
            I32 i = SvIVX(tmpsv);
            if (i > 0)
               return 1;
            return i? -1 : 0;
        }
        d = SvNV(tmpsv);
        if (d > 0)
           return 1;
        return d? -1 : 0;
    }
    return sv_cmp(str1, str2);
}

static I32
amagic_cmp_locale(pTHX_ register SV *str1, register SV *str2)
{
    SV *tmpsv;
    tryCALL_AMAGICbin(str1,str2,scmp,&tmpsv);
    if (tmpsv) {
    	NV d;
 
        if (SvIOK(tmpsv)) {
            I32 i = SvIVX(tmpsv);
            if (i > 0)
               return 1;
            return i? -1 : 0;
        }
        d = SvNV(tmpsv);
        if (d > 0)
           return 1;
        return d? -1 : 0;
    }
    return sv_cmp_locale(str1, str2);
}
