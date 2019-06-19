/*      $OpenBSD: malloc.c,v 1.29 2018/11/02 07:26:25 otto Exp $       */
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
#include  "resolve.h"

#if defined(__mips64__)
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
#define MALLOC_INITIAL_REGIONS	(MALLOC_PAGESIZE / sizeof(struct region_info))
#define MALLOC_DEFAULT_CACHE	64
#define MALLOC_CHUNK_LISTS	4
#define CHUNK_CHECK_LENGTH	32

/*
 * We move allocations between half a page and a whole page towards the end,
 * subject to alignment constraints. This is the extra headroom we allow.
 * Set to zero to be the most strict.
 */
#define MALLOC_LEEWAY		0

#define PAGEROUND(x)  (((x) + (MALLOC_PAGEMASK)) & ~MALLOC_PAGEMASK)

/*
 * What to use for Junk.  This is the byte value we use to fill with
 * when the 'J' option is enabled. Use SOME_JUNK right after alloc,
 * and SOME_FREEJUNK right before free.
 */
#define SOME_JUNK		0xdb	/* deadbeef */
#define SOME_FREEJUNK		0xdf	/* dead, free */

#define MMAP(sz)	_dl_mmap(NULL, (size_t)(sz), PROT_READ | PROT_WRITE, \
    MAP_ANON | MAP_PRIVATE, -1, (off_t) 0)

#define MMAPNONE(sz)	_dl_mmap(NULL, (size_t)(sz), PROT_NONE, \
    MAP_ANON | MAP_PRIVATE, -1, (off_t) 0)

#define MMAP_ERROR(p)	(_dl_mmap_error(p) ? MAP_FAILED : (p))

struct region_info {
	void *p;		/* page; low bits used to mark chunks */
	uintptr_t size;		/* size for pages, or chunk_info pointer */
};

LIST_HEAD(chunk_head, chunk_info);

struct dir_info {
	u_int32_t canary1;
	int active;			/* status of malloc */
	struct region_info *r;		/* region slots */
	size_t regions_total;		/* number of region slots */
	size_t regions_free;		/* number of free slots */
					/* lists of free chunk info structs */
	struct chunk_head chunk_info_list[MALLOC_MAXSHIFT + 1];
					/* lists of chunks with free slots */
	struct chunk_head chunk_dir[MALLOC_MAXSHIFT + 1][MALLOC_CHUNK_LISTS];
	size_t free_regions_size;	/* free pages cached */
					/* free pages cache */
	u_int rotor;
	struct region_info free_regions[MALLOC_MAXCACHE];
					/* delayed free chunk slots */
	void *delayed_chunks[MALLOC_DELAYED_CHUNK_MASK + 1];
	size_t rbytesused;		/* random bytes used */
	char *func;			/* current function */
	u_char rbytes[256];		/* random bytes */
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
	u_short canary;
	u_short size;			/* size of this page's chunks */
	u_short shift;			/* how far to shift for this size */
	u_short free;			/* how many free chunks */
	u_short total;			/* how many chunk */
	u_short offset;			/* requested size table offset */
					/* which chunks are free */
	u_short bits[1];
};

#define MALLOC_FREEUNMAP	0
#define MALLOC_JUNK		1
#define CHUNK_CANARIES		1
#define MALLOC_GUARD		((size_t)MALLOC_PAGESIZE)
#define MALLOC_CACHE		MALLOC_DEFAULT_CACHE

struct malloc_readonly {
	struct dir_info *g_pool;	/* Main bookkeeping information */
	u_int32_t malloc_canary;	/* Matched against ones in g_pool */
};

/*
 * malloc configuration
 */
static struct malloc_readonly mopts __relro;

#define g_pool	mopts.g_pool

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

static __dead void
wrterror(char *msg)
{
	if (g_pool != NULL && g_pool->func != NULL)
		_dl_die("%s error: %s", g_pool->func, msg);
	else
		_dl_die("%s", msg);
}

