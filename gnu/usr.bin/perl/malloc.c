/*    malloc.c
 *
 */

#ifndef lint
#ifdef DEBUGGING
#define RCHECK
#endif
/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small 
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this 
 * implementation, the available sizes are 2^n-4 (or 2^n-12) bytes long.
 * This is designed for use in a program that uses vast quantities of memory,
 * but bombs when it runs out. 
 */

#include "EXTERN.h"
#include "perl.h"

/* I don't much care whether these are defined in sys/types.h--LAW */

#define u_char unsigned char
#define u_int unsigned int
#define u_short unsigned short

/*
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled and the size of the block fits
 * in two bytes, then the top two bytes hold the size of the requested block
 * plus the range checking words, and the header word MINUS ONE.
 */
union	overhead {
	union	overhead *ov_next;	/* when free */
#if MEM_ALIGNBYTES > 4
	double	strut;			/* alignment problems */
#endif
	struct {
		u_char	ovu_magic;	/* magic number */
		u_char	ovu_index;	/* bucket # */
#ifdef RCHECK
		u_short	ovu_size;	/* actual block size */
		u_int	ovu_rmagic;	/* range magic number */
#endif
	} ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_size		ovu.ovu_size
#define	ov_rmagic	ovu.ovu_rmagic
};

#ifdef debug
static void botch _((char *s));
#endif
static void morecore _((int bucket));
static int findbucket _((union overhead *freep, int srchlen));

#define	MAGIC		0xff		/* magic # on accounting info */
#define RMAGIC		0x55555555	/* magic # on range info */
#ifdef RCHECK
#define	RSLOP		sizeof (u_int)
#else
#define	RSLOP		0
#endif

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS 30
static	union overhead *nextf[NBUCKETS];
extern	char *sbrk();

#ifdef DEBUGGING_MSTATS
/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static	u_int nmalloc[NBUCKETS];
#include <stdio.h>
#endif

#ifdef debug
#define	ASSERT(p)   if (!(p)) botch("p"); else
static void
botch(s)
	char *s;
{

	printf("assertion botched: %s\n", s);
	abort();
}
#else
#define	ASSERT(p)
#endif

Malloc_t
malloc(nbytes)
	register MEM_SIZE nbytes;
{
  	register union overhead *p;
  	register int bucket = 0;
  	register MEM_SIZE shiftr;

#ifdef safemalloc
#ifdef DEBUGGING
	MEM_SIZE size = nbytes;
#endif

#ifdef MSDOS
	if (nbytes > 0xffff) {
		fprintf(stderr, "Allocation too large: %lx\n", (long)nbytes);
		my_exit(1);
	}
#endif /* MSDOS */
#ifdef DEBUGGING
	if ((long)nbytes < 0)
	    croak("panic: malloc");
#endif
#endif /* safemalloc */

	/*
	 * Convert amount of memory requested into
	 * closest block size stored in hash buckets
	 * which satisfies request.  Account for
	 * space used per block for accounting.
	 */
  	nbytes += sizeof (union overhead) + RSLOP;
  	nbytes = (nbytes + 3) &~ 3; 
  	shiftr = (nbytes - 1) >> 2;
	/* apart from this loop, this is O(1) */
  	while (shiftr >>= 1)
  		bucket++;
	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
  	if (nextf[bucket] == NULL)    
  		morecore(bucket);
  	if ((p = (union overhead *)nextf[bucket]) == NULL) {
#ifdef safemalloc
		if (!nomemok) {
		    fputs("Out of memory!\n", stderr);
		    my_exit(1);
		}
#else
  		return (NULL);
#endif
	}

#ifdef safemalloc
    DEBUG_m(fprintf(stderr,"0x%lx: (%05d) malloc %ld bytes\n",
	(unsigned long)(p+1),an++,(long)size));
#endif /* safemalloc */

	/* remove from linked list */
#ifdef RCHECK
	if (*((int*)p) & (sizeof(union overhead) - 1))
	    fprintf(stderr,"Corrupt malloc ptr 0x%lx at 0x%lx\n",
		(unsigned long)*((int*)p),(unsigned long)p);
#endif
  	nextf[bucket] = p->ov_next;
	p->ov_magic = MAGIC;
	p->ov_index= bucket;
#ifdef DEBUGGING_MSTATS
  	nmalloc[bucket]++;
#endif
#ifdef RCHECK
	/*
	 * Record allocated size of block and
	 * bound space with magic numbers.
	 */
  	if (nbytes <= 0x10000)
		p->ov_size = nbytes - 1;
	p->ov_rmagic = RMAGIC;
  	*((u_int *)((caddr_t)p + nbytes - RSLOP)) = RMAGIC;
#endif
  	return ((Malloc_t)(p + 1));
}

