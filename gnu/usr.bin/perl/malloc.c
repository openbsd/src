/*    malloc.c
 *
 */

#if defined(PERL_CORE) && !defined(DEBUGGING_MSTATS)
#  define DEBUGGING_MSTATS
#endif 

#ifndef lint
#  if defined(DEBUGGING) && !defined(NO_RCHECK)
#    define RCHECK
#  endif
/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small 
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this 
 * implementation, the available sizes are 2^n-4 (or 2^n-12) bytes long.
 * If PACK_MALLOC is defined, small blocks are 2^n bytes long.
 * This is designed for use in a program that uses vast quantities of memory,
 * but bombs when it runs out. 
 */

#include "EXTERN.h"
#include "perl.h"

#ifdef DEBUGGING
#undef DEBUG_m
#define DEBUG_m(a)  if (debug & 128)   a
#endif

/* I don't much care whether these are defined in sys/types.h--LAW */

#define u_char unsigned char
#define u_int unsigned int
#define u_short unsigned short

/* 286 and atarist like big chunks, which gives too much overhead. */
#if (defined(RCHECK) || defined(I286) || defined(atarist)) && defined(PACK_MALLOC)
#undef PACK_MALLOC
#endif 


/*
 * The description below is applicable if PACK_MALLOC is not defined.
 *
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

#ifdef DEBUGGING
static void botch _((char *s));
#endif
static void morecore _((int bucket));
static int findbucket _((union overhead *freep, int srchlen));

#define	MAGIC		0xff		/* magic # on accounting info */
#define RMAGIC		0x55555555	/* magic # on range info */
#ifdef RCHECK
#  define	RSLOP		sizeof (u_int)
#  ifdef TWO_POT_OPTIMIZE
#    define MAX_SHORT_BUCKET 12
#  else
#    define MAX_SHORT_BUCKET 13
#  endif 
#else
#  define	RSLOP		0
#endif

#ifdef PACK_MALLOC
/*
 * In this case it is assumed that if we do sbrk() in 2K units, we
 * will get 2K aligned blocks. The bucket number of the given subblock is
 * on the boundary of 2K block which contains the subblock.
 * Several following bytes contain the magic numbers for the subblocks
 * in the block.
 *
 * Sizes of chunks are powers of 2 for chunks in buckets <=
 * MAX_PACKED, after this they are (2^n - sizeof(union overhead)) (to
 * get alignment right).
 *
 * We suppose that starts of all the chunks in a 2K block are in
 * different 2^n-byte-long chunks.  If the top of the last chunk is
 * aligned on a boundary of 2K block, this means that
 * sizeof(union overhead)*"number of chunks" < 2^n, or
 * sizeof(union overhead)*2K < 4^n, or n > 6 + log2(sizeof()/2)/2, if a
 * chunk of size 2^n - overhead is used. Since this rules out n = 7
 * for 8 byte alignment, we specialcase allocation of the first of 16
 * 128-byte-long chunks.
 *
 * Note that with the above assumption we automatically have enough
 * place for MAGIC at the start of 2K block.  Note also that we
 * overlay union overhead over the chunk, thus the start of the chunk
 * is immediately overwritten after freeing.
 */
#  define MAX_PACKED 6
#  define MAX_2_POT_ALGO ((1<<(MAX_PACKED + 1)) - M_OVERHEAD)
#  define TWOK_MASK ((1<<11) - 1)
#  define TWOK_MASKED(x) ((u_int)(x) & ~TWOK_MASK)
#  define TWOK_SHIFT(x) ((u_int)(x) & TWOK_MASK)
#  define OV_INDEXp(block) ((u_char*)(TWOK_MASKED(block)))
#  define OV_INDEX(block) (*OV_INDEXp(block))
#  define OV_MAGIC(block,bucket) (*(OV_INDEXp(block) +			\
				    (TWOK_SHIFT(block)>>(bucket + 3)) + \
				    (bucket > MAX_NONSHIFT ? 1 : 0)))
