/*	$OpenBSD: malloc.c,v 1.7 2015/01/22 05:48:17 deraadt Exp $	*/
/*
 * Copyright (c) 2008, 2010, 2011 Otto Moerbeek <otto@drijf.net>
 * Copyright (c) 2012 Matthew Dempsky <matthew@openbsd.org>
 * Copyright (c) 2008 Damien Miller <djm@openbsd.org>
 * Copyright (c) 2000 Poul-Henning Kamp <phk@FreeBSD.org>
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

/*
 * If we meet some day, and you think this stuff is worth it, you
 * can buy me a beer in return. Poul-Henning Kamp
 */

#include <sys/param.h>	/* PAGE_SHIFT ALIGN */
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdint.h>
#include <unistd.h>

#include  "archdep.h"

#if defined(__sparc__) && !defined(__sparcv9__)
#define MALLOC_PAGESHIFT	(13U)
#elif defined(__mips64__)
#define MALLOC_PAGESHIFT	(14U)
#else
#define MALLOC_PAGESHIFT	(PAGE_SHIFT)
#endif

#define MALLOC_MINSHIFT		4
#define MALLOC_MAXSHIFT		(MALLOC_PAGESHIFT - 1)
#define MALLOC_PAGESIZE		(1UL << MALLOC_PAGESHIFT)
#define MALLOC_MINSIZE		(1UL << MALLOC_MINSHIFT)
#define MALLOC_PAGEMASK		(MALLOC_PAGESIZE - 1)
#define MASK_POINTER(p)		((void *)(((uintptr_t)(p)) & ~MALLOC_PAGEMASK))

#define MALLOC_MAXCHUNK		(1 << MALLOC_MAXSHIFT)
#define MALLOC_MAXCACHE		256
#define MALLOC_DELAYED_CHUNK_MASK	15
#define MALLOC_INITIAL_REGIONS	512
#define MALLOC_DEFAULT_CACHE	64
#define	MALLOC_CHUNK_LISTS	4

/*
 * When the P option is active, we move allocations between half a page
 * and a whole page towards the end, subject to alignment constraints.
 * This is the extra headroom we allow. Set to zero to be the most
 * strict.
 */
#define MALLOC_LEEWAY		0

#define PAGEROUND(x)  (((x) + (MALLOC_PAGEMASK)) & ~MALLOC_PAGEMASK)

/*
 * What to use for Junk.  This is the byte value we use to fill with
 * when the 'J' option is enabled. Use SOME_JUNK right after alloc,
 * and SOME_FREEJUNK right before free.
 */
#define SOME_JUNK		0xd0	/* as in "Duh" :-) */
#define SOME_FREEJUNK		0xdf

#define MMAP(sz)	_dl_mmap(NULL, (size_t)(sz), PROT_READ | PROT_WRITE, \
    MAP_ANON | MAP_PRIVATE, -1, (off_t) 0)

#define MMAP_ERROR(p)	(_dl_mmap_error(p) ? MAP_FAILED : (p))

struct region_info {
	void *p;		/* page; low bits used to mark chunks */
	uintptr_t size;		/* size for pages, or chunk_info pointer */
};

LIST_HEAD(chunk_head, chunk_info);

struct dir_info {
	u_int32_t canary1;
	struct region_info *r;		/* region slots */
	size_t regions_total;		/* number of region slots */
	size_t regions_free;		/* number of free slots */
					/* lists of free chunk info structs */
	struct chunk_head chunk_info_list[MALLOC_MAXSHIFT + 1];
					/* lists of chunks with free slots */
	struct chunk_head chunk_dir[MALLOC_MAXSHIFT + 1][MALLOC_CHUNK_LISTS];
	size_t free_regions_size;	/* free pages cached */
					/* free pages cache */
	struct region_info free_regions[MALLOC_MAXCACHE];
					/* delayed free chunk slots */
	void *delayed_chunks[MALLOC_DELAYED_CHUNK_MASK + 1];
	size_t rbytesused;		/* random bytes used */
	u_char rbytes[256];		/* random bytes */
	u_short chunk_start;
	u_int32_t canary2;
};
#define DIR_INFO_RSZ	((sizeof(struct dir_info) + MALLOC_PAGEMASK) & \
			~MALLOC_PAGEMASK)

