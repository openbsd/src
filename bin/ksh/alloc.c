/*	$OpenBSD: alloc.c,v 1.3 1998/06/29 20:06:19 deraadt Exp $	*/

/*
 * area-based allocation built on malloc/free
 */

#include "sh.h"

#ifdef TEST_ALLOC
# define shellf	printf
# ifndef DEBUG_ALLOC
#  define DEBUG_ALLOC
# endif /* DEBUG_ALLOC */
#endif /* TEST_ALLOC */

#ifdef MEM_DEBUG

/*
 * Special versions of alloc routines if doing mem_debug
 */
Area *
_chmem_ainit(ap, file, line)
	Area *ap;
	const char *file;
	int line;
{
	ap->freelist = (struct Block *) _chmem_newpool("ainit", (char *) 0, -1,
						file, line);
	if (!ap->freelist)
	    aerror(ap, "ainit failed (ie, newpool)");
	return ap;
}

/* free all object in Area */
void
_chmem_afreeall(ap, file, line)
	Area *ap;
	const char *file;
	int line;
{
	_chmem_delpool((Chmem_poolp) ap->freelist, 0, file, line);
	/* Kind of ugly, but it works */
	_chmem_ainit(ap, file, line);
}

/* allocate object from Area */
void *
_chmem_alloc(size, ap, file, line)
	size_t size;
	Area *ap;
	const char *file;
	int line;
{
	return _chmem_mallocp((Chmem_poolp) ap->freelist, size, file, line);
}

/* change size of object -- like realloc */
void *
_chmem_aresize(ptr, size, ap, file, line)
	void *ptr;
	size_t size;
	Area *ap;
	const char *file;
	int line;
{
	if (!ptr)
		/* Done as realloc(0, size) is not portable */
		return _chmem_mallocp((Chmem_poolp) ap->freelist, size,
					file, line);
	else
		return _chmem_reallocp((Chmem_poolp) ap->freelist, ptr, size,
					file, line);
}

void
_chmem_afree(ptr, ap, file, line)
	void *ptr;
	Area *ap;
	const char *file;
	int line;
{
	return _chmem_freep((Chmem_poolp) ap->freelist, ptr, file, line);
}

#else /* MEM_DEBUG */

# if DEBUG_ALLOC
void acheck ARGS((Area *ap));
#  define ACHECK(ap)	acheck(ap)
# else /* DEBUG_ALLOC */
#  define ACHECK(ap)
# endif /* DEBUG_ALLOC */

#define	ICELLS	200		/* number of Cells in small Block */

typedef union Cell Cell;
typedef struct Block Block;

/*
 * The Cells in a Block are organized as a set of objects.
 * Each object (pointed to by dp) begins with the block it is in
 * (dp-2)->block, then has a size in (dp-1)->size, which is
 * followed with "size" data Cells.  Free objects are
 * linked together via dp->next.
 */

#define NOBJECT_FIELDS	2	/* the block and size `fields' */

union Cell {
	size_t	size;
	Cell   *next;
	Block  *block;
	struct {int _;} junk;	/* alignment */
	double djunk;		/* alignment */
};

struct Block {
	Block  *next;		/* list of Blocks in Area */
	Block  *prev;		/* previous block in list */
	Cell   *freelist;	/* object free list */
	Cell   *last;		/* &b.cell[size] */
	Cell	cell [1];	/* [size] Cells for allocation */
};

static Block aempty = {&aempty, &aempty, aempty.cell, aempty.cell};

static void ablockfree ARGS((Block *bp, Area *ap));
static void *asplit ARGS((Area *ap, Block *bp, Cell *fp, Cell *fpp, int cells));

/* create empty Area */
Area *
ainit(ap)
	register Area *ap;
{
	ap->freelist = &aempty;
	ACHECK(ap);
	return ap;
}

/* free all object in Area */
void
afreeall(ap)
	register Area *ap;
{
	register Block *bp;
	register Block *tmp;

	ACHECK(ap);
	bp = ap->freelist;
	if (bp != NULL && bp != &aempty) {
		do {
			tmp = bp;
			bp = bp->next;
			free((void*)tmp);
		} while (bp != ap->freelist);
		ap->freelist = &aempty;
	}
	ACHECK(ap);
}