#  define CHUNK_SHIFT 0

static u_char n_blks[11 - 3]	 = {224, 120, 62, 31, 16, 8, 4, 2};
static u_short blk_shift[11 - 3] = {256, 128, 64, 32, 
				    16*sizeof(union overhead), 
				    8*sizeof(union overhead), 
				    4*sizeof(union overhead), 
				    2*sizeof(union overhead), 
#  define MAX_NONSHIFT 2	/* Shift 64 greater than chunk 32. */
};

#else  /* !PACK_MALLOC */

#  define OV_MAGIC(block,bucket) (block)->ov_magic
#  define OV_INDEX(block) (block)->ov_index
#  define CHUNK_SHIFT 1
#endif /* !PACK_MALLOC */

#  define M_OVERHEAD (sizeof(union overhead) + RSLOP)

/*
 * Big allocations are often of the size 2^n bytes. To make them a
 * little bit better, make blocks of size 2^n+pagesize for big n.
 */

#ifdef TWO_POT_OPTIMIZE

#  ifndef PERL_PAGESIZE
#    define PERL_PAGESIZE 4096
#  endif 
#  ifndef FIRST_BIG_TWO_POT
#    define FIRST_BIG_TWO_POT 14	/* 16K */
#  endif
#  define FIRST_BIG_BLOCK (1<<FIRST_BIG_TWO_POT) /* 16K */
/* If this value or more, check against bigger blocks. */
#  define FIRST_BIG_BOUND (FIRST_BIG_BLOCK - M_OVERHEAD)
/* If less than this value, goes into 2^n-overhead-block. */
#  define LAST_SMALL_BOUND ((FIRST_BIG_BLOCK>>1) - M_OVERHEAD)

#endif /* TWO_POT_OPTIMIZE */

#if defined(PERL_EMERGENCY_SBRK) && defined(PERL_CORE)

#ifndef BIG_SIZE
#  define BIG_SIZE (1<<16)		/* 64K */
#endif 

static char *emergency_buffer;
static MEM_SIZE emergency_buffer_size;

static char *
emergency_sbrk(size)
    MEM_SIZE size;
{
    if (size >= BIG_SIZE) {
	/* Give the possibility to recover: */
	die("Out of memory during request for %i bytes", size);
	/* croak may eat too much memory. */
    }

    if (!emergency_buffer) {		
	/* First offense, give a possibility to recover by dieing. */
	/* No malloc involved here: */
	GV **gvp = (GV**)hv_fetch(defstash, "^M", 2, 0);
	SV *sv;
	char *pv;

	if (!gvp) gvp = (GV**)hv_fetch(defstash, "\015", 1, 0);
	if (!gvp || !(sv = GvSV(*gvp)) || !SvPOK(sv) 
	    || (SvLEN(sv) < (1<<11) - M_OVERHEAD)) 
	    return (char *)-1;		/* Now die die die... */

	/* Got it, now detach SvPV: */
	pv = SvPV(sv, na);
	/* Check alignment: */
	if (((u_int)(pv - M_OVERHEAD)) & ((1<<11) - 1)) {
	    PerlIO_puts(PerlIO_stderr(),"Bad alignment of $^M!\n");
	    return (char *)-1;		/* die die die */
	}

	emergency_buffer = pv - M_OVERHEAD;
	emergency_buffer_size = SvLEN(sv) + M_OVERHEAD;
	SvPOK_off(sv);
	SvREADONLY_on(sv);
	die("Out of memory!");		/* croak may eat too much memory. */
    }
    else if (emergency_buffer_size >= size) {
	emergency_buffer_size -= size;
	return emergency_buffer + emergency_buffer_size;
    }
    
    return (char *)-1;			/* poor guy... */
}

#else /* !(defined(TWO_POT_OPTIMIZE) && defined(PERL_CORE)) */
#  define emergency_sbrk(size)	-1
#endif /* !(defined(TWO_POT_OPTIMIZE) && defined(PERL_CORE)) */

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS 30
static	union overhead *nextf[NBUCKETS];