/*
 * Allocate more memory to the indicated bucket.
 */
static void
morecore(bucket)
	register int bucket;
{
  	register union overhead *op;
  	register int rnu;       /* 2^rnu bytes will be requested */
  	register int nblks;     /* become nblks blocks of the desired size */
	register MEM_SIZE siz;

  	if (nextf[bucket])
  		return;
	/*
	 * Insure memory is allocated
	 * on a page boundary.  Should
	 * make getpageize call?
	 */
#ifndef atarist /* on the atari we dont have to worry about this */
  	op = (union overhead *)sbrk(0);
#ifndef I286
  	if ((int)op & 0x3ff)
  		(void)sbrk(1024 - ((int)op & 0x3ff));
#else
	/* The sbrk(0) call on the I286 always returns the next segment */
#endif
#endif /* atarist */

#if !(defined(I286) || defined(atarist))
	/* take 2k unless the block is bigger than that */
  	rnu = (bucket <= 8) ? 11 : bucket + 3;
#else
	/* take 16k unless the block is bigger than that 
	   (80286s like large segments!), probably good on the atari too */
  	rnu = (bucket <= 11) ? 14 : bucket + 3;
#endif
  	nblks = 1 << (rnu - (bucket + 3));  /* how many blocks to get */
  	if (rnu < bucket)
		rnu = bucket;
	op = (union overhead *)sbrk(1L << rnu);
	/* no more room! */
  	if ((int)op == -1)
  		return;
	/*
	 * Round up to minimum allocation size boundary
	 * and deduct from block count to reflect.
	 */
#ifndef I286
  	if ((int)op & 7) {
  		op = (union overhead *)(((MEM_SIZE)op + 8) &~ 7);
  		nblks--;
  	}
#else
	/* Again, this should always be ok on an 80286 */
#endif
	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	nextf[bucket] = op;
  	siz = 1 << (bucket + 3);
  	while (--nblks > 0) {
		op->ov_next = (union overhead *)((caddr_t)op + siz);
		op = (union overhead *)((caddr_t)op + siz);
  	}
}

Free_t
free(mp)
	Malloc_t mp;
{   
  	register MEM_SIZE size;
	register union overhead *op;
	char *cp = (char*)mp;

#ifdef safemalloc
    DEBUG_m(fprintf(stderr,"0x%lx: (%05d) free\n",(unsigned long)cp,an++));
#endif /* safemalloc */

  	if (cp == NULL)
  		return;
	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
#ifdef debug
  	ASSERT(op->ov_magic == MAGIC);		/* make sure it was in use */
#else
	if (op->ov_magic != MAGIC) {
#ifdef RCHECK
		warn("%s free() ignored",
		    op->ov_rmagic == RMAGIC - 1 ? "Duplicate" : "Bad");
#else
		warn("Bad free() ignored");
#endif
		return;				/* sanity */
	}
#endif
#ifdef RCHECK
  	ASSERT(op->ov_rmagic == RMAGIC);
	if (op->ov_index <= 13)
		ASSERT(*(u_int *)((caddr_t)op + op->ov_size + 1 - RSLOP) == RMAGIC);
	op->ov_rmagic = RMAGIC - 1;
#endif
  	ASSERT(op->ov_index < NBUCKETS);
  	size = op->ov_index;
	op->ov_next = nextf[size];
  	nextf[size] = op;
#ifdef DEBUGGING_MSTATS
  	nmalloc[size]--;
#endif
}

/*
 * When a program attempts "storage compaction" as mentioned in the
 * old malloc man page, it realloc's an already freed block.  Usually
 * this is the last block it freed; occasionally it might be farther
 * back.  We have to search all the free lists for the block in order
 * to determine its bucket: 1st we make one pass thru the lists
 * checking only the first block in each; if that fails we search
 * ``reall_srchlen'' blocks in each list for a match (the variable
 * is extern so the caller can modify it).  If that fails we just copy
 * however many bytes was given to realloc() and hope it's not huge.
 */
int reall_srchlen = 4;	/* 4 should be plenty, -1 =>'s whole list */