/*
 * This structure describes a page worth of chunks.
 *
 * How many bits per u_short in the bitmap
 */
#define MALLOC_BITS		(NBBY * sizeof(u_short))
struct chunk_info {
	LIST_ENTRY(chunk_info) entries;
	void *page;			/* pointer to the page */
	u_int32_t canary;
	u_short size;			/* size of this page's chunks */
	u_short shift;			/* how far to shift for this size */
	u_short free;			/* how many free chunks */
	u_short total;			/* how many chunk */
					/* which chunks are free */
	u_short bits[1];
};

struct malloc_readonly {
	struct dir_info *g_pool;	/* Main bookkeeping information */
	int	malloc_freenow;		/* Free quickly - disable chunk rnd */
	int	malloc_freeunmap;	/* mprotect free pages PROT_NONE? */
	int	malloc_junk;		/* junk fill? */
	int	malloc_move;		/* move allocations to end of page? */
	size_t	malloc_guard;		/* use guard pages after allocations? */
	u_int	malloc_cache;		/* free pages we cache */
	u_int32_t malloc_canary;	/* Matched against ones in g_pool */
};

/* This object is mapped PROT_READ after initialisation to prevent tampering */
static union {
	struct malloc_readonly mopts;
	u_char _pad[MALLOC_PAGESIZE];
} malloc_readonly __attribute__((aligned(MALLOC_PAGESIZE)));
#define mopts	malloc_readonly.mopts
#define g_pool	mopts.g_pool

static char	*malloc_func;		/* current function */
static int	malloc_active;		/* status of malloc */

static u_char getrbyte(struct dir_info *d);

/* low bits of r->p determine size: 0 means >= page size and p->size holding
 *  real size, otherwise r->size is a shift count, or 1 for malloc(0)
 */
#define REALSIZE(sz, r)						\
	(sz) = (uintptr_t)(r)->p & MALLOC_PAGEMASK,		\
	(sz) = ((sz) == 0 ? (r)->size : ((sz) == 1 ? 0 : (1 << ((sz)-1))))

static inline size_t
hash(void *p)
{
	size_t sum;
	uintptr_t u;

	u = (uintptr_t)p >> MALLOC_PAGESHIFT;
	sum = u;
	sum = (sum << 7) - sum + (u >> 16);
#ifdef __LP64__
	sum = (sum << 7) - sum + (u >> 32);
	sum = (sum << 7) - sum + (u >> 48);
#endif
	return sum;
}

static void
wrterror(char *msg)
{
	char		*q = " error: ";
	struct iovec	iov[4];

	iov[0].iov_base = malloc_func;
	iov[0].iov_len = _dl_strlen(malloc_func);
	iov[1].iov_base = q;
	iov[1].iov_len = _dl_strlen(q);
	iov[2].iov_base = msg;
	iov[2].iov_len = _dl_strlen(msg);
	iov[3].iov_base = "\n";
	iov[3].iov_len = 1;
	_dl_write(STDERR_FILENO, iov[0].iov_base, iov[0].iov_len);
	_dl_write(STDERR_FILENO, iov[1].iov_base, iov[1].iov_len);
	_dl_write(STDERR_FILENO, iov[2].iov_base, iov[2].iov_len);
	_dl_write(STDERR_FILENO, iov[3].iov_base, iov[3].iov_len);
	_dl_exit(7);
}

static void
rbytes_init(struct dir_info *d)
{
	_dl_randombuf(d->rbytes, sizeof(d->rbytes));
	d->rbytesused = 0;
}

static inline u_char
getrbyte(struct dir_info *d)
{
	u_char x;

	if (d->rbytesused >= sizeof(d->rbytes))
		rbytes_init(d);
	x = d->rbytes[d->rbytesused++];
	return x;
}

/*
 * Cache maintenance. We keep at most malloc_cache pages cached.
 * If the cache is becoming full, unmap pages in the cache for real,
 * and then add the region to the cache
 * Opposed to the regular region data structure, the sizes in the
 * cache are in MALLOC_PAGESIZE units.
 */