#ifdef USE_PERL_SBRK
#define sbrk(a) Perl_sbrk(a)
char *  Perl_sbrk _((int size));
#else
extern	char *sbrk();
#endif

#ifdef DEBUGGING_MSTATS
/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static	u_int nmalloc[NBUCKETS];
static	u_int goodsbrk;
static  u_int sbrk_slack;
static  u_int start_slack;
#endif

#ifdef DEBUGGING
#define	ASSERT(p)   if (!(p)) botch(STRINGIFY(p));  else
static void
botch(s)
	char *s;
{
	PerlIO_printf(PerlIO_stderr(), "assertion botched: %s\n", s);
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

#if defined(DEBUGGING) || defined(RCHECK)
	MEM_SIZE size = nbytes;
#endif

#ifdef PERL_CORE
#ifdef HAS_64K_LIMIT
	if (nbytes > 0xffff) {
		PerlIO_printf(PerlIO_stderr(),
			      "Allocation too large: %lx\n", (long)nbytes);
		my_exit(1);
	}
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
	if ((long)nbytes < 0)
		croak("panic: malloc");
#endif
#endif /* PERL_CORE */

	/*
	 * Convert amount of memory requested into
	 * closest block size stored in hash buckets
	 * which satisfies request.  Account for
	 * space used per block for accounting.
	 */
#ifdef PACK_MALLOC
	if (nbytes == 0)
	    nbytes = 1;
	else if (nbytes > MAX_2_POT_ALGO)
#endif
	{
#ifdef TWO_POT_OPTIMIZE
		if (nbytes >= FIRST_BIG_BOUND)
			nbytes -= PERL_PAGESIZE;
#endif 
		nbytes += M_OVERHEAD;
		nbytes = (nbytes + 3) &~ 3; 
	}
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
#ifdef PERL_CORE
		if (!nomemok) {
		    PerlIO_puts(PerlIO_stderr(),"Out of memory!\n");
		    my_exit(1);
		}
#else
  		return (NULL);
#endif
	}

#ifdef PERL_CORE
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%lx: (%05lu) malloc %ld bytes\n",
	(unsigned long)(p+1),(unsigned long)(an++),(long)size));
#endif /* PERL_CORE */

	/* remove from linked list */
#ifdef RCHECK
	if (*((int*)p) & (sizeof(union overhead) - 1))
	    PerlIO_printf(PerlIO_stderr(), "Corrupt malloc ptr 0x%lx at 0x%lx\n",
		(unsigned long)*((int*)p),(unsigned long)p);
#endif
  	nextf[bucket] = p->ov_next;
	OV_MAGIC(p, bucket) = MAGIC;
#ifndef PACK_MALLOC
	OV_INDEX(p) = bucket;
#endif
#ifdef RCHECK
	/*
	 * Record allocated size of block and
	 * bound space with magic numbers.
	 */
	nbytes = (size + M_OVERHEAD + 3) &~ 3; 
  	if (nbytes <= 0x10000)
		p->ov_size = nbytes - 1;
	p->ov_rmagic = RMAGIC;
  	*((u_int *)((caddr_t)p + nbytes - RSLOP)) = RMAGIC;
#endif
  	return ((Malloc_t)(p + CHUNK_SHIFT));
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
	register MEM_SIZE siz, needed;
	int slack = 0;

  	if (nextf[bucket])
  		return;
	if (bucket == (sizeof(MEM_SIZE)*8 - 3)) {
	    croak("Allocation too large");
	}
	/*
	 * Insure memory is allocated
	 * on a page boundary.  Should
	 * make getpageize call?
	 */
#ifndef atarist /* on the atari we dont have to worry about this */
  	op = (union overhead *)sbrk(0);