/* allocate object from Area */
void *
alloc(size, ap)
	size_t size;
	register Area *ap;
{
	int cells, acells;
	Block *bp = 0;
	Cell *fp = 0, *fpp = 0;

	ACHECK(ap);
	if (size <= 0)
		aerror(ap, "allocate bad size");
	cells = (unsigned)(size - 1) / sizeof(Cell) + 1;

	/* allocate at least this many cells */
	acells = cells + NOBJECT_FIELDS;

	/*
	 * Only attempt to track small objects - let malloc deal
	 * with larger objects. (this way we don't have to deal with
	 * coalescing memory, or with releasing it to the system)
	 */
	if (cells <= ICELLS) {
		/* find free Cell large enough */
		for (bp = ap->freelist; ; bp = bp->next) {
			for (fpp = NULL, fp = bp->freelist;
			     fp != bp->last; fpp = fp, fp = fp->next)
			{
				if ((fp-1)->size >= cells)
					goto Found;
			}
			/* wrapped around Block list, create new Block */
			if (bp->next == ap->freelist) {
				bp = 0;
				break;
			}
		}
		/* Not much free space left?  Allocate a big object this time */
		acells += ICELLS;
	}
	if (bp == 0) {
		bp = (Block*) malloc(offsetof(Block, cell[acells]));
		if (bp == NULL)
			aerror(ap, "cannot allocate");
		if (ap->freelist == &aempty) {
			ap->freelist = bp->next = bp->prev = bp;
		} else {
			bp->next = ap->freelist->next;
			ap->freelist->next->prev = bp;
			ap->freelist->next = bp;
			bp->prev = ap->freelist;
		}
		bp->last = bp->cell + acells;
		/* initial free list */
		fp = bp->freelist = bp->cell + NOBJECT_FIELDS;
		(fp-1)->size = acells - NOBJECT_FIELDS;
		(fp-2)->block = bp;
		fp->next = bp->last;
		fpp = NULL;
	}

  Found:
	return asplit(ap, bp, fp, fpp, cells);
}

/* Do the work of splitting an object into allocated and (possibly) unallocated
 * objects.  Returns the `allocated' object.
 */
static void *
asplit(ap, bp, fp, fpp, cells)
	Area *ap;
	Block *bp;
	Cell *fp;
	Cell *fpp;
	int cells;
{
	Cell *dp = fp;	/* allocated object */
	int split = (fp-1)->size - cells;

	ACHECK(ap);
	if (split < 0)
		aerror(ap, "allocated object too small");
	if (split <= NOBJECT_FIELDS) {	/* allocate all */
		fp = fp->next;
	} else {		/* allocate head, free tail */
		Cell *next = fp->next; /* needed, as cells may be 0 */
		ap->freelist = bp; /* next time, start looking for space here */
		(fp-1)->size = cells;
		fp += cells + NOBJECT_FIELDS;
		(fp-1)->size = split - NOBJECT_FIELDS;
		(fp-2)->block = bp;
		fp->next = next;
	}
	if (fpp == NULL)
		bp->freelist = fp;
	else
		fpp->next = fp;
	ACHECK(ap);
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
	Cell *dp = (Cell*) ptr;
	int oldcells = dp ? (dp-1)->size : 0;

	ACHECK(ap);
	if (size <= 0)
		aerror(ap, "allocate bad size");
	/* New size (in cells) */
	cells = (unsigned)(size - 1) / sizeof(Cell) + 1;

	/* Is this a large object?  If so, let malloc deal with it
	 * directly (unless we are crossing the ICELLS border, in
	 * which case the alloc/free below handles it - this should
	 * cut down on fragmentation, and will also keep the code
	 * working (as it assumes size < ICELLS means it is not
	 * a `large object').
	 */
	if (oldcells > ICELLS && cells > ICELLS) {
		Block *bp = (dp-2)->block;
		Block *nbp;
		/* Saved in case realloc fails.. */
		Block *next = bp->next, *prev = bp->prev;

		if (bp->freelist != bp->last)
			aerror(ap, "allocation resizing free pointer");
		nbp = realloc((void *) bp,
			      offsetof(Block, cell[cells + NOBJECT_FIELDS]));
		if (!nbp) {
			/* Have to clean up... */
			/* NOTE: If this code changes, similar changes may be
			 * needed in ablockfree().
			 */
			if (next == bp) /* only block */
				ap->freelist = &aempty;
			else {
				next->prev = prev;
				prev->next = next;
				if (ap->freelist == bp)
					ap->freelist = next;
			}
			aerror(ap, "cannot re-allocate");
		}
		/* If location changed, keep pointers straight... */
		if (nbp != bp) {
			if (next == bp) /* only one block */
				nbp->next = nbp->prev = nbp;
			else {
				next->prev = nbp;
				prev->next = nbp;
			}
			if (ap->freelist == bp)
				ap->freelist = nbp;
			dp = nbp->cell + NOBJECT_FIELDS;
			(dp-2)->block = nbp;
		}
		(dp-1)->size = cells;
		nbp->last = nbp->cell + cells + NOBJECT_FIELDS;
		nbp->freelist = nbp->last;

		ACHECK(ap);
		return (void*) dp;
	}

	/* Check if we can just grow this cell
	 * (need to check that cells < ICELLS so we don't make an
	 * object a `large' - that would mess everything up).
	 */
	if (dp && cells > oldcells && cells <= ICELLS) {
		Cell *fp, *fpp;
		Block *bp = (dp-2)->block;
		int need = cells - oldcells - NOBJECT_FIELDS;

		/* XXX if we had a flag in an object indicating
		 * if the object was free/allocated, we could
		 * avoid this loop (perhaps)
		 */
		for (fpp = NULL, fp = bp->freelist;
		     fp != bp->last
		     && dp + oldcells + NOBJECT_FIELDS <= fp
		     ; fpp = fp, fp = fp->next)
		{
			if (dp + oldcells + NOBJECT_FIELDS == fp
			    && (fp-1)->size >= need)
			{
				Cell *np = asplit(ap, bp, fp, fpp, need);
				/* May get more than we need here */
				(dp-1)->size += (np-1)->size + NOBJECT_FIELDS;
				ACHECK(ap);
				return ptr;
			}
		}
	}

	/* Check if we can just shrink this cell
	 * (if oldcells > ICELLS, this is a large object and we leave
	 * it to malloc...)
	 * Note: this also handles cells == oldcells (a no-op).
	 */
	if (dp && cells <= oldcells && oldcells <= ICELLS) {
		int split;

		split = oldcells - cells;
		if (split <= NOBJECT_FIELDS) /* cannot split */
			;
		else {		/* shrink head, free tail */
			Block *bp = (dp-2)->block;

			(dp-1)->size = cells;
			dp += cells + NOBJECT_FIELDS;
			(dp-1)->size = split - NOBJECT_FIELDS;
			(dp-2)->block = bp;
			afree((void*)dp, ap);
		}
		/* ACHECK() done in afree() */
		return ptr;
	}

	/* Have to do it the hard way... */
	ptr = alloc(size, ap);
	if (dp != NULL) {
		size_t s = (dp-1)->size * sizeof(Cell);
		if (s > size)
			s = size;
		memcpy(ptr, dp, s);
		afree((void *) dp, ap);
	}
	/* ACHECK() done in alloc()/afree() */
	return ptr;
}