static void
unmap(struct dir_info *d, void *p, size_t sz)
{
	size_t psz = sz >> MALLOC_PAGESHIFT;
	size_t rsz, tounmap;
	struct region_info *r;
	u_int i, offset;

	if (sz != PAGEROUND(sz)) {
		wrterror("munmap round");
		return;
	}

	if (psz > mopts.malloc_cache) {
		if (_dl_munmap(p, sz))
			wrterror("munmap");
		return;
	}
	tounmap = 0;
	rsz = mopts.malloc_cache - d->free_regions_size;
	if (psz > rsz)
		tounmap = psz - rsz;
	offset = getrbyte(d);
	for (i = 0; tounmap > 0 && i < mopts.malloc_cache; i++) {
		r = &d->free_regions[(i + offset) & (mopts.malloc_cache - 1)];
		if (r->p != NULL) {
			rsz = r->size << MALLOC_PAGESHIFT;
			if (_dl_munmap(r->p, rsz))
				wrterror("munmap");
			r->p = NULL;
			if (tounmap > r->size)
				tounmap -= r->size;
			else
				tounmap = 0;
			d->free_regions_size -= r->size;
			r->size = 0;
		}
	}
	if (tounmap > 0)
		wrterror("malloc cache underflow");
	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[(i + offset) & (mopts.malloc_cache - 1)];
		if (r->p == NULL) {
			if (mopts.malloc_freeunmap)
				_dl_mprotect(p, sz, PROT_NONE);
			r->p = p;
			r->size = psz;
			d->free_regions_size += psz;
			break;
		}
	}
	if (i == mopts.malloc_cache)
		wrterror("malloc free slot lost");
	if (d->free_regions_size > mopts.malloc_cache)
		wrterror("malloc cache overflow");
}

static void *
map(struct dir_info *d, size_t sz, int zero_fill)
{
	size_t psz = sz >> MALLOC_PAGESHIFT;
	struct region_info *r, *big = NULL;
	u_int i, offset;
	void *p;

	if (mopts.malloc_canary != (d->canary1 ^ (u_int32_t)(uintptr_t)d) ||
	    d->canary1 != ~d->canary2)
		wrterror("internal struct corrupt");
	if (sz != PAGEROUND(sz)) {
		wrterror("map round");
		return MAP_FAILED;
	}
	if (psz > d->free_regions_size) {
		p = MMAP(sz);
		p = MMAP_ERROR(p);
		/* zero fill not needed */
		return p;
	}
	offset = getrbyte(d);
	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[(i + offset) & (mopts.malloc_cache - 1)];
		if (r->p != NULL) {
			if (r->size == psz) {
				p = r->p;
				if (mopts.malloc_freeunmap)
					_dl_mprotect(p, sz, PROT_READ | PROT_WRITE);
				r->p = NULL;
				r->size = 0;
				d->free_regions_size -= psz;
				if (zero_fill)
					_dl_memset(p, 0, sz);
				else if (mopts.malloc_junk == 2 &&
				    mopts.malloc_freeunmap)
					_dl_memset(p, SOME_FREEJUNK, sz);
				return p;
			} else if (r->size > psz)
				big = r;
		}
	}
	if (big != NULL) {
		r = big;
		p = (char *)r->p + ((r->size - psz) << MALLOC_PAGESHIFT);
		if (mopts.malloc_freeunmap)
			_dl_mprotect(p, sz, PROT_READ | PROT_WRITE);
		r->size -= psz;
		d->free_regions_size -= psz;
		if (zero_fill)
			_dl_memset(p, 0, sz);
		else if (mopts.malloc_junk == 2 && mopts.malloc_freeunmap)
			_dl_memset(p, SOME_FREEJUNK, sz);
		return p;
	}
	p = MMAP(sz);
	p = MMAP_ERROR(p);
	if (d->free_regions_size > mopts.malloc_cache)
		wrterror("malloc cache");
	/* zero fill not needed */
	return p;
}

/*
 * Initialize a dir_info, which should have been cleared by caller
 */