#  ifndef I286
  	if ((UV)op & (0x7FF >> CHUNK_SHIFT)) {
	    slack = (0x800 >> CHUNK_SHIFT) - ((UV)op & (0x7FF >> CHUNK_SHIFT));
	    (void)sbrk(slack);
#    if defined(DEBUGGING_MSTATS)
	    sbrk_slack += slack;
#    endif
	}
#  else
	/* The sbrk(0) call on the I286 always returns the next segment */
#  endif
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
	needed = (MEM_SIZE)1 << rnu;
#ifdef TWO_POT_OPTIMIZE
	needed += (bucket >= (FIRST_BIG_TWO_POT - 3) ? PERL_PAGESIZE : 0);
#endif 
	op = (union overhead *)sbrk(needed);
	/* no more room! */
  	if (op == (union overhead *)-1) {
	    op = (union overhead *)emergency_sbrk(needed);
	    if (op == (union overhead *)-1)
  		return;
	}
#ifdef DEBUGGING_MSTATS
	goodsbrk += needed;
#endif	
	/*
	 * Round up to minimum allocation size boundary
	 * and deduct from block count to reflect.
	 */
#ifndef I286
#  ifdef PACK_MALLOC
	if ((UV)op & 0x7FF)
		croak("panic: Off-page sbrk");
#  endif
  	if ((UV)op & 7) {
  		op = (union overhead *)(((UV)op + 8) & ~7);
  		nblks--;
  	}
#else
	/* Again, this should always be ok on an 80286 */
#endif
	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	siz = 1 << (bucket + 3);
#ifdef PACK_MALLOC
	*(u_char*)op = bucket;	/* Fill index. */
	if (bucket <= MAX_PACKED - 3) {
	    op = (union overhead *) ((char*)op + blk_shift[bucket]);
	    nblks = n_blks[bucket];
#  ifdef DEBUGGING_MSTATS
	    start_slack += blk_shift[bucket];
#  endif
	} else if (bucket <= 11 - 1 - 3) {
	    op = (union overhead *) ((char*)op + blk_shift[bucket]);
	    /* nblks = n_blks[bucket]; */
	    siz -= sizeof(union overhead);
	} else op++;		/* One chunk per block. */
#endif /* !PACK_MALLOC */
  	nextf[bucket] = op;
#ifdef DEBUGGING_MSTATS
	nmalloc[bucket] += nblks;
#endif 
  	while (--nblks > 0) {
		op->ov_next = (union overhead *)((caddr_t)op + siz);
		op = (union overhead *)((caddr_t)op + siz);
  	}
	/* Not all sbrks return zeroed memory.*/
	op->ov_next = (union overhead *)NULL;
#ifdef PACK_MALLOC
	if (bucket == 7 - 3) {	/* Special case, explanation is above. */
	    union overhead *n_op = nextf[7 - 3]->ov_next;
	    nextf[7 - 3] = (union overhead *)((caddr_t)nextf[7 - 3] 
					      - sizeof(union overhead));
	    nextf[7 - 3]->ov_next = n_op;
	}
#endif /* !PACK_MALLOC */
}