static void
rbytes_init(struct dir_info *d)
{
	_dl_arc4randombuf(d->rbytes, sizeof(d->rbytes));
	/* add 1 to account for using d->rbytes[0] */
	d->rbytesused = 1 + d->rbytes[0] % (sizeof(d->rbytes) / 2);
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
 * Initialize the malloc subsystem before relro processing.
 */
void
_dl_malloc_init(void)
{
	char *p;
	int i, j;
	size_t d_avail, regioninfo_size, tmp;
	struct dir_info *d;

	do {
		_dl_arc4randombuf(&mopts.malloc_canary,
		    sizeof(mopts.malloc_canary));
	} while (mopts.malloc_canary == 0);

	/*
	 * Allocate dir_info with a guard page on either side. Also
	 * randomise offset inside the page at which the dir_info
	 * lies (subject to alignment by 1 << MALLOC_MINSHIFT)
	 */
	p = MMAPNONE(DIR_INFO_RSZ + (MALLOC_PAGESIZE * 2));
	p = MMAP_ERROR(p);
	if (p == MAP_FAILED)
		wrterror("malloc init mmap failed");
	_dl_mprotect(p + MALLOC_PAGESIZE, DIR_INFO_RSZ, PROT_READ | PROT_WRITE);
	d_avail = (DIR_INFO_RSZ - sizeof(*d)) >> MALLOC_MINSHIFT;

	_dl_arc4randombuf(&tmp, sizeof(tmp));
	d = (struct dir_info *)(p + MALLOC_PAGESIZE +
	    ((tmp % d_avail) << MALLOC_MINSHIFT)); /* not uniform */

	rbytes_init(d);
	d->regions_free = d->regions_total = MALLOC_INITIAL_REGIONS;
	regioninfo_size = d->regions_total * sizeof(struct region_info);
	d->r = MMAP(regioninfo_size);
	d->r = MMAP_ERROR(d->r);
	if (d->r == MAP_FAILED)
		wrterror("malloc init mmap failed");
	for (i = 0; i <= MALLOC_MAXSHIFT; i++) {
		LIST_INIT(&d->chunk_info_list[i]);
		for (j = 0; j < MALLOC_CHUNK_LISTS; j++)
			LIST_INIT(&d->chunk_dir[i][j]);
	}
	d->canary1 = mopts.malloc_canary ^ (u_int32_t)(uintptr_t)d;
	d->canary2 = ~d->canary1;

	g_pool = d;
}

static int
omalloc_grow(struct dir_info *d)
{
	size_t newtotal;
	size_t newsize;
	size_t mask;
	size_t i;
	struct region_info *p;

	if (d->regions_total > SIZE_MAX / sizeof(struct region_info) / 2)
		return 1;

	newtotal = d->regions_total * 2;
	newsize = newtotal * sizeof(struct region_info);
	mask = newtotal - 1;

	p = MMAP(newsize);
	p = MMAP_ERROR(p);
	if (p == MAP_FAILED)
		return 1;

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
 * Cache maintenance. We keep at most malloc_cache pages cached.
 * If the cache is becoming full, unmap pages in the cache for real,
 * and then add the region to the cache
 * Opposed to the regular region data structure, the sizes in the
 * cache are in MALLOC_PAGESIZE units.
 */
static void
unmap(struct dir_info *d, void *p, size_t sz, int junk)
{
	size_t psz = sz >> MALLOC_PAGESHIFT;
	size_t rsz;
	struct region_info *r;
	u_int i, offset, mask;

	if (sz != PAGEROUND(sz))
		wrterror("munmap round");

	rsz = MALLOC_CACHE - d->free_regions_size;

	if (psz > MALLOC_CACHE) {
		if (_dl_munmap(p, sz))
			wrterror("munmap");
		return;
	}
	offset = getrbyte(d);
	mask = MALLOC_CACHE - 1;
	if (psz > rsz) {
		size_t tounmap = psz - rsz;
		for (i = 0; ; i++) {
			r = &d->free_regions[(i + offset) & mask];
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
				if (tounmap == 0) {
					offset = i;
					break;
				}
			}
		}
	}
	for (i = 0; ; i++) {
		r = &d->free_regions[(i + offset) & mask];
		if (r->p == NULL) {
			if (junk && !MALLOC_FREEUNMAP) {
				size_t amt = junk == 1 ?  MALLOC_MAXCHUNK : sz;
				_dl_memset(p, SOME_FREEJUNK, amt);
			}
			if (MALLOC_FREEUNMAP)
				_dl_mprotect(p, sz, PROT_NONE);
			r->p = p;
			r->size = psz;
			d->free_regions_size += psz;
			break;
		}
	}
	if (d->free_regions_size > MALLOC_CACHE)
		wrterror("malloc cache overflow");
}

static void *
map(struct dir_info *d, size_t sz, int zero_fill)
{
	size_t psz = sz >> MALLOC_PAGESHIFT;
	struct region_info *r, *big = NULL;
	u_int i;
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
	for (i = 0; i < MALLOC_CACHE; i++) {
		r = &d->free_regions[(i + d->rotor) & (MALLOC_CACHE - 1)];
		if (r->p != NULL) {
			if (r->size == psz) {
				p = r->p;
				if (MALLOC_FREEUNMAP)
					_dl_mprotect(p, sz, PROT_READ | PROT_WRITE);
				r->p = NULL;
				d->free_regions_size -= psz;
				if (zero_fill)
					_dl_memset(p, 0, sz);
				else if (MALLOC_JUNK == 2 &&
				    MALLOC_FREEUNMAP)
					_dl_memset(p, SOME_FREEJUNK, sz);
				d->rotor += i + 1;
				return p;
			} else if (r->size > psz)
				big = r;
		}
	}
	if (big != NULL) {
		r = big;
		p = (char *)r->p + ((r->size - psz) << MALLOC_PAGESHIFT);
		if (MALLOC_FREEUNMAP)
			_dl_mprotect(p, sz, PROT_READ | PROT_WRITE);
		r->size -= psz;
		d->free_regions_size -= psz;
		if (zero_fill)
			_dl_memset(p, 0, sz);
		else if (MALLOC_JUNK == 2 && MALLOC_FREEUNMAP)
			_dl_memset(p, SOME_FREEJUNK, sz);
		return p;
	}
	p = MMAP(sz);
	p = MMAP_ERROR(p);
	if (d->free_regions_size > MALLOC_CACHE)
		wrterror("malloc cache");
	/* zero fill not needed */
	return p;
}

static void
init_chunk_info(struct dir_info *d, struct chunk_info *p, int bits)
{
	int i;

	if (bits == 0) {
		p->shift = MALLOC_MINSHIFT;
		p->total = p->free = MALLOC_PAGESIZE >> p->shift;
		p->size = 0;
		p->offset = 0xdead;
	} else {
		p->shift = bits;
		p->total = p->free = MALLOC_PAGESIZE >> p->shift;
		p->size = 1U << bits;
		p->offset = howmany(p->total, MALLOC_BITS);
	}
	p->canary = (u_short)d->canary1;

	/* set all valid bits in the bitmap */
	i = p->total - 1;
	_dl_memset(p->bits, 0xff, sizeof(p->bits[0]) * (i / MALLOC_BITS));
	p->bits[i / MALLOC_BITS] = (2U << (i % MALLOC_BITS)) - 1;
}

static struct chunk_info *
alloc_chunk_info(struct dir_info *d, int bits)
{
	struct chunk_info *p;

	if (LIST_EMPTY(&d->chunk_info_list[bits])) {
		size_t size, count, i;
		char *q;

		if (bits == 0)
			count = MALLOC_PAGESIZE / MALLOC_MINSIZE;
		else
			count = MALLOC_PAGESIZE >> bits;

		size = howmany(count, MALLOC_BITS);
		size = sizeof(struct chunk_info) + (size - 1) * sizeof(u_short);
		if (CHUNK_CANARIES)
			size += count * sizeof(u_short);
		size = ALIGN(size);

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
	if (p->shift == 0)
		init_chunk_info(d, p, bits);
	return p;
}

/*
 * Allocate a page of chunks
 */
static struct chunk_info *
omalloc_make_chunks(struct dir_info *d, int bits, int listnum)
{
	struct chunk_info *bp;
	void *pp;

	/* Allocate a new bucket */
	pp = map(d, MALLOC_PAGESIZE, 0);
	if (pp == MAP_FAILED)
		return NULL;

	bp = alloc_chunk_info(d, bits);
	if (bp == NULL)
		goto err;
	/* memory protect the page allocated in the malloc(0) case */
	if (bits == 0 && _dl_mprotect(pp, MALLOC_PAGESIZE, PROT_NONE) < 0)
		goto err;

	bp = alloc_chunk_info(d, bits);
	if (bp == NULL)
		goto err;
	bp->page = pp;

	if (insert(d, (void *)((uintptr_t)pp | (bits + 1)), (uintptr_t)bp))
		goto err;
	LIST_INSERT_HEAD(&d->chunk_dir[bits][listnum], bp, entries);
	return bp;

err:
	unmap(d, pp, MALLOC_PAGESIZE, MALLOC_JUNK);
	return NULL;
}

static int
find_chunksize(size_t size)
{
	int r;

	/* malloc(0) is special */
	if (size == 0)
		return 0;

	if (size < MALLOC_MINSIZE)
		size = MALLOC_MINSIZE;
	size--;

	r = MALLOC_MINSHIFT;
	while (size >> r)
		r++;
	return r;
}

static void
fill_canary(char *ptr, size_t sz, size_t allocated)
{
	size_t check_sz = allocated - sz;

	if (check_sz > CHUNK_CHECK_LENGTH)
		check_sz = CHUNK_CHECK_LENGTH;
	_dl_memset(ptr + sz, SOME_JUNK, check_sz);
}

/*
 * Allocate a chunk
 */
static void *
malloc_bytes(struct dir_info *d, size_t size)
{
	u_int i, r;
	int j, listnum;
	size_t k;
	u_short	*lp;
	struct chunk_info *bp;
	void *p;

	if (mopts.malloc_canary != (d->canary1 ^ (u_int32_t)(uintptr_t)d) ||
	    d->canary1 != ~d->canary2)
		wrterror("internal struct corrupt");

	j = find_chunksize(size);

	r = ((u_int)getrbyte(d) << 8) | getrbyte(d);
	listnum = r % MALLOC_CHUNK_LISTS;
	/* If it's empty, make a page more of that size chunks */
	if ((bp = LIST_FIRST(&d->chunk_dir[j][listnum])) == NULL) {
		bp = omalloc_make_chunks(d, j, listnum);
		if (bp == NULL)
			return NULL;
	}

	if (bp->canary != (u_short)d->canary1)
		wrterror("chunk info corrupted");

	i = (r / MALLOC_CHUNK_LISTS) & (bp->total - 1);

	/* start somewhere in a short */
	lp = &bp->bits[i / MALLOC_BITS];
	if (*lp) {
		j = i % MALLOC_BITS;
		k = __builtin_ffs(*lp >> j);
		if (k != 0) {
			k += j - 1;
			goto found;
		}
	}
	/* no bit halfway, go to next full short */
	i /= MALLOC_BITS;
	for (;;) {
		if (++i >= bp->total / MALLOC_BITS)
			i = 0;
		lp = &bp->bits[i];
		if (*lp) {
			k = __builtin_ffs(*lp) - 1;
			break;
		}
	}
found:
	*lp ^= 1 << k;

	/* If there are no more free, remove from free-list */
	if (--bp->free == 0)
		LIST_REMOVE(bp, entries);

	/* Adjust to the real offset of that chunk */
	k += (lp - bp->bits) * MALLOC_BITS;

	if (CHUNK_CANARIES && size > 0)
		bp->bits[bp->offset + k] = size;

	k <<= bp->shift;

	p = (char *)bp->page + k;
	if (bp->size > 0) {
		if (MALLOC_JUNK == 2)
			_dl_memset(p, SOME_JUNK, bp->size);
		else if (CHUNK_CANARIES)
			fill_canary(p, size, bp->size);
	}
	return p;
}

static void
validate_canary(u_char *ptr, size_t sz, size_t allocated)
{
	size_t check_sz = allocated - sz;
	u_char *p, *q;

	if (check_sz > CHUNK_CHECK_LENGTH)
		check_sz = CHUNK_CHECK_LENGTH;
	p = ptr + sz;
	q = p + check_sz;

	while (p < q)
		if (*p++ != SOME_JUNK)
			wrterror("chunk canary corrupted");
}

static uint32_t
find_chunknum(struct dir_info *d, struct region_info *r, void *ptr, int check)
{
	struct chunk_info *info;
	uint32_t chunknum;

	info = (struct chunk_info *)r->size;
	if (info->canary != (u_short)d->canary1)
		wrterror("chunk info corrupted");

	/* Find the chunk number on the page */
	chunknum = ((uintptr_t)ptr & MALLOC_PAGEMASK) >> info->shift;
	if (check && info->size > 0) {
		validate_canary(ptr, info->bits[info->offset + chunknum],
		    info->size);
	}

	if ((uintptr_t)ptr & ((1U << (info->shift)) - 1)) {
		wrterror("modified chunk-pointer");
		return -1;
	}
	if (info->bits[chunknum / MALLOC_BITS] &
	    (1U << (chunknum % MALLOC_BITS)))
		wrterror("chunk is already free");
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
	chunknum = find_chunknum(d, r, ptr, 0);

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

	if (info->size == 0 && !MALLOC_FREEUNMAP)
		_dl_mprotect(info->page, MALLOC_PAGESIZE, PROT_READ | PROT_WRITE);
	unmap(d, info->page, MALLOC_PAGESIZE, 0);

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
		if (sz >= SIZE_MAX - MALLOC_GUARD - MALLOC_PAGESIZE) {
			return NULL;
		}
		sz += MALLOC_GUARD;
		psz = PAGEROUND(sz);
		p = map(g_pool, psz, zero_fill);
		if (p == MAP_FAILED) {
			return NULL;
		}
		if (insert(g_pool, p, sz)) {
			unmap(g_pool, p, psz, 0);
			return NULL;
		}
		if (MALLOC_GUARD) {
			if (_dl_mprotect((char *)p + psz - MALLOC_GUARD,
			    MALLOC_GUARD, PROT_NONE))
				wrterror("mprotect");
		}

		if (sz - MALLOC_GUARD < MALLOC_PAGESIZE - MALLOC_LEEWAY) {
			/* fill whole allocation */
			if (MALLOC_JUNK == 2)
				_dl_memset(p, SOME_JUNK, psz - MALLOC_GUARD);
			/* shift towards the end */
			p = ((char *)p) + ((MALLOC_PAGESIZE - MALLOC_LEEWAY -
			    (sz - MALLOC_GUARD)) & ~(MALLOC_MINSIZE-1));
			/* fill zeros if needed and overwritten above */
			if (zero_fill && MALLOC_JUNK == 2)
				_dl_memset(p, 0, sz - MALLOC_GUARD);
		} else {
			if (MALLOC_JUNK == 2) {
				if (zero_fill)
					_dl_memset((char *)p + sz - MALLOC_GUARD,
					    SOME_JUNK, psz - sz);
				else
					_dl_memset(p, SOME_JUNK,
					    psz - MALLOC_GUARD);
			} else if (CHUNK_CANARIES)
				fill_canary(p, sz - MALLOC_GUARD,
				    psz - MALLOC_GUARD);
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
	g_pool->active--;
}

void *
_dl_malloc(size_t size)
{
	void *r = NULL;
	lock_cb *cb;

	cb = _dl_thread_kern_stop();
	g_pool->func = "malloc():";
	if (g_pool->active++) {
		malloc_recurse();
		goto ret;
	}
	r = omalloc(size, 0);
	g_pool->active--;
ret:
	_dl_thread_kern_go(cb);
	return r;
}

static void
validate_junk(struct dir_info *pool, void *p)
{
	struct region_info *r;
	size_t byte, sz;

	if (p == NULL)
		return;
	r = find(pool, p);
	if (r == NULL)
		wrterror("bogus pointer in validate_junk");
	REALSIZE(sz, r);
	if (sz > CHUNK_CHECK_LENGTH)
		sz = CHUNK_CHECK_LENGTH;
	for (byte = 0; byte < sz; byte++) {
		if (((unsigned char *)p)[byte] != SOME_FREEJUNK)
			wrterror("use after free");
	}
}

static void
ofree(void *p)
{
	struct region_info *r;
	size_t sz;

	r = find(g_pool, p);
	if (r == NULL)
		wrterror("bogus pointer (double free?)");
	REALSIZE(sz, r);
	if (sz > MALLOC_MAXCHUNK) {
		if (sz - MALLOC_GUARD >= MALLOC_PAGESIZE -
		    MALLOC_LEEWAY) {
			if (r->p != p)
				wrterror("bogus pointer");
			if (CHUNK_CANARIES)
				validate_canary(p,
				    sz - MALLOC_GUARD,
				    PAGEROUND(sz - MALLOC_GUARD));
		} else {
#if notyetbecause_of_realloc
			/* shifted towards the end */
			if (p != ((char *)r->p) + ((MALLOC_PAGESIZE -
			    MALLOC_MINSIZE - sz - MALLOC_GUARD) &
			    ~(MALLOC_MINSIZE-1))) {
			}
#endif
			p = r->p;
		}
		if (MALLOC_GUARD) {
			if (sz < MALLOC_GUARD)
				wrterror("guard size");
			if (!MALLOC_FREEUNMAP) {
				if (_dl_mprotect((char *)p + PAGEROUND(sz) -
				    MALLOC_GUARD, MALLOC_GUARD,
				    PROT_READ | PROT_WRITE))
					wrterror("mprotect");
			}
		}
		unmap(g_pool, p, PAGEROUND(sz), MALLOC_JUNK);
		delete(g_pool, r);
	} else {
		void *tmp;
		int i;

		find_chunknum(g_pool, r, p, CHUNK_CANARIES);
		for (i = 0; i <= MALLOC_DELAYED_CHUNK_MASK; i++) {
			if (p == g_pool->delayed_chunks[i])
				wrterror("double free");
		}
		if (MALLOC_JUNK && sz > 0)
			_dl_memset(p, SOME_FREEJUNK, sz);
		i = getrbyte(g_pool) & MALLOC_DELAYED_CHUNK_MASK;
		tmp = p;
		p = g_pool->delayed_chunks[i];
		g_pool->delayed_chunks[i] = tmp;
		if (MALLOC_JUNK)
			validate_junk(g_pool, p);
		if (p != NULL) {
			r = find(g_pool, p);
			if (r == NULL)
				wrterror("bogus pointer (double free?)");
			free_bytes(g_pool, r, p);
		}
	}
}

void
_dl_free(void *ptr)
{
	lock_cb *cb;

	/* This is legal. */
	if (ptr == NULL)
		return;

	cb = _dl_thread_kern_stop();
	if (g_pool == NULL)
		wrterror("free() called before allocation");
	g_pool->func = "free():";
	if (g_pool->active++) {
		malloc_recurse();
		goto ret;
	}
	ofree(ptr);
	g_pool->active--;
ret:
	_dl_thread_kern_go(cb);
}


/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW	(1UL << (sizeof(size_t) * 4))

void *
_dl_calloc(size_t nmemb, size_t size)
{
	void *r = NULL;
	lock_cb *cb;

	cb = _dl_thread_kern_stop();
	g_pool->func = "calloc():";
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX / nmemb < size) {
		goto ret;
	}

	if (g_pool->active++) {
		malloc_recurse();
		goto ret;
	}

	size *= nmemb;
	r = omalloc(size, 1);
	g_pool->active--;
ret:
	_dl_thread_kern_go(cb);
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
		if (oldsz < MALLOC_GUARD)
			wrterror("guard size");
		oldsz -= MALLOC_GUARD;
	}
	_dl_bcopy(p, q, oldsz < newsz ? oldsz : newsz);
	_dl_free(p);
	return q;
}


void *
_dl_realloc(void *ptr, size_t size)
{
	void *r = NULL;
	lock_cb *cb;

	cb = _dl_thread_kern_stop();
	g_pool->func = "realloc():";
	if (g_pool->active++) {
		malloc_recurse();
		goto ret;
	}
	r = orealloc(ptr, size);
	g_pool->active--;
ret:
	_dl_thread_kern_go(cb);
	return r;
}

static void *
mapalign(struct dir_info *d, size_t alignment, size_t sz, int zero_fill)
{
	char *p, *q;

	if (alignment < MALLOC_PAGESIZE || ((alignment - 1) & alignment) != 0)
		wrterror("mapalign bad alignment");
	if (sz != PAGEROUND(sz))
		wrterror("mapalign round");

	/* Allocate sz + alignment bytes of memory, which must include a
	 * subrange of size bytes that is properly aligned.  Unmap the
	 * other bytes, and then return that subrange.
	 */

	/* We need sz + alignment to fit into a size_t. */
	if (alignment > SIZE_MAX - sz)
		return MAP_FAILED;

	p = map(d, sz + alignment, zero_fill);
	if (p == MAP_FAILED)
		return MAP_FAILED;
	q = (char *)(((uintptr_t)p + alignment - 1) & ~(alignment - 1));
	if (q != p) {
		if (_dl_munmap(p, q - p))
			wrterror("munmap");
	}
	if (_dl_munmap(q + sz, alignment - (q - p)))
		wrterror("munmap");

	return q;
}

static void *
omemalign(size_t alignment, size_t sz, int zero_fill)
{
	size_t psz;
	void *p;

	/* If between half a page and a page, avoid MALLOC_MOVE. */
	if (sz > MALLOC_MAXCHUNK && sz < MALLOC_PAGESIZE)
		sz = MALLOC_PAGESIZE;
	if (alignment <= MALLOC_PAGESIZE) {
		/*
		 * max(size, alignment) is enough to assure the requested
		 * alignment, since the allocator always allocates
		 * power-of-two blocks.
		 */
		if (sz < alignment)
			sz = alignment;
		return omalloc(sz, zero_fill);
	}

	if (sz >= SIZE_MAX - MALLOC_GUARD - MALLOC_PAGESIZE) {
		return NULL;
	}

	sz += MALLOC_GUARD;
	psz = PAGEROUND(sz);

	p = mapalign(g_pool, alignment, psz, zero_fill);
	if (p == MAP_FAILED) {
		return NULL;
	}

	if (insert(g_pool, p, sz)) {
		unmap(g_pool, p, psz, 0);
		return NULL;
	}

	if (MALLOC_GUARD) {
		if (_dl_mprotect((char *)p + psz - MALLOC_GUARD,
		    MALLOC_GUARD, PROT_NONE))
			wrterror("mprotect");
	}

	if (MALLOC_JUNK == 2) {
		if (zero_fill)
			_dl_memset((char *)p + sz - MALLOC_GUARD,
			    SOME_JUNK, psz - sz);
		else
			_dl_memset(p, SOME_JUNK, psz - MALLOC_GUARD);
	} else if (CHUNK_CANARIES)
		fill_canary(p, sz - MALLOC_GUARD,
		    psz - MALLOC_GUARD);

	return p;
}

void *
_dl_aligned_alloc(size_t alignment, size_t size)
{
	void *r = NULL;
	lock_cb *cb;

	/* Make sure that alignment is a large enough power of 2. */
	if (((alignment - 1) & alignment) != 0 || alignment < sizeof(void *))
		return NULL;

	cb = _dl_thread_kern_stop();
	g_pool->func = "aligned_alloc():";
	if (g_pool->active++) {
		malloc_recurse();
		goto ret;
	}
	r = omemalign(alignment, size, 0);
	g_pool->active--;
ret:
	_dl_thread_kern_go(cb);
	return r;
}