static int
omalloc_init(struct dir_info **dp)
{
	char *p;
	int i, j;
	size_t d_avail, regioninfo_size, tmp;
	struct dir_info *d;

	/*
	 * Default options
	 */
	mopts.malloc_junk = 1;
	mopts.malloc_move = 1;
	mopts.malloc_cache = MALLOC_DEFAULT_CACHE;
	mopts.malloc_guard = MALLOC_PAGESIZE;

	do {
		_dl_randombuf(&mopts.malloc_canary,
		    sizeof(mopts.malloc_canary));
	} while (mopts.malloc_canary == 0);

	/*
	 * Allocate dir_info with a guard page on either side. Also
	 * randomise offset inside the page at which the dir_info
	 * lies (subject to alignment by 1 << MALLOC_MINSHIFT)
	 */
	p = MMAP(DIR_INFO_RSZ + (MALLOC_PAGESIZE * 2));
	p = MMAP_ERROR(p);
	if (p == MAP_FAILED)
		return -1;
	_dl_mprotect(p, MALLOC_PAGESIZE, PROT_NONE);
	_dl_mprotect(p + MALLOC_PAGESIZE + DIR_INFO_RSZ,
	    MALLOC_PAGESIZE, PROT_NONE);
	d_avail = (DIR_INFO_RSZ - sizeof(*d)) >> MALLOC_MINSHIFT;

	_dl_randombuf(&tmp, sizeof(tmp));
	d = (struct dir_info *)(p + MALLOC_PAGESIZE +
	    ((tmp % d_avail) << MALLOC_MINSHIFT)); /* not uniform */

	rbytes_init(d);
	d->regions_free = d->regions_total = MALLOC_INITIAL_REGIONS;
	regioninfo_size = d->regions_total * sizeof(struct region_info);
	d->r = MMAP(regioninfo_size);
	d->r = MMAP_ERROR(d->r);
	if (d->r == MAP_FAILED) {
		wrterror("malloc init mmap failed");
		d->regions_total = 0;
		return 1;
	}
	for (i = 0; i <= MALLOC_MAXSHIFT; i++) {
		LIST_INIT(&d->chunk_info_list[i]);
		for (j = 0; j < MALLOC_CHUNK_LISTS; j++)
			LIST_INIT(&d->chunk_dir[i][j]);
	}
	d->canary1 = mopts.malloc_canary ^ (u_int32_t)(uintptr_t)d;
	d->canary2 = ~d->canary1;

	*dp = d;

	/*
	 * Options have been set and will never be reset.
	 * Prevent further tampering with them.
	 */
	if (((uintptr_t)&malloc_readonly & MALLOC_PAGEMASK) == 0)
		_dl_mprotect(&malloc_readonly, sizeof(malloc_readonly), PROT_READ);

	return 0;
}

static int
omalloc_grow(struct dir_info *d)
{
	size_t newtotal;
	size_t newsize;
	size_t mask;
	size_t i;
	struct region_info *p;

	if (d->regions_total > SIZE_MAX / sizeof(struct region_info) / 2 )
		return 1;

	newtotal = d->regions_total * 2;
	newsize = newtotal * sizeof(struct region_info);
	mask = newtotal - 1;

	p = MMAP(newsize);
	p = MMAP_ERROR(p);
	if (p == MAP_FAILED)
		return 1;

	_dl_memset(p, 0, newsize);
	for (i = 0; i < d->regions_total; i++) {
		void *q = d->r[i].p;
		if (q != NULL) {
			size_t index = hash(q) & mask;
			while (p[index].p != NULL) {
				index = (index - 1) & mask;
			}
			p[index] = d->r[i];
		}
	}
	/* avoid pages containing meta info to end up in cache */
	if (_dl_munmap(d->r, d->regions_total * sizeof(struct region_info)))
		wrterror("munmap");
	d->regions_free = d->regions_free + d->regions_total;
	d->regions_total = newtotal;
	d->r = p;
	return 0;
}