Free_t
free(mp)
	Malloc_t mp;
{   
  	register MEM_SIZE size;
	register union overhead *op;
	char *cp = (char*)mp;
#ifdef PACK_MALLOC
	u_char bucket;
#endif 

#ifdef PERL_CORE
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%lx: (%05lu) free\n",(unsigned long)cp,(unsigned long)(an++)));
#endif /* PERL_CORE */

	if (cp == NULL)
		return;
	op = (union overhead *)((caddr_t)cp 
				- sizeof (union overhead) * CHUNK_SHIFT);
#ifdef PACK_MALLOC
	bucket = OV_INDEX(op);
#endif 
	if (OV_MAGIC(op, bucket) != MAGIC) {
		static int bad_free_warn = -1;
		if (bad_free_warn == -1) {
		    char *pbf = getenv("PERL_BADFREE");
		    bad_free_warn = (pbf) ? atoi(pbf) : 1;
		}
		if (!bad_free_warn)
		    return;
#ifdef RCHECK
		warn("%s free() ignored",
		    op->ov_rmagic == RMAGIC - 1 ? "Duplicate" : "Bad");
#else
		warn("Bad free() ignored");
#endif
		return;				/* sanity */
	}
#ifdef RCHECK
  	ASSERT(op->ov_rmagic == RMAGIC);
	if (OV_INDEX(op) <= MAX_SHORT_BUCKET)
		ASSERT(*(u_int *)((caddr_t)op + op->ov_size + 1 - RSLOP) == RMAGIC);
	op->ov_rmagic = RMAGIC - 1;
#endif
  	ASSERT(OV_INDEX(op) < NBUCKETS);
  	size = OV_INDEX(op);
	op->ov_next = nextf[size];
  	nextf[size] = op;
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

#ifdef DEBUGGING
	MEM_SIZE size = nbytes;
#endif

#ifdef PERL_CORE
#ifdef HAS_64K_LIMIT
	if (nbytes > 0xffff) {
		PerlIO_printf(PerlIO_stderr(),
			      "Reallocation too large: %lx\n", size);
		my_exit(1);
	}
#endif /* HAS_64K_LIMIT */
	if (!cp)
		return malloc(nbytes);
#ifdef DEBUGGING
	if ((long)nbytes < 0)
		croak("panic: realloc");
#endif
#endif /* PERL_CORE */

	op = (union overhead *)((caddr_t)cp 
				- sizeof (union overhead) * CHUNK_SHIFT);
	i = OV_INDEX(op);
	if (OV_MAGIC(op, i) == MAGIC) {
		was_alloced = 1;
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
	onb = (1L << (i + 3)) - 
#ifdef PACK_MALLOC
	    (i <= (MAX_PACKED - 3) ? 0 : M_OVERHEAD)
#else
	    M_OVERHEAD
#endif
#ifdef TWO_POT_OPTIMIZE
	    + (i >= (FIRST_BIG_TWO_POT - 3) ? PERL_PAGESIZE : 0)
#endif
	    ;
	/* 
	 *  avoid the copy if same size block.
	 *  We are not agressive with boundary cases. Note that it is
	 *  possible for small number of cases give false negative if
	 *  both new size and old one are in the bucket for
	 *  FIRST_BIG_TWO_POT, but the new one is near the lower end.
	 */
	if (was_alloced &&
	    nbytes <= onb && (nbytes > ( (onb >> 1) - M_OVERHEAD )
#ifdef TWO_POT_OPTIMIZE
			      || (i == (FIRST_BIG_TWO_POT - 3) 
				  && nbytes >= LAST_SMALL_BOUND )
#endif	
		)) {
#ifdef RCHECK
		/*
		 * Record new allocated size of block and
		 * bound space with magic numbers.
		 */
		if (OV_INDEX(op) <= MAX_SHORT_BUCKET) {
			/*
			 * Convert amount of memory requested into
			 * closest block size stored in hash buckets
			 * which satisfies request.  Account for
			 * space used per block for accounting.
			 */
			nbytes += M_OVERHEAD;
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

#ifdef PERL_CORE
#ifdef DEBUGGING
    if (debug & 128) {
	PerlIO_printf(Perl_debug_log, "0x%lx: (%05lu) rfree\n",(unsigned long)res,(unsigned long)(an++));
	PerlIO_printf(Perl_debug_log, "0x%lx: (%05lu) realloc %ld bytes\n",
	    (unsigned long)res,(unsigned long)(an++),(long)size);
    }
#endif
#endif /* PERL_CORE */
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

Malloc_t
calloc(elements, size)
	register MEM_SIZE elements;
	register MEM_SIZE size;
{
    long sz = elements * size;
    Malloc_t p = malloc(sz);

    if (p) {
	memset((void*)p, 0, sz);
    }
    return p;
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
  	int topbucket=0, totfree=0, total=0;
	u_int nfree[NBUCKETS];

  	for (i=0; i < NBUCKETS; i++) {
  		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
		nfree[i] = j;
  		totfree += nfree[i]   * (1 << (i + 3));
  		total += nmalloc[i] * (1 << (i + 3));
		if (nmalloc[i])
			topbucket = i;
  	}
  	if (s)
		PerlIO_printf(PerlIO_stderr(), "Memory allocation statistics %s (buckets 8..%d)\n",
			s, (1 << (topbucket + 3)) );
  	PerlIO_printf(PerlIO_stderr(), "%8d free:", totfree);
  	for (i=0; i <= topbucket; i++) {
  		PerlIO_printf(PerlIO_stderr(), (i<5 || i==7)?" %5d": (i<9)?" %3d":" %d", nfree[i]);
  	}
  	PerlIO_printf(PerlIO_stderr(), "\n%8d used:", total - totfree);
  	for (i=0; i <= topbucket; i++) {
  		PerlIO_printf(PerlIO_stderr(), (i<5 || i==7)?" %5d": (i<9)?" %3d":" %d", nmalloc[i] - nfree[i]);
  	}
	PerlIO_printf(PerlIO_stderr(), "\nTotal sbrk(): %8d. Odd ends: sbrk(): %7d, malloc(): %7d bytes.\n",
		      goodsbrk + sbrk_slack, sbrk_slack, start_slack);
}
#else
void
dump_mstats(s)
    char *s;
{
}
#endif
#endif /* lint */


#ifdef USE_PERL_SBRK

#   ifdef NeXT
#      define PERL_SBRK_VIA_MALLOC
#   endif

#   ifdef PERL_SBRK_VIA_MALLOC
#      if defined(HIDEMYMALLOC) || defined(EMBEDMYMALLOC)
#         undef malloc
#      else
#         include "Error: -DPERL_SBRK_VIA_MALLOC needs -D(HIDE|EMBED)MYMALLOC"
#      endif

/* it may seem schizophrenic to use perl's malloc and let it call system */
/* malloc, the reason for that is only the 3.2 version of the OS that had */
/* frequent core dumps within nxzonefreenolock. This sbrk routine put an */
/* end to the cores */

#      define SYSTEM_ALLOC(a) malloc(a)

#   endif  /* PERL_SBRK_VIA_MALLOC */

static IV Perl_sbrk_oldchunk;
static long Perl_sbrk_oldsize;

#   define PERLSBRK_32_K (1<<15)
#   define PERLSBRK_64_K (1<<16)

char *
Perl_sbrk(size)
int size;
{
    IV got;
    int small, reqsize;

    if (!size) return 0;
#ifdef PERL_CORE
    reqsize = size; /* just for the DEBUG_m statement */
#endif
#ifdef PACK_MALLOC
    size = (size + 0x7ff) & ~0x7ff;
#endif
    if (size <= Perl_sbrk_oldsize) {
	got = Perl_sbrk_oldchunk;
	Perl_sbrk_oldchunk += size;
	Perl_sbrk_oldsize -= size;
    } else {
      if (size >= PERLSBRK_32_K) {
	small = 0;
      } else {
#ifndef PERL_CORE
	reqsize = size;
#endif
	size = PERLSBRK_64_K;
	small = 1;
      }
      got = (IV)SYSTEM_ALLOC(size);
#ifdef PACK_MALLOC
      got = (got + 0x7ff) & ~0x7ff;
#endif
      if (small) {
	/* Chunk is small, register the rest for future allocs. */
	Perl_sbrk_oldchunk = got + reqsize;
	Perl_sbrk_oldsize = size - reqsize;
      }
    }

#ifdef PERL_CORE
    DEBUG_m(PerlIO_printf(Perl_debug_log, "sbrk malloc size %ld (reqsize %ld), left size %ld, give addr 0x%lx\n",
		    size, reqsize, Perl_sbrk_oldsize, got));
#endif

    return (void *)got;
}

#endif /* ! defined USE_PERL_SBRK */