Malloc_t
realloc(mp, nbytes)
	Malloc_t mp; 
	MEM_SIZE nbytes;
{   
  	register MEM_SIZE onb;
	union overhead *op;
  	char *res;
	register int i;
	int was_alloced = 0;
	char *cp = (char*)mp;

#ifdef safemalloc
#ifdef DEBUGGING
	MEM_SIZE size = nbytes;
#endif

#ifdef MSDOS
	if (nbytes > 0xffff) {
		fprintf(stderr, "Reallocation too large: %lx\n", size);
		my_exit(1);
	}
#endif /* MSDOS */
	if (!cp)
		return malloc(nbytes);
#ifdef DEBUGGING
	if ((long)nbytes < 0)
		croak("panic: realloc");
#endif
#endif /* safemalloc */

	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
	if (op->ov_magic == MAGIC) {
		was_alloced++;
		i = op->ov_index;
	} else {
		/*
		 * Already free, doing "compaction".
		 *
		 * Search for the old block of memory on the
		 * free list.  First, check the most common
		 * case (last element free'd), then (this failing)
		 * the last ``reall_srchlen'' items free'd.
		 * If all lookups fail, then assume the size of
		 * the memory block being realloc'd is the
		 * smallest possible.
		 */
		if ((i = findbucket(op, 1)) < 0 &&
		    (i = findbucket(op, reall_srchlen)) < 0)
			i = 0;
	}
	onb = (1L << (i + 3)) - sizeof (*op) - RSLOP;
	/* avoid the copy if same size block */
	if (was_alloced &&
	    nbytes <= onb && nbytes > (onb >> 1) - sizeof(*op) - RSLOP) {
#ifdef RCHECK
		/*
		 * Record new allocated size of block and
		 * bound space with magic numbers.
		 */
		if (op->ov_index <= 13) {
			/*
			 * Convert amount of memory requested into
			 * closest block size stored in hash buckets
			 * which satisfies request.  Account for
			 * space used per block for accounting.
			 */
			nbytes += sizeof (union overhead) + RSLOP;
			nbytes = (nbytes + 3) &~ 3; 
			op->ov_size = nbytes - 1;
			*((u_int *)((caddr_t)op + nbytes - RSLOP)) = RMAGIC;
		}
#endif
		res = cp;
	}
	else {
		if ((res = (char*)malloc(nbytes)) == NULL)
			return (NULL);
		if (cp != res)			/* common optimization */
			Copy(cp, res, (MEM_SIZE)(nbytes<onb?nbytes:onb), char);
		if (was_alloced)
			free(cp);
	}

#ifdef safemalloc
#ifdef DEBUGGING
    if (debug & 128) {
	fprintf(stderr,"0x%lx: (%05d) rfree\n",(unsigned long)res,an++);
	fprintf(stderr,"0x%lx: (%05d) realloc %ld bytes\n",
	    (unsigned long)res,an++,(long)size);
    }
#endif
#endif /* safemalloc */
  	return ((Malloc_t)res);
}

/*
 * Search ``srchlen'' elements of each free list for a block whose
 * header starts at ``freep''.  If srchlen is -1 search the whole list.
 * Return bucket number, or -1 if not found.
 */
static int
findbucket(freep, srchlen)
	union overhead *freep;
	int srchlen;
{
	register union overhead *p;
	register int i, j;

	for (i = 0; i < NBUCKETS; i++) {
		j = 0;
		for (p = nextf[i]; p && j != srchlen; p = p->ov_next) {
			if (p == freep)
				return (i);
			j++;
		}
	}
	return (-1);
}

#ifdef DEBUGGING_MSTATS
/*
 * mstats - print out statistics about malloc
 * 
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
dump_mstats(s)
	char *s;
{
  	register int i, j;
  	register union overhead *p;
  	int topbucket=0, totfree=0, totused=0;
	u_int nfree[NBUCKETS];

  	for (i=0; i < NBUCKETS; i++) {
  		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
		nfree[i] = j;
  		totfree += nfree[i]   * (1 << (i + 3));
  		totused += nmalloc[i] * (1 << (i + 3));
		if (nfree[i] || nmalloc[i])
			topbucket = i;
  	}
  	if (s)
		fprintf(stderr, "Memory allocation statistics %s (buckets 8..%d)\n",
			s, (1 << (topbucket + 3)) );
  	fprintf(stderr, " %7d free: ", totfree);
  	for (i=0; i <= topbucket; i++) {
  		fprintf(stderr, (i<5)?" %5d":" %3d", nfree[i]);
  	}
  	fprintf(stderr, "\n %7d used: ", totused);
  	for (i=0; i <= topbucket; i++) {
  		fprintf(stderr, (i<5)?" %5d":" %3d", nmalloc[i]);
  	}
  	fprintf(stderr, "\n");
}
#else
void
dump_mstats(s)
    char *s;
{
}
#endif
#endif /* lint */