static struct chunk_info *
alloc_chunk_info(struct dir_info *d, int bits)
{
	struct chunk_info *p;
	size_t size, count;

	if (bits == 0)
		count = MALLOC_PAGESIZE / MALLOC_MINSIZE;
	else
		count = MALLOC_PAGESIZE >> bits;

	size = howmany(count, MALLOC_BITS);
	size = sizeof(struct chunk_info) + (size - 1) * sizeof(u_short);
	size = ALIGN(size);

	if (LIST_EMPTY(&d->chunk_info_list[bits])) {
		char *q;
		int i;

		q = MMAP(MALLOC_PAGESIZE);
		q = MMAP_ERROR(q);
		if (q == MAP_FAILED)
			return NULL;
		count = MALLOC_PAGESIZE / size;
		for (i = 0; i < count; i++, q += size)
			LIST_INSERT_HEAD(&d->chunk_info_list[bits],
			    (struct chunk_info *)q, entries);
	}
	p = LIST_FIRST(&d->chunk_info_list[bits]);
	LIST_REMOVE(p, entries);
	_dl_memset(p, 0, size);
	p->canary = d->canary1;
	return p;
}


/*
 * The hashtable uses the assumption that p is never NULL. This holds since
 * non-MAP_FIXED mappings with hint 0 start at BRKSIZ.
 */
static int
insert(struct dir_info *d, void *p, size_t sz)
{
	size_t index;
	size_t mask;
	void *q;

	if (d->regions_free * 4 < d->regions_total) {
		if (omalloc_grow(d))
			return 1;
	}
	mask = d->regions_total - 1;
	index = hash(p) & mask;
	q = d->r[index].p;
	while (q != NULL) {
		index = (index - 1) & mask;
		q = d->r[index].p;
	}
	d->r[index].p = p;
	d->r[index].size = sz;
	d->regions_free--;
	return 0;
}

static struct region_info *
find(struct dir_info *d, void *p)
{
	size_t index;
	size_t mask = d->regions_total - 1;
	void *q, *r;

	if (mopts.malloc_canary != (d->canary1 ^ (u_int32_t)(uintptr_t)d) ||
	    d->canary1 != ~d->canary2)
		wrterror("internal struct corrupt");
	p = MASK_POINTER(p);
	index = hash(p) & mask;
	r = d->r[index].p;
	q = MASK_POINTER(r);
	while (q != p && r != NULL) {
		index = (index - 1) & mask;
		r = d->r[index].p;
		q = MASK_POINTER(r);
	}
	return (q == p && r != NULL) ? &d->r[index] : NULL;
}

static void
delete(struct dir_info *d, struct region_info *ri)
{
	/* algorithm R, Knuth Vol III section 6.4 */
	size_t mask = d->regions_total - 1;
	size_t i, j, r;

	if (d->regions_total & (d->regions_total - 1))
		wrterror("regions_total not 2^x");
	d->regions_free++;

	i = ri - d->r;
	for (;;) {
		d->r[i].p = NULL;
		d->r[i].size = 0;
		j = i;
		for (;;) {
			i = (i - 1) & mask;
			if (d->r[i].p == NULL)
				return;
			r = hash(d->r[i].p) & mask;
			if ((i <= r && r < j) || (r < j && j < i) ||
			    (j < i && i <= r))
				continue;
			d->r[j] = d->r[i];
			break;
		}

	}
}

/*
 * Allocate a page of chunks
 */
static struct chunk_info *
omalloc_make_chunks(struct dir_info *d, int bits, int listnum)
{
	struct chunk_info *bp;
	void		*pp;
	int		i, k;

	/* Allocate a new bucket */
	pp = map(d, MALLOC_PAGESIZE, 0);
	if (pp == MAP_FAILED)
		return NULL;

	bp = alloc_chunk_info(d, bits);
	if (bp == NULL) {
		unmap(d, pp, MALLOC_PAGESIZE);
		return NULL;
	}

	/* memory protect the page allocated in the malloc(0) case */
	if (bits == 0) {
		bp->size = 0;
		bp->shift = 1;
		i = MALLOC_MINSIZE - 1;
		while (i >>= 1)
			bp->shift++;
		bp->total = bp->free = MALLOC_PAGESIZE >> bp->shift;
		bp->page = pp;

		k = _dl_mprotect(pp, MALLOC_PAGESIZE, PROT_NONE);
		if (k < 0) {
			unmap(d, pp, MALLOC_PAGESIZE);
			LIST_INSERT_HEAD(&d->chunk_info_list[0], bp, entries);
			return NULL;
		}
	} else {
		bp->size = 1U << bits;
		bp->shift = bits;
		bp->total = bp->free = MALLOC_PAGESIZE >> bits;
		bp->page = pp;
	}

	/* set all valid bits in the bitmap */
	k = bp->total;
	i = 0;

	/* Do a bunch at a time */
	for (; (k - i) >= MALLOC_BITS; i += MALLOC_BITS)
		bp->bits[i / MALLOC_BITS] = (u_short)~0U;

	for (; i < k; i++)
		bp->bits[i / MALLOC_BITS] |= (u_short)1U << (i % MALLOC_BITS);

	LIST_INSERT_HEAD(&d->chunk_dir[bits][listnum], bp, entries);

	bits++;
	if ((uintptr_t)pp & bits)
		wrterror("pp & bits");

	insert(d, (void *)((uintptr_t)pp | bits), (uintptr_t)bp);
	return bp;
}