void
afree(ptr, ap)
	void *ptr;
	register Area *ap;
{
	register Block *bp;
	register Cell *fp, *fpp;
	register Cell *dp = (Cell*)ptr;

	ACHECK(ap);
	if (ptr == 0)
		aerror(ap, "freeing null pointer");
	bp = (dp-2)->block;

	/* If this is a large object, just free it up... */
	/* Release object... */
	if ((dp-1)->size > ICELLS) {
		ablockfree(bp, ap);
		ACHECK(ap);
		return;
	}

	if (dp < &bp->cell[NOBJECT_FIELDS] || dp >= bp->last)
		aerror(ap, "freeing memory outside of block (corrupted?)");

	/* find position in free list */
	/* XXX if we had prev/next pointers for objects, this loop could go */
	for (fpp = NULL, fp = bp->freelist; fp < dp; fpp = fp, fp = fp->next)
		;

	if (fp == dp)
		aerror(ap, "freeing free object");

	/* join object with next */
	if (dp + (dp-1)->size == fp-NOBJECT_FIELDS) { /* adjacent */
		(dp-1)->size += (fp-1)->size + NOBJECT_FIELDS;
		dp->next = fp->next;
	} else			/* non-adjacent */
		dp->next = fp;

	/* join previous with object */
	if (fpp == NULL)
		bp->freelist = dp;
	else if (fpp + (fpp-1)->size == dp-NOBJECT_FIELDS) { /* adjacent */
		(fpp-1)->size += (dp-1)->size + NOBJECT_FIELDS;
		fpp->next = dp->next;
	} else			/* non-adjacent */
		fpp->next = dp;

	/* If whole block is free (and we have some other blocks
	 * around), release this block back to the system...
	 */
	if (bp->next != bp && bp->freelist == bp->cell + NOBJECT_FIELDS
	    && bp->freelist + (bp->freelist-1)->size == bp->last
	    /* XXX and the other block has some free memory? */
	    )
		ablockfree(bp, ap);
	ACHECK(ap);
}

