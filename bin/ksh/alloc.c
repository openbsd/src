/*	$OpenBSD: alloc.c,v 1.1.1.1 1996/08/14 06:19:10 downsj Exp $	*/

/*
 * area-based allocation built on malloc/free
 */

#include "sh.h"
#ifdef MEM_DEBUG
# undef alloc
# undef aresize
# undef afree
#endif /* MEM_DEBUG */

#define	ICELLS	100		/* number of Cells in small Block */

typedef union Cell Cell;
typedef struct Block Block;

/*
 * The Cells in a Block are organized as a set of objects.
 * Each object (pointed to by dp) begins with a size in (dp-1)->size,
 * followed with "size" data Cells.  Free objects are
 * linked together via dp->next.
 */

union Cell {
	size_t	size;
	Cell   *next;
	struct {int _;} junk;	/* alignment */
};

struct Block {
	Block  *next;		/* list of Blocks in Area */
	Cell   *freelist;	/* object free list */
	Cell   *last;		/* &b.cell[size] */
	Cell	cell [1];	/* [size] Cells for allocation */
};

static Block aempty = {&aempty, aempty.cell, aempty.cell};

/* create empty Area */
Area *
ainit(ap)
	register Area *ap;
{
	ap->freelist = &aempty;
	return ap;
}

/* free all object in Area */
void
afreeall(ap)
	register Area *ap;
{
	register Block *bp;
	register Block *tmp;

	bp = ap->freelist;
	if (bp != NULL && bp != &aempty) {
		do {
			tmp = bp->next;
			free((void*)bp);
			bp = tmp;
		} while (bp != ap->freelist);
		ap->freelist = &aempty;
	}
}

/* allocate object from Area */
void *
alloc(size, ap)
	size_t size;
	register Area *ap;
{
	int cells, split;
	register Block *bp;
	register Cell *dp, *fp, *fpp;

	if (size <= 0) {
		aerror(ap, "allocate bad size");
		return NULL;
	}
	cells = (unsigned)(size - 1) / sizeof(Cell) + 1;

	/* find Cell large enough */
	for (bp = ap->freelist; ; bp = bp->next) {
		for (fpp = NULL, fp = bp->freelist;
		     fp != bp->last; fpp = fp, fp = fpp->next)
			if ((fp-1)->size >= cells)
				goto Found;

		/* wrapped around Block list, create new Block */
		if (bp->next == ap->freelist) {
			bp = (Block*) malloc(offsetof(Block, cell[ICELLS])
					     + sizeof(bp->cell[0]) * cells);
			if (bp == NULL) {
				aerror(ap, "cannot allocate");
				return NULL;
			}
			if (ap->freelist == &aempty)
				bp->next = bp;
			else {
				bp->next = ap->freelist->next;
				ap->freelist->next = bp;
			}
			bp->last = bp->cell + ICELLS + cells;
			fp = bp->freelist = bp->cell + 1; /* initial free list */
			(fp-1)->size = ICELLS + cells - 1;
			fp->next = bp->last;
			fpp = NULL;
			break;
		}
	}
  Found:
	ap->freelist = bp;
	dp = fp;		/* allocated object */
	split = (dp-1)->size - cells;
	if (split < 0)
		aerror(ap, "allocated object too small");
	if (--split <= 0) {	/* allocate all */
		fp = fp->next;
	} else {		/* allocate head, free tail */
		(fp-1)->size = cells;
		fp += cells + 1;
		(fp-1)->size = split;
		fp->next = dp->next;
	}
	if (fpp == NULL)
		bp->freelist = fp;
	else
		fpp->next = fp;
	return (void*) dp;
}