/*
 * Allocate a chunk
 */
static void *
malloc_bytes(struct dir_info *d, size_t size)
{
	int		i, j, listnum;
	size_t		k;
	u_short		u, *lp;
	struct chunk_info *bp;

	if (mopts.malloc_canary != (d->canary1 ^ (u_int32_t)(uintptr_t)d) ||
	    d->canary1 != ~d->canary2)
		wrterror("internal struct corrupt");
	/* Don't bother with anything less than this */
	/* unless we have a malloc(0) requests */
	if (size != 0 && size < MALLOC_MINSIZE)
		size = MALLOC_MINSIZE;

	/* Find the right bucket */
	if (size == 0)
		j = 0;
	else {
		j = MALLOC_MINSHIFT;
		i = (size - 1) >> (MALLOC_MINSHIFT - 1);
		while (i >>= 1)
			j++;
	}

	listnum = getrbyte(d) % MALLOC_CHUNK_LISTS;
	/* If it's empty, make a page more of that size chunks */
	if ((bp = LIST_FIRST(&d->chunk_dir[j][listnum])) == NULL) {
		bp = omalloc_make_chunks(d, j, listnum);
		if (bp == NULL)
			return NULL;
	}

	if (bp->canary != d->canary1)
		wrterror("chunk info corrupted");

	i = d->chunk_start;
	if (bp->free > 1)
		i += getrbyte(d);
	if (i >= bp->total)
		i &= bp->total - 1;
	for (;;) {
		for (;;) {
			lp = &bp->bits[i / MALLOC_BITS];
			if (!*lp) {
				i += MALLOC_BITS;
				i &= ~(MALLOC_BITS - 1);
				if (i >= bp->total)
					i = 0;
			} else
				break;
		}
		k = i % MALLOC_BITS;
		u = 1 << k;
		if (*lp & u)
			break;
		if (++i >= bp->total)
			i = 0;
	}
	d->chunk_start += i + 1;
	*lp ^= u;

	/* If there are no more free, remove from free-list */
	if (!--bp->free)
		LIST_REMOVE(bp, entries);

	/* Adjust to the real offset of that chunk */
	k += (lp - bp->bits) * MALLOC_BITS;
	k <<= bp->shift;

	if (mopts.malloc_junk == 2 && bp->size > 0)
		_dl_memset((char *)bp->page + k, SOME_JUNK, bp->size);
	return ((char *)bp->page + k);
}

static uint32_t
find_chunknum(struct dir_info *d, struct region_info *r, void *ptr)
{
	struct chunk_info *info;
	uint32_t chunknum;

	info = (struct chunk_info *)r->size;
	if (info->canary != d->canary1)
		wrterror("chunk info corrupted");

	/* Find the chunk number on the page */
	chunknum = ((uintptr_t)ptr & MALLOC_PAGEMASK) >> info->shift;

	if ((uintptr_t)ptr & ((1U << (info->shift)) - 1)) {
		wrterror("modified chunk-pointer");
		return -1;
	}
	if (info->bits[chunknum / MALLOC_BITS] &
	    (1U << (chunknum % MALLOC_BITS))) {
		wrterror("chunk is already free");
		return -1;
	}
	return chunknum;
}

/*
 * Free a chunk, and possibly the page it's on, if the page becomes empty.
 */