static void
ablockfree(bp, ap)
	Block *bp;
	Area *ap;
{
	/* NOTE: If this code changes, similar changes may be
	 * needed in alloc() (where realloc fails).
	 */

	if (bp->next == bp) /* only block */
		ap->freelist = &aempty;
	else {
		bp->next->prev = bp->prev;
		bp->prev->next = bp->next;
		if (ap->freelist == bp)
			ap->freelist = bp->next;
	}
	free((void*) bp);
}

# if DEBUG_ALLOC
void
acheck(ap)
	Area *ap;
{
	Block *bp, *bpp;
	Cell *dp, *dptmp, *fp;
	int ok = 1;
	int isfree;
	static int disabled;

	if (disabled)
		return;

	if (!ap) {
		disabled = 1;
		aerror(ap, "acheck: null area pointer");
	}

	bp = ap->freelist;
	if (!bp) {
		disabled = 1;
		aerror(ap, "acheck: null area freelist");
	}

	/* Nothing to check... */
	if (bp == &aempty)
		return;

	bpp = ap->freelist->prev;
	while (1) {
		if (bp->prev != bpp) {
			shellf("acheck: bp->prev != previous\n");
			ok = 0;
		}
		fp = bp->freelist;
		for (dp = &bp->cell[NOBJECT_FIELDS]; dp != bp->last; ) {
			if ((dp-2)->block != bp) {
				shellf("acheck: fragment's block is wrong\n");
				ok = 0;
			}
			isfree = dp == fp;
			if ((dp-1)->size == 0 && isfree) {
				shellf("acheck: 0 size frag\n");
				ok = 0;
			}
			if ((dp-1)->size > ICELLS
			    && !isfree
			    && (dp != &bp->cell[NOBJECT_FIELDS]
				|| dp + (dp-1)->size != bp->last))
			{
				shellf("acheck: big cell doesn't make up whole block\n");
				ok = 0;
			}
			if (isfree) {
				if (dp->next <= dp) {
					shellf("acheck: free fragment's next <= self\n");
					ok = 0;
				}
				if (dp->next > bp->last) {
					shellf("acheck: free fragment's next > last\n");
					ok = 0;
				}
				fp = dp->next;
			}
			dptmp = dp + (dp-1)->size;
			if (dptmp > bp->last) {
				shellf("acheck: next frag out of range\n");
				ok = 0;
				break;
			} else if (dptmp != bp->last) {
				dptmp += NOBJECT_FIELDS;
				if (dptmp > bp->last) {
					shellf("acheck: next frag just out of range\n");
					ok = 0;
					break;
				}
			}
			if (isfree && dptmp == fp && dptmp != bp->last) {
				shellf("acheck: adjacent free frags\n");
				ok = 0;
			} else if (dptmp > fp) {
				shellf("acheck: free frag list messed up\n");
				ok = 0;
			}
			dp = dptmp;
		}
		bpp = bp;
		bp = bp->next;
		if (bp == ap->freelist)
			break;
	}
	if (!ok) {
		disabled = 1;
		aerror(ap, "acheck failed");
	}
}

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
		Cell *dp, *fp;
		Block *bpp;

		bpp = ap->freelist->prev;
		for (i = 0; ; i++) {
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
			if (bp->prev != bpp || bp->next->prev != bp)
				shellf(
	"aprint: BAD prev pointer: bp %p, bp->prev %p, bp->next %p, bpp=%p\n",
					bp, bp->prev, bp->next, bpp);
			shellf("aprint: block %2d (p=%p,%p,n=%p): 0x%p .. 0x%p (%ld)\n", i,
				bp->prev, bp, bp->next,
				bp->cell, bp->last,
				(long) ((char *) bp->last - (char *) bp->cell));
			fp = bp->freelist;
			if (bp->last <= bp->cell + NOBJECT_FIELDS)
				shellf(
			"aprint: BAD bp->last too small: %p <= %p\n",
					bp->last, bp->cell + NOBJECT_FIELDS);
			if (bp->freelist < bp->cell + NOBJECT_FIELDS
			    || bp->freelist > bp->last)
				shellf(
			"aprint: BAD bp->freelist %p out of range: %p .. %p\n",
					bp->freelist,
					bp->cell + NOBJECT_FIELDS, bp->last);
			for (dp = bp->cell; dp != bp->last ; ) {
				dp += NOBJECT_FIELDS;
				shellf(
				    "aprint:   0x%p .. 0x%p (%ld) %s\n",
					(dp-NOBJECT_FIELDS),
					(dp-NOBJECT_FIELDS) + (dp-1)->size
						+ NOBJECT_FIELDS,
					(long) ((dp-1)->size + NOBJECT_FIELDS)
						* sizeof(Cell),
					dp == fp ? "free" : "allocated");
				if ((dp-2)->block != bp)
					shellf(
					"aprint: BAD dp->block %p != bp %p\n",
						(dp-2)->block, bp);
				if (dp > bp->last)
					shellf(
				"aprint: BAD dp gone past block: %p > %p\n",
						dp, bp->last);
				if (dp > fp)
					shellf(
				"aprint: BAD dp gone past free: %p > %p\n",
						dp, fp);
				if (dp == fp) {
					fp = fp->next;
					if (fp < dp || fp > bp->last)
						shellf(
			"aprint: BAD free object %p out of range: %p .. %p\n",
							fp,
							dp, bp->last);
				}
				dp += (dp-1)->size;
			}
			bpp = bp;
			bp = bp->next;
			if (bp == ap->freelist)
				break;
		}
	}
}
# endif /* DEBUG_ALLOC */