/* change size of object -- like realloc */
void *
aresize(ptr, size, ap)
	register void *ptr;
	size_t size;
	Area *ap;
{
	int cells;
	register Cell *dp = (Cell*) ptr;

	if (size <= 0) {
		aerror(ap, "allocate bad size");
		return NULL;
	}
	cells = (unsigned)(size - 1) / sizeof(Cell) + 1;

	if (dp == NULL || (dp-1)->size < cells) { /* enlarge object */
		/* XXX check for available adjacent free block */
		ptr = alloc(size, ap);
		if (dp != NULL) {
			memcpy(ptr, dp, (dp-1)->size * sizeof(Cell));
			afree((void *) dp, ap);
		}
	} else {		/* shrink object */
		int split;

		split = (dp-1)->size - cells;
		if (--split <= 0) /* cannot split */
			;
		else {		/* shrink head, free tail */
			(dp-1)->size = cells;
			dp += cells + 1;
			(dp-1)->size = split;
			afree((void*)dp, ap);
		}
	}
	return (void*) ptr;
}

void
afree(ptr, ap)
	void *ptr;
	register Area *ap;
{
	register Block *bp;
	register Cell *fp, *fpp;
	register Cell *dp = (Cell*)ptr;

	/* find Block containing Cell */
	for (bp = ap->freelist; ; bp = bp->next) {
		if (bp->cell <= dp && dp < bp->last)
			break;
		if (bp->next == ap->freelist) {
			aerror(ap, "freeing with invalid area");
			return;
		}
	}

	/* find position in free list */
	for (fpp = NULL, fp = bp->freelist; fp < dp; fpp = fp, fp = fpp->next)
		;

	if (fp == dp) {
		aerror(ap, "freeing free object");
		return;
	}

	/* join object with next */
	if (dp + (dp-1)->size == fp-1) { /* adjacent */
		(dp-1)->size += (fp-1)->size + 1;
		dp->next = fp->next;
	} else			/* non-adjacent */
		dp->next = fp;

	/* join previous with object */
	if (fpp == NULL)
		bp->freelist = dp;
	else if (fpp + (fpp-1)->size == dp-1) { /* adjacent */
		(fpp-1)->size += (dp-1)->size + 1;
		fpp->next = dp->next;
	} else			/* non-adjacent */
		fpp->next = dp;
}

#if DEBUG_ALLOC
void
aprint(ap, ptr, size)
	register Area *ap;
	void *ptr;
	size_t size;
{
	Block *bp;

	if (!ap)
		shellf("aprint: null area pointer\n");
	else if (!(bp = ap->freelist))
		shellf("aprint: null area freelist\n");
	else if (bp == &aempty)
		shellf("aprint: area is empty\n");
	else {
		int i;
		Cell *fp;

		for (i = 0; !i || bp != ap->freelist; bp = bp->next, i++) {
			if (ptr) {
				void *eptr = (void *) (((char *) ptr) + size);
				/* print block only if it overlaps ptr/size */
				if (!((ptr >= (void *) bp
				       && ptr <= (void *) bp->last)
				      || (eptr >= (void *) bp
				         && eptr <= (void *) bp->last)))
					continue;
				shellf("aprint: overlap of 0x%p .. 0x%p\n",
					ptr, eptr);
			}
			shellf("aprint: block %2d: 0x%p .. 0x%p (%d)\n", i,
				bp->cell, bp->last,
				(char *) bp->last - (char *) bp->cell);
			for (fp = bp->freelist; fp != bp->last; fp = fp->next)
				shellf(
				    "aprint:   0x%p .. 0x%p (%d) free\n",
					(fp-1), (fp-1) + (fp-1)->size,
					(fp-1)->size * sizeof(Cell));
		}
	}
}
#endif /* DEBUG_ALLOC */


#ifdef TEST_ALLOC

Area a;

main(int argc, char **argv) {
	int i;
	char *p [9];

	ainit(&a);
	for (i = 0; i < 9; i++) {
		p[i] = alloc(124, &a);
		printf("alloc: %x\n", p[i]);
	}
	for (i = 1; i < argc; i++)
		afree(p[atoi(argv[i])], &a);
	afreeall(&a);
	return 0;
}

void aerror(Area *ap, const char *msg) {
	abort();
}

#endif