static void
free_bytes(struct dir_info *d, struct region_info *r, void *ptr)
{
	struct chunk_head *mp;
	struct chunk_info *info;
	uint32_t chunknum;
	int listnum;

	info = (struct chunk_info *)r->size;
	if ((chunknum = find_chunknum(d, r, ptr)) == -1)
		return;

	info->bits[chunknum / MALLOC_BITS] |= 1U << (chunknum % MALLOC_BITS);
	info->free++;

	if (info->free == 1) {
		/* Page became non-full */
		listnum = getrbyte(d) % MALLOC_CHUNK_LISTS;
		if (info->size != 0)
			mp = &d->chunk_dir[info->shift][listnum];
		else
			mp = &d->chunk_dir[0][listnum];

		LIST_INSERT_HEAD(mp, info, entries);
		return;
	}

	if (info->free != info->total)
		return;

	LIST_REMOVE(info, entries);

	if (info->size == 0 && !mopts.malloc_freeunmap)
		_dl_mprotect(info->page, MALLOC_PAGESIZE, PROT_READ | PROT_WRITE);
	unmap(d, info->page, MALLOC_PAGESIZE);

	delete(d, r);
	if (info->size != 0)
		mp = &d->chunk_info_list[info->shift];
	else
		mp = &d->chunk_info_list[0];
	LIST_INSERT_HEAD(mp, info, entries);
}



static void *
omalloc(size_t sz, int zero_fill)
{
	void *p;
	size_t psz;

	if (sz > MALLOC_MAXCHUNK) {
		if (sz >= SIZE_MAX - mopts.malloc_guard - MALLOC_PAGESIZE) {
			return NULL;
		}
		sz += mopts.malloc_guard;
		psz = PAGEROUND(sz);
		p = map(g_pool, psz, zero_fill);
		if (p == MAP_FAILED) {
			return NULL;
		}
		if (insert(g_pool, p, sz)) {
			unmap(g_pool, p, psz);
			return NULL;
		}
		if (mopts.malloc_guard) {
			if (_dl_mprotect((char *)p + psz - mopts.malloc_guard,
			    mopts.malloc_guard, PROT_NONE))
				wrterror("mprotect");
		}

		if (mopts.malloc_move &&
		    sz - mopts.malloc_guard < MALLOC_PAGESIZE -
		    MALLOC_LEEWAY) {
			/* fill whole allocation */
			if (mopts.malloc_junk == 2)
				_dl_memset(p, SOME_JUNK, psz - mopts.malloc_guard);
			/* shift towards the end */
			p = ((char *)p) + ((MALLOC_PAGESIZE - MALLOC_LEEWAY -
			    (sz - mopts.malloc_guard)) & ~(MALLOC_MINSIZE-1));
			/* fill zeros if needed and overwritten above */
			if (zero_fill && mopts.malloc_junk == 2)
				_dl_memset(p, 0, sz - mopts.malloc_guard);
		} else {
			if (mopts.malloc_junk == 2) {
				if (zero_fill)
					_dl_memset((char *)p + sz - mopts.malloc_guard,
					    SOME_JUNK, psz - sz);
				else
					_dl_memset(p, SOME_JUNK,
					    psz - mopts.malloc_guard);
			}
		}

	} else {
		/* takes care of SOME_JUNK */
		p = malloc_bytes(g_pool, sz);
		if (zero_fill && p != NULL && sz > 0)
			_dl_memset(p, 0, sz);
	}

	return p;
}

/*
 * Common function for handling recursion.  Only
 * print the error message once, to avoid making the problem
 * potentially worse.
 */
static void
malloc_recurse(void)
{
	static int noprint;

	if (noprint == 0) {
		noprint = 1;
		wrterror("recursive call");
	}
	malloc_active--;
}

static int
malloc_init(void)
{
	if (omalloc_init(&g_pool))
		return -1;
	return 0;
}

void *
_dl_malloc(size_t size)
{
	void *r;

	malloc_func = "malloc():";
	if (g_pool == NULL) {
		if (malloc_init() != 0)
			return NULL;
	}
	if (malloc_active++) {
		malloc_recurse();
		return NULL;
	}
	r = omalloc(size, 0);
	malloc_active--;
	return r;
}