# ifdef TEST_ALLOC

Area a;
FILE *myout;

int
main(int argc, char **argv)
{
	char buf[1024];
	struct info {
		int size;
		void *value;
	};
	struct info info[1024 * 2];
	int size, ident;
	int lineno = 0;

	myout = stdout;
	ainit(&a);
	while (fgets(buf, sizeof(buf), stdin)) {
		lineno++;
		if (buf[0] == '\n' || buf[0] == '#')
			continue;
		if (sscanf(buf, " alloc %d = i%d", &size, &ident) == 2) {
			if (ident < 0 || ident > NELEM(info)) {
				fprintf(stderr, "bad ident (%d) on line %d\n",
					ident, lineno);
				exit(1);
			}
			info[ident].value = alloc(info[ident].size = size, &a);
			printf("%p = alloc(%d) [%d,i%d]\n", 
				info[ident].value, info[ident].size,
				lineno, ident);
			memset(info[ident].value, 1, size);
			continue;
		}
		if (sscanf(buf, " afree i%d", &ident) == 1) {
			if (ident < 0 || ident > NELEM(info)) {
				fprintf(stderr, "bad ident (%d) on line %d\n",
					ident, lineno);
				exit(1);
			}
			afree(info[ident].value, &a);
			printf("afree(%p) [%d,i%d]\n", info[ident].value,
				lineno, ident);
			continue;
		}
		if (sscanf(buf, " aresize i%d , %d", &ident, &size) == 2) {
			void *value;
			if (ident < 0 || ident > NELEM(info)) {
				fprintf(stderr, "bad ident (%d) on line %d\n",
					ident, lineno);
				exit(1);
			}
			value = info[ident].value;
			info[ident].value = aresize(value,
						    info[ident].size = size,
						    &a);
			printf("%p = aresize(%p, %d) [%d,i%d]\n", 
				info[ident].value, value, info[ident].size,
				lineno, ident);
			memset(info[ident].value, 1, size);
			continue;
		}
		if (sscanf(buf, " aprint i%d , %d", &ident, &size) == 2) {
			if (ident < 0 || ident > NELEM(info)) {
				fprintf(stderr, "bad ident (%d) on line %d\n",
					ident, lineno);
				exit(1);
			}
			printf("aprint(%p, %d) [%d,i%d]\n",
				info[ident].value, size, lineno, ident);
			aprint(&a, info[ident].value, size);
			continue;
		}
		if (sscanf(buf, " aprint %d", &ident) == 1) {
			if (ident < 0 || ident > NELEM(info)) {
				fprintf(stderr, "bad ident (%d) on line %d\n",
					ident, lineno);
				exit(1);
			}
			printf("aprint(0, 0) [%d]\n", lineno);
			aprint(&a, 0, 0);
			continue;
		}
		if (sscanf(buf, " afreeall %d", &ident) == 1) {
			printf("afreeall() [%d]\n", lineno);
			afreeall(&a);
			memset(info, 0, sizeof(info));
			continue;
		}
		fprintf(stderr, "unrecognized line (line %d)\n",
			lineno);
		exit(1);
	}
	return 0;
}

void
aerror(Area *ap, const char *msg)
{
	printf("aerror: %s\n", msg);
	fflush(stdout);
	abort();
}

# endif /* TEST_ALLOC */

#endif /* MEM_DEBUG */