static void
ofree(void *p)
{
	struct region_info *r;
	size_t sz;

	r = find(g_pool, p);
	if (r == NULL) {
		wrterror("bogus pointer (double free?)");
		return;
	}
	REALSIZE(sz, r);
	if (sz > MALLOC_MAXCHUNK) {
		if (sz - mopts.malloc_guard >= MALLOC_PAGESIZE -
		    MALLOC_LEEWAY) {
			if (r->p != p) {
				wrterror("bogus pointer");
				return;
			}
		} else {
#if notyetbecause_of_realloc
			/* shifted towards the end */
			if (p != ((char *)r->p) + ((MALLOC_PAGESIZE -
			    MALLOC_MINSIZE - sz - mopts.malloc_guard) &
			    ~(MALLOC_MINSIZE-1))) {
			}
#endif
			p = r->p;
		}
		if (mopts.malloc_guard) {
			if (sz < mopts.malloc_guard)
				wrterror("guard size");
			if (!mopts.malloc_freeunmap) {
				if (_dl_mprotect((char *)p + PAGEROUND(sz) -
				    mopts.malloc_guard, mopts.malloc_guard,
				    PROT_READ | PROT_WRITE))
					wrterror("mprotect");
			}
		}
		if (mopts.malloc_junk && !mopts.malloc_freeunmap) {
			size_t amt = mopts.malloc_junk == 1 ? MALLOC_MAXCHUNK :
			    PAGEROUND(sz) - mopts.malloc_guard;
			_dl_memset(p, SOME_FREEJUNK, amt);
		}
		unmap(g_pool, p, PAGEROUND(sz));
		delete(g_pool, r);
	} else {
		void *tmp;
		int i;

		if (mopts.malloc_junk && sz > 0)
			_dl_memset(p, SOME_FREEJUNK, sz);
		if (!mopts.malloc_freenow) {
			if (find_chunknum(g_pool, r, p) == -1)
				return;
			i = getrbyte(g_pool) & MALLOC_DELAYED_CHUNK_MASK;
			tmp = p;
			p = g_pool->delayed_chunks[i];
			if (tmp == p) {
				wrterror("double free");
				return;
			}
			g_pool->delayed_chunks[i] = tmp;
		}
		if (p != NULL) {
			r = find(g_pool, p);
			if (r == NULL) {
				wrterror("bogus pointer (double free?)");
				return;
			}
			free_bytes(g_pool, r, p);
		}
	}
}

void
_dl_free(void *ptr)
{
	/* This is legal. */
	if (ptr == NULL)
		return;

	malloc_func = "free():";
	if (g_pool == NULL) {
		wrterror("free() called before allocation");
		return;
	}
	if (malloc_active++) {
		malloc_recurse();
		return;
	}
	ofree(ptr);
	malloc_active--;
}


/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW	(1UL << (sizeof(size_t) * 4))

void *
_dl_calloc(size_t nmemb, size_t size)
{
	void *r;

	malloc_func = "calloc():";
	if (g_pool == NULL) {
		if (malloc_init() != 0)
			return NULL;
	}
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX / nmemb < size) {
		return NULL;
	}

	if (malloc_active++) {
		malloc_recurse();
		return NULL;
	}

	size *= nmemb;
	r = omalloc(size, 1);

	malloc_active--;
	return r;
}


static void *
orealloc(void *p, size_t newsz)
{
	struct region_info *r;
	void *q;
	size_t oldsz;

	q = omalloc(newsz, 0);
	if (p == NULL || q == NULL)
		return q;
	r = find(g_pool, p);
	if (r == NULL)
		wrterror("bogus pointer (double free?)");
	REALSIZE(oldsz, r);
	if (oldsz > MALLOC_MAXCHUNK) {
		if (oldsz < mopts.malloc_guard)
			wrterror("guard size");
		oldsz -= mopts.malloc_guard;
	}
	_dl_bcopy(p, q, oldsz < newsz ? oldsz : newsz);
	_dl_free(p);
	return q;
}


void *
_dl_realloc(void *ptr, size_t size)
{
	void *r;

	malloc_func = "realloc():";
	if (g_pool == NULL) {
		if (malloc_init() != 0)
			return NULL;
	}
	if (malloc_active++) {
		malloc_recurse();
		return NULL;
	}
	r = orealloc(ptr, size);
	malloc_active--;
	return r;
}

