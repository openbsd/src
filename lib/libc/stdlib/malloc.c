/*	$OpenBSD: malloc.c,v 1.174 2015/04/06 09:18:51 tedu Exp $	*/
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

/* #define MALLOC_STATS */

#include <sys/types.h>
#include <sys/param.h>	/* PAGE_SHIFT ALIGN */
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#ifdef MALLOC_STATS
#include <sys/tree.h>
#include <fcntl.h>
#endif

#include "thread_private.h"

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

#define MMAP(sz)	mmap(NULL, (size_t)(sz), PROT_READ | PROT_WRITE, \
    MAP_ANON | MAP_PRIVATE, -1, (off_t) 0)

#define MMAPA(a,sz)	mmap((a), (size_t)(sz), PROT_READ | PROT_WRITE, \
    MAP_ANON | MAP_PRIVATE, -1, (off_t) 0)

#define MQUERY(a, sz)	mquery((a), (size_t)(sz), PROT_READ | PROT_WRITE, \
    MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, (off_t)0)

#define _MALLOC_LEAVE() if (__isthreaded) do { \
	malloc_active--; \
	_MALLOC_UNLOCK(); \
} while (0)
#define _MALLOC_ENTER() if (__isthreaded) do { \
	_MALLOC_LOCK(); \
	malloc_active++; \
} while (0)

struct region_info {
	void *p;		/* page; low bits used to mark chunks */
	uintptr_t size;		/* size for pages, or chunk_info pointer */
#ifdef MALLOC_STATS
	void *f;		/* where allocated from */
#endif
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
	u_char rbytes[32];		/* random bytes */
	u_short chunk_start;
#ifdef MALLOC_STATS
	size_t inserts;
	size_t insert_collisions;
	size_t finds;
	size_t find_collisions;
	size_t deletes;
	size_t delete_moves;
	size_t cheap_realloc_tries;
	size_t cheap_reallocs;
	size_t malloc_used;		/* bytes allocated */
	size_t malloc_guarded;		/* bytes used for guards */
#define STATS_ADD(x,y)	((x) += (y))
#define STATS_SUB(x,y)	((x) -= (y))
#define STATS_INC(x)	((x)++)
#define STATS_ZERO(x)	((x) = 0)
#define STATS_SETF(x,y)	((x)->f = (y))
#else
#define STATS_ADD(x,y)	/* nothing */
#define STATS_SUB(x,y)	/* nothing */
#define STATS_INC(x)	/* nothing */
#define STATS_ZERO(x)	/* nothing */
#define STATS_SETF(x,y)	/* nothing */
#endif /* MALLOC_STATS */
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
	struct dir_info *malloc_pool;	/* Main bookkeeping information */
	int	malloc_abort;		/* abort() on error */
	int	malloc_freenow;		/* Free quickly - disable chunk rnd */
	int	malloc_freeunmap;	/* mprotect free pages PROT_NONE? */
	int	malloc_hint;		/* call madvice on free pages?  */
	int	malloc_junk;		/* junk fill? */
	int	malloc_move;		/* move allocations to end of page? */
	int	malloc_realloc;		/* always realloc? */
	int	malloc_xmalloc;		/* xmalloc behaviour? */
	size_t	malloc_guard;		/* use guard pages after allocations? */
	u_int	malloc_cache;		/* free pages we cache */
#ifdef MALLOC_STATS
	int	malloc_stats;		/* dump statistics at end */
#endif
	u_int32_t malloc_canary;	/* Matched against ones in malloc_pool */
};

/* This object is mapped PROT_READ after initialisation to prevent tampering */
static union {
	struct malloc_readonly mopts;
	u_char _pad[MALLOC_PAGESIZE];
} malloc_readonly __attribute__((aligned(MALLOC_PAGESIZE)));
#define mopts	malloc_readonly.mopts
#define getpool() mopts.malloc_pool

char		*malloc_options;	/* compile-time options */
static char	*malloc_func;		/* current function */
static int	malloc_active;		/* status of malloc */

static u_char getrbyte(struct dir_info *d);

extern char	*__progname;

#ifdef MALLOC_STATS
void malloc_dump(int);
static void malloc_exit(void);
#define CALLER	__builtin_return_address(0)
#else
#define CALLER	NULL
#endif

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
wrterror(char *msg, void *p)
{
	char		*q = " error: ";
	struct iovec	iov[7];
	char		pidbuf[20];
	char		buf[20];
	int		saved_errno = errno;

	iov[0].iov_base = __progname;
	iov[0].iov_len = strlen(__progname);
	iov[1].iov_base = pidbuf;
	snprintf(pidbuf, sizeof(pidbuf), "(%d) in ", getpid());
	iov[1].iov_len = strlen(pidbuf);
	iov[2].iov_base = malloc_func;
	iov[2].iov_len = strlen(malloc_func);
	iov[3].iov_base = q;
	iov[3].iov_len = strlen(q);
	iov[4].iov_base = msg;
	iov[4].iov_len = strlen(msg);
	iov[5].iov_base = buf;
	if (p == NULL)
		iov[5].iov_len = 0;
	else {
		snprintf(buf, sizeof(buf), " %p", p);
		iov[5].iov_len = strlen(buf);
	}
	iov[6].iov_base = "\n";
	iov[6].iov_len = 1;
	writev(STDERR_FILENO, iov, 7);

#ifdef MALLOC_STATS
	if (mopts.malloc_stats)
		malloc_dump(STDERR_FILENO);
#endif /* MALLOC_STATS */

	errno = saved_errno;
	if (mopts.malloc_abort)
		abort();
}

static void
rbytes_init(struct dir_info *d)
{
	arc4random_buf(d->rbytes, sizeof(d->rbytes));
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
		wrterror("munmap round", NULL);
		return;
	}

	if (psz > mopts.malloc_cache) {
		i = munmap(p, sz);
		if (i)
			wrterror("munmap", p);
		STATS_SUB(d->malloc_used, sz);
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
			if (munmap(r->p, rsz))
				wrterror("munmap", r->p);
			r->p = NULL;
			if (tounmap > r->size)
				tounmap -= r->size;
			else
				tounmap = 0;
			d->free_regions_size -= r->size;
			r->size = 0;
			STATS_SUB(d->malloc_used, rsz);
		}
	}
	if (tounmap > 0)
		wrterror("malloc cache underflow", NULL);
	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[(i + offset) & (mopts.malloc_cache - 1)];
		if (r->p == NULL) {
			if (mopts.malloc_hint)
				madvise(p, sz, MADV_FREE);
			if (mopts.malloc_freeunmap)
				mprotect(p, sz, PROT_NONE);
			r->p = p;
			r->size = psz;
			d->free_regions_size += psz;
			break;
		}
	}
	if (i == mopts.malloc_cache)
		wrterror("malloc free slot lost", NULL);
	if (d->free_regions_size > mopts.malloc_cache)
		wrterror("malloc cache overflow", NULL);
}

static void
zapcacheregion(struct dir_info *d, void *p, size_t len)
{
	u_int i;
	struct region_info *r;
	size_t rsz;

	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[i];
		if (r->p >= p && r->p <= (void *)((char *)p + len)) {
			rsz = r->size << MALLOC_PAGESHIFT;
			if (munmap(r->p, rsz))
				wrterror("munmap", r->p);
			r->p = NULL;
			d->free_regions_size -= r->size;
			r->size = 0;
			STATS_SUB(d->malloc_used, rsz);
		}
	}
}

static void *
map(struct dir_info *d, void *hint, size_t sz, int zero_fill)
{
	size_t psz = sz >> MALLOC_PAGESHIFT;
	struct region_info *r, *big = NULL;
	u_int i, offset;
	void *p;

	if (mopts.malloc_canary != (d->canary1 ^ (u_int32_t)(uintptr_t)d) ||
	    d->canary1 != ~d->canary2)
		wrterror("internal struct corrupt", NULL);
	if (sz != PAGEROUND(sz)) {
		wrterror("map round", NULL);
		return MAP_FAILED;
	}
	if (!hint && psz > d->free_regions_size) {
		_MALLOC_LEAVE();
		p = MMAP(sz);
		_MALLOC_ENTER();
		if (p != MAP_FAILED)
			STATS_ADD(d->malloc_used, sz);
		/* zero fill not needed */
		return p;
	}
	offset = getrbyte(d);
	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[(i + offset) & (mopts.malloc_cache - 1)];
		if (r->p != NULL) {
			if (hint && r->p != hint)
				continue;
			if (r->size == psz) {
				p = r->p;
				r->p = NULL;
				r->size = 0;
				d->free_regions_size -= psz;
				if (mopts.malloc_freeunmap)
					mprotect(p, sz, PROT_READ | PROT_WRITE);
				if (mopts.malloc_hint)
					madvise(p, sz, MADV_NORMAL);
				if (zero_fill)
					memset(p, 0, sz);
				else if (mopts.malloc_junk == 2 &&
				    mopts.malloc_freeunmap)
					memset(p, SOME_FREEJUNK, sz);
				return p;
			} else if (r->size > psz)
				big = r;
		}
	}
	if (big != NULL) {
		r = big;
		p = r->p;
		r->p = (char *)r->p + (psz << MALLOC_PAGESHIFT);
		if (mopts.malloc_freeunmap)
			mprotect(p, sz, PROT_READ | PROT_WRITE);
		if (mopts.malloc_hint)
			madvise(p, sz, MADV_NORMAL);
		r->size -= psz;
		d->free_regions_size -= psz;
		if (zero_fill)
			memset(p, 0, sz);
		else if (mopts.malloc_junk == 2 && mopts.malloc_freeunmap)
			memset(p, SOME_FREEJUNK, sz);
		return p;
	}
	if (hint)
		return MAP_FAILED;
	if (d->free_regions_size > mopts.malloc_cache)
		wrterror("malloc cache", NULL);
	_MALLOC_LEAVE();
	p = MMAP(sz);
	_MALLOC_ENTER();
	if (p != MAP_FAILED)
		STATS_ADD(d->malloc_used, sz);
	/* zero fill not needed */
	return p;
}

/*
 * Initialize a dir_info, which should have been cleared by caller
 */
static int
omalloc_init(struct dir_info **dp)
{
	char *p, b[64];
	int i, j;
	size_t d_avail, regioninfo_size;
	struct dir_info *d;

	/*
	 * Default options
	 */
	mopts.malloc_abort = 1;
	mopts.malloc_junk = 1;
	mopts.malloc_move = 1;
	mopts.malloc_cache = MALLOC_DEFAULT_CACHE;

	for (i = 0; i < 3; i++) {
		switch (i) {
		case 0:
			j = readlink("/etc/malloc.conf", b, sizeof b - 1);
			if (j <= 0)
				continue;
			b[j] = '\0';
			p = b;
			break;
		case 1:
			if (issetugid() == 0)
				p = getenv("MALLOC_OPTIONS");
			else
				continue;
			break;
		case 2:
			p = malloc_options;
			break;
		default:
			p = NULL;
		}

		for (; p != NULL && *p != '\0'; p++) {
			switch (*p) {
			case '>':
				mopts.malloc_cache <<= 1;
				if (mopts.malloc_cache > MALLOC_MAXCACHE)
					mopts.malloc_cache = MALLOC_MAXCACHE;
				break;
			case '<':
				mopts.malloc_cache >>= 1;
				break;
			case 'a':
				mopts.malloc_abort = 0;
				break;
			case 'A':
				mopts.malloc_abort = 1;
				break;
#ifdef MALLOC_STATS
			case 'd':
				mopts.malloc_stats = 0;
				break;
			case 'D':
				mopts.malloc_stats = 1;
				break;
#endif /* MALLOC_STATS */
			case 'f':
				mopts.malloc_freenow = 0;
				mopts.malloc_freeunmap = 0;
				break;
			case 'F':
				mopts.malloc_freenow = 1;
				mopts.malloc_freeunmap = 1;
				break;
			case 'g':
				mopts.malloc_guard = 0;
				break;
			case 'G':
				mopts.malloc_guard = MALLOC_PAGESIZE;
				break;
			case 'h':
				mopts.malloc_hint = 0;
				break;
			case 'H':
				mopts.malloc_hint = 1;
				break;
			case 'j':
				mopts.malloc_junk = 0;
				break;
			case 'J':
				mopts.malloc_junk = 2;
				break;
			case 'n':
			case 'N':
				break;
			case 'p':
				mopts.malloc_move = 0;
				break;
			case 'P':
				mopts.malloc_move = 1;
				break;
			case 'r':
				mopts.malloc_realloc = 0;
				break;
			case 'R':
				mopts.malloc_realloc = 1;
				break;
			case 's':
				mopts.malloc_freeunmap = mopts.malloc_junk = 0;
				mopts.malloc_guard = 0;
				mopts.malloc_cache = MALLOC_DEFAULT_CACHE;
				break;
			case 'S':
				mopts.malloc_freeunmap = 1;
				mopts.malloc_junk = 2;
				mopts.malloc_guard = MALLOC_PAGESIZE;
				mopts.malloc_cache = 0;
				break;
			case 'u':
				mopts.malloc_freeunmap = 0;
				break;
			case 'U':
				mopts.malloc_freeunmap = 1;
				break;
			case 'x':
				mopts.malloc_xmalloc = 0;
				break;
			case 'X':
				mopts.malloc_xmalloc = 1;
				break;
			default: {
				static const char q[] = "malloc() warning: "
				    "unknown char in MALLOC_OPTIONS\n";
				write(STDERR_FILENO, q, sizeof(q) - 1);
				break;
			}
			}
		}
	}

#ifdef MALLOC_STATS
	if (mopts.malloc_stats && (atexit(malloc_exit) == -1)) {
		static const char q[] = "malloc() warning: atexit(2) failed."
		    " Will not be able to dump stats on exit\n";
		write(STDERR_FILENO, q, sizeof(q) - 1);
	}
#endif /* MALLOC_STATS */

	while ((mopts.malloc_canary = arc4random()) == 0)
		;

	/*
	 * Allocate dir_info with a guard page on either side. Also
	 * randomise offset inside the page at which the dir_info
	 * lies (subject to alignment by 1 << MALLOC_MINSHIFT)
	 */
	if ((p = MMAP(DIR_INFO_RSZ + (MALLOC_PAGESIZE * 2))) == MAP_FAILED)
		return -1;
	mprotect(p, MALLOC_PAGESIZE, PROT_NONE);
	mprotect(p + MALLOC_PAGESIZE + DIR_INFO_RSZ,
	    MALLOC_PAGESIZE, PROT_NONE);
	d_avail = (DIR_INFO_RSZ - sizeof(*d)) >> MALLOC_MINSHIFT;
	d = (struct dir_info *)(p + MALLOC_PAGESIZE +
	    (arc4random_uniform(d_avail) << MALLOC_MINSHIFT));

	rbytes_init(d);
	d->regions_free = d->regions_total = MALLOC_INITIAL_REGIONS;
	regioninfo_size = d->regions_total * sizeof(struct region_info);
	d->r = MMAP(regioninfo_size);
	if (d->r == MAP_FAILED) {
		wrterror("malloc init mmap failed", NULL);
		d->regions_total = 0;
		return 1;
	}
	for (i = 0; i <= MALLOC_MAXSHIFT; i++) {
		LIST_INIT(&d->chunk_info_list[i]);
		for (j = 0; j < MALLOC_CHUNK_LISTS; j++)
			LIST_INIT(&d->chunk_dir[i][j]);
	}
	STATS_ADD(d->malloc_used, regioninfo_size);
	d->canary1 = mopts.malloc_canary ^ (u_int32_t)(uintptr_t)d;
	d->canary2 = ~d->canary1;

	*dp = d;

	/*
	 * Options have been set and will never be reset.
	 * Prevent further tampering with them.
	 */
	if (((uintptr_t)&malloc_readonly & MALLOC_PAGEMASK) == 0)
		mprotect(&malloc_readonly, sizeof(malloc_readonly), PROT_READ);

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
	if (p == MAP_FAILED)
		return 1;

	STATS_ADD(d->malloc_used, newsize);
	memset(p, 0, newsize);
	STATS_ZERO(d->inserts);
	STATS_ZERO(d->insert_collisions);
	for (i = 0; i < d->regions_total; i++) {
		void *q = d->r[i].p;
		if (q != NULL) {
			size_t index = hash(q) & mask;
			STATS_INC(d->inserts);
			while (p[index].p != NULL) {
				index = (index - 1) & mask;
				STATS_INC(d->insert_collisions);
			}
			p[index] = d->r[i];
		}
	}
	/* avoid pages containing meta info to end up in cache */
	if (munmap(d->r, d->regions_total * sizeof(struct region_info)))
		wrterror("munmap", d->r);
	else
		STATS_SUB(d->malloc_used,
		    d->regions_total * sizeof(struct region_info));
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
		if (q == MAP_FAILED)
			return NULL;
		STATS_ADD(d->malloc_used, MALLOC_PAGESIZE);
		count = MALLOC_PAGESIZE / size;
		for (i = 0; i < count; i++, q += size)
			LIST_INSERT_HEAD(&d->chunk_info_list[bits],
			    (struct chunk_info *)q, entries);
	}
	p = LIST_FIRST(&d->chunk_info_list[bits]);
	LIST_REMOVE(p, entries);
	memset(p, 0, size);
	p->canary = d->canary1;
	return p;
}


/*
 * The hashtable uses the assumption that p is never NULL. This holds since
 * non-MAP_FIXED mappings with hint 0 start at BRKSIZ.
 */
static int
insert(struct dir_info *d, void *p, size_t sz, void *f)
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
	STATS_INC(d->inserts);
	while (q != NULL) {
		index = (index - 1) & mask;
		q = d->r[index].p;
		STATS_INC(d->insert_collisions);
	}
	d->r[index].p = p;
	d->r[index].size = sz;
#ifdef MALLOC_STATS
	d->r[index].f = f;
#endif
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
		wrterror("internal struct corrupt", NULL);
	p = MASK_POINTER(p);
	index = hash(p) & mask;
	r = d->r[index].p;
	q = MASK_POINTER(r);
	STATS_INC(d->finds);
	while (q != p && r != NULL) {
		index = (index - 1) & mask;
		r = d->r[index].p;
		q = MASK_POINTER(r);
		STATS_INC(d->find_collisions);
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
		wrterror("regions_total not 2^x", NULL);
	d->regions_free++;
	STATS_INC(getpool()->deletes);

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
			STATS_INC(getpool()->delete_moves);
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
	pp = map(d, NULL, MALLOC_PAGESIZE, 0);
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

		k = mprotect(pp, MALLOC_PAGESIZE, PROT_NONE);
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
		wrterror("pp & bits", pp);

	insert(d, (void *)((uintptr_t)pp | bits), (uintptr_t)bp, NULL);
	return bp;
}


/*
 * Allocate a chunk
 */
static void *
malloc_bytes(struct dir_info *d, size_t size, void *f)
{
	int		i, j, listnum;
	size_t		k;
	u_short		u, *lp;
	struct chunk_info *bp;

	if (mopts.malloc_canary != (d->canary1 ^ (u_int32_t)(uintptr_t)d) ||
	    d->canary1 != ~d->canary2)
		wrterror("internal struct corrupt", NULL);
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
		wrterror("chunk info corrupted", NULL);

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
#ifdef MALLOC_STATS
	if (i == 0) {
		struct region_info *r = find(d, bp->page);
		r->f = f;
	}
#endif

	*lp ^= u;

	/* If there are no more free, remove from free-list */
	if (!--bp->free)
		LIST_REMOVE(bp, entries);

	/* Adjust to the real offset of that chunk */
	k += (lp - bp->bits) * MALLOC_BITS;
	k <<= bp->shift;

	if (mopts.malloc_junk == 2 && bp->size > 0)
		memset((char *)bp->page + k, SOME_JUNK, bp->size);
	return ((char *)bp->page + k);
}

static uint32_t
find_chunknum(struct dir_info *d, struct region_info *r, void *ptr)
{
	struct chunk_info *info;
	uint32_t chunknum;

	info = (struct chunk_info *)r->size;
	if (info->canary != d->canary1)
		wrterror("chunk info corrupted", NULL);

	/* Find the chunk number on the page */
	chunknum = ((uintptr_t)ptr & MALLOC_PAGEMASK) >> info->shift;

	if ((uintptr_t)ptr & ((1U << (info->shift)) - 1)) {
		wrterror("modified chunk-pointer", ptr);
		return -1;
	}
	if (info->bits[chunknum / MALLOC_BITS] &
	    (1U << (chunknum % MALLOC_BITS))) {
		wrterror("chunk is already free", ptr);
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
		mprotect(info->page, MALLOC_PAGESIZE, PROT_READ | PROT_WRITE);
	unmap(d, info->page, MALLOC_PAGESIZE);

	delete(d, r);
	if (info->size != 0)
		mp = &d->chunk_info_list[info->shift];
	else
		mp = &d->chunk_info_list[0];
	LIST_INSERT_HEAD(mp, info, entries);
}



static void *
omalloc(size_t sz, int zero_fill, void *f)
{
	struct dir_info *pool = getpool();
	void *p;
	size_t psz;

	if (sz > MALLOC_MAXCHUNK) {
		if (sz >= SIZE_MAX - mopts.malloc_guard - MALLOC_PAGESIZE) {
			errno = ENOMEM;
			return NULL;
		}
		sz += mopts.malloc_guard;
		psz = PAGEROUND(sz);
		p = map(pool, NULL, psz, zero_fill);
		if (p == MAP_FAILED) {
			errno = ENOMEM;
			return NULL;
		}
		if (insert(pool, p, sz, f)) {
			unmap(pool, p, psz);
			errno = ENOMEM;
			return NULL;
		}
		if (mopts.malloc_guard) {
			if (mprotect((char *)p + psz - mopts.malloc_guard,
			    mopts.malloc_guard, PROT_NONE))
				wrterror("mprotect", NULL);
			STATS_ADD(pool->malloc_guarded, mopts.malloc_guard);
		}

		if (mopts.malloc_move &&
		    sz - mopts.malloc_guard < MALLOC_PAGESIZE -
		    MALLOC_LEEWAY) {
			/* fill whole allocation */
			if (mopts.malloc_junk == 2)
				memset(p, SOME_JUNK, psz - mopts.malloc_guard);
			/* shift towards the end */
			p = ((char *)p) + ((MALLOC_PAGESIZE - MALLOC_LEEWAY -
			    (sz - mopts.malloc_guard)) & ~(MALLOC_MINSIZE-1));
			/* fill zeros if needed and overwritten above */
			if (zero_fill && mopts.malloc_junk == 2)
				memset(p, 0, sz - mopts.malloc_guard);
		} else {
			if (mopts.malloc_junk == 2) {
				if (zero_fill)
					memset((char *)p + sz - mopts.malloc_guard,
					    SOME_JUNK, psz - sz);
				else
					memset(p, SOME_JUNK,
					    psz - mopts.malloc_guard);
			}
		}

	} else {
		/* takes care of SOME_JUNK */
		p = malloc_bytes(pool, sz, f);
		if (zero_fill && p != NULL && sz > 0)
			memset(p, 0, sz);
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
		wrterror("recursive call", NULL);
	}
	malloc_active--;
	_MALLOC_UNLOCK();
	errno = EDEADLK;
}

static int
malloc_init(void)
{
	if (omalloc_init(&mopts.malloc_pool)) {
		_MALLOC_UNLOCK();
		if (mopts.malloc_xmalloc)
			wrterror("out of memory", NULL);
		errno = ENOMEM;
		return -1;
	}
	return 0;
}

void *
malloc(size_t size)
{
	void *r;
	int saved_errno = errno;

	_MALLOC_LOCK();
	malloc_func = "malloc():";
	if (getpool() == NULL) {
		if (malloc_init() != 0)
			return NULL;
	}
	
	if (malloc_active++) {
		malloc_recurse();
		return NULL;
	}
	r = omalloc(size, 0, CALLER);
	malloc_active--;
	_MALLOC_UNLOCK();
	if (r == NULL && mopts.malloc_xmalloc) {
		wrterror("out of memory", NULL);
		errno = ENOMEM;
	}
	if (r != NULL)
		errno = saved_errno;
	return r;
}

static void
ofree(void *p)
{
	struct dir_info *pool = getpool();
	struct region_info *r;
	size_t sz;

	r = find(pool, p);
	if (r == NULL) {
		wrterror("bogus pointer (double free?)", p);
		return;
	}
	REALSIZE(sz, r);
	if (sz > MALLOC_MAXCHUNK) {
		if (sz - mopts.malloc_guard >= MALLOC_PAGESIZE -
		    MALLOC_LEEWAY) {
			if (r->p != p) {
				wrterror("bogus pointer", p);
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
				wrterror("guard size", NULL);
			if (!mopts.malloc_freeunmap) {
				if (mprotect((char *)p + PAGEROUND(sz) -
				    mopts.malloc_guard, mopts.malloc_guard,
				    PROT_READ | PROT_WRITE))
					wrterror("mprotect", NULL);
			}
			STATS_SUB(pool->malloc_guarded, mopts.malloc_guard);
		}
		if (mopts.malloc_junk && !mopts.malloc_freeunmap) {
			size_t amt = mopts.malloc_junk == 1 ? MALLOC_MAXCHUNK :
			    PAGEROUND(sz) - mopts.malloc_guard;
			memset(p, SOME_FREEJUNK, amt);
		}
		unmap(pool, p, PAGEROUND(sz));
		delete(pool, r);
	} else {
		void *tmp;
		int i;

		if (mopts.malloc_junk && sz > 0)
			memset(p, SOME_FREEJUNK, sz);
		if (!mopts.malloc_freenow) {
			if (find_chunknum(pool, r, p) == -1)
				return;
			i = getrbyte(pool) & MALLOC_DELAYED_CHUNK_MASK;
			tmp = p;
			p = pool->delayed_chunks[i];
			if (tmp == p) {
				wrterror("double free", p);
				return;
			}
			pool->delayed_chunks[i] = tmp;
		}
		if (p != NULL) {
			r = find(pool, p);
			if (r == NULL) {
				wrterror("bogus pointer (double free?)", p);
				return;
			}
			free_bytes(pool, r, p);
		}
	}
}

void
free(void *ptr)
{
	int saved_errno = errno;

	/* This is legal. */
	if (ptr == NULL)
		return;

	_MALLOC_LOCK();
	malloc_func = "free():";
	if (getpool() == NULL) {
		_MALLOC_UNLOCK();
		wrterror("free() called before allocation", NULL);
		return;
	}
	if (malloc_active++) {
		malloc_recurse();
		return;
	}
	ofree(ptr);
	malloc_active--;
	_MALLOC_UNLOCK();
	errno = saved_errno;
}


static void *
orealloc(void *p, size_t newsz, void *f)
{
	struct dir_info *pool = getpool();
	struct region_info *r;
	size_t oldsz, goldsz, gnewsz;
	void *q;

	if (p == NULL)
		return omalloc(newsz, 0, f);

	r = find(pool, p);
	if (r == NULL) {
		wrterror("bogus pointer (double free?)", p);
		return NULL;
	}
	if (newsz >= SIZE_MAX - mopts.malloc_guard - MALLOC_PAGESIZE) {
		errno = ENOMEM;
		return NULL;
	}

	REALSIZE(oldsz, r);
	goldsz = oldsz;
	if (oldsz > MALLOC_MAXCHUNK) {
		if (oldsz < mopts.malloc_guard)
			wrterror("guard size", NULL);
		oldsz -= mopts.malloc_guard;
	}

	gnewsz = newsz;
	if (gnewsz > MALLOC_MAXCHUNK)
		gnewsz += mopts.malloc_guard;

	if (newsz > MALLOC_MAXCHUNK && oldsz > MALLOC_MAXCHUNK && p == r->p &&
	    !mopts.malloc_realloc) {
		size_t roldsz = PAGEROUND(goldsz);
		size_t rnewsz = PAGEROUND(gnewsz);

		if (rnewsz > roldsz) {
			if (!mopts.malloc_guard) {
				void *hint = (char *)p + roldsz;
				size_t needed = rnewsz - roldsz;

				STATS_INC(pool->cheap_realloc_tries);
				q = map(pool, hint, needed, 0);
				if (q == hint)
					goto gotit;
				zapcacheregion(pool, hint, needed);
				q = MQUERY(hint, needed);
				if (q == hint)
					q = MMAPA(hint, needed);
				else
					q = MAP_FAILED;
				if (q == hint) {
gotit:
					STATS_ADD(pool->malloc_used, needed);
					if (mopts.malloc_junk == 2)
						memset(q, SOME_JUNK, needed);
					r->size = newsz;
					STATS_SETF(r, f);
					STATS_INC(pool->cheap_reallocs);
					return p;
				} else if (q != MAP_FAILED) {
					if (munmap(q, needed))
						wrterror("munmap", q);
				}
			}
		} else if (rnewsz < roldsz) {
			if (mopts.malloc_guard) {
				if (mprotect((char *)p + roldsz -
				    mopts.malloc_guard, mopts.malloc_guard,
				    PROT_READ | PROT_WRITE))
					wrterror("mprotect", NULL);
				if (mprotect((char *)p + rnewsz -
				    mopts.malloc_guard, mopts.malloc_guard,
				    PROT_NONE))
					wrterror("mprotect", NULL);
			}
			unmap(pool, (char *)p + rnewsz, roldsz - rnewsz);
			r->size = gnewsz;
			STATS_SETF(r, f);
			return p;
		} else {
			if (newsz > oldsz && mopts.malloc_junk == 2)
				memset((char *)p + newsz, SOME_JUNK,
				    rnewsz - mopts.malloc_guard - newsz);
			r->size = gnewsz;
			STATS_SETF(r, f);
			return p;
		}
	}
	if (newsz <= oldsz && newsz > oldsz / 2 && !mopts.malloc_realloc) {
		if (mopts.malloc_junk == 2 && newsz > 0)
			memset((char *)p + newsz, SOME_JUNK, oldsz - newsz);
		STATS_SETF(r, f);
		return p;
	} else if (newsz != oldsz || mopts.malloc_realloc) {
		q = omalloc(newsz, 0, f);
		if (q == NULL)
			return NULL;
		if (newsz != 0 && oldsz != 0)
			memcpy(q, p, oldsz < newsz ? oldsz : newsz);
		ofree(p);
		return q;
	} else {
		STATS_SETF(r, f);
		return p;
	}
}

void *
realloc(void *ptr, size_t size)
{
	void *r;
	int saved_errno = errno;

	_MALLOC_LOCK();
	malloc_func = "realloc():";
	if (getpool() == NULL) {
		if (malloc_init() != 0)
			return NULL;
	}
	if (malloc_active++) {
		malloc_recurse();
		return NULL;
	}
	r = orealloc(ptr, size, CALLER);

	malloc_active--;
	_MALLOC_UNLOCK();
	if (r == NULL && mopts.malloc_xmalloc) {
		wrterror("out of memory", NULL);
		errno = ENOMEM;
	}
	if (r != NULL)
		errno = saved_errno;
	return r;
}


/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW	(1UL << (sizeof(size_t) * 4))

void *
calloc(size_t nmemb, size_t size)
{
	void *r;
	int saved_errno = errno;

	_MALLOC_LOCK();
	malloc_func = "calloc():";
	if (getpool() == NULL) {
		if (malloc_init() != 0)
			return NULL;
	}
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX / nmemb < size) {
		_MALLOC_UNLOCK();
		if (mopts.malloc_xmalloc)
			wrterror("out of memory", NULL);
		errno = ENOMEM;
		return NULL;
	}

	if (malloc_active++) {
		malloc_recurse();
		return NULL;
	}

	size *= nmemb;
	r = omalloc(size, 1, CALLER);

	malloc_active--;
	_MALLOC_UNLOCK();
	if (r == NULL && mopts.malloc_xmalloc) {
		wrterror("out of memory", NULL);
		errno = ENOMEM;
	}
	if (r != NULL)
		errno = saved_errno;
	return r;
}

static void *
mapalign(struct dir_info *d, size_t alignment, size_t sz, int zero_fill)
{
	char *p, *q;

	if (alignment < MALLOC_PAGESIZE || ((alignment - 1) & alignment) != 0) {
		wrterror("mapalign bad alignment", NULL);
		return MAP_FAILED;
	}
	if (sz != PAGEROUND(sz)) {
		wrterror("mapalign round", NULL);
		return MAP_FAILED;
	}

	/* Allocate sz + alignment bytes of memory, which must include a
	 * subrange of size bytes that is properly aligned.  Unmap the
	 * other bytes, and then return that subrange.
	 */

	/* We need sz + alignment to fit into a size_t. */
	if (alignment > SIZE_MAX - sz)
		return MAP_FAILED;

	p = map(d, NULL, sz + alignment, zero_fill);
	if (p == MAP_FAILED)
		return MAP_FAILED;
	q = (char *)(((uintptr_t)p + alignment - 1) & ~(alignment - 1));
	if (q != p) {
		if (munmap(p, q - p))
			wrterror("munmap", p);
	}
	if (munmap(q + sz, alignment - (q - p)))
		wrterror("munmap", q + sz);
	STATS_SUB(d->malloc_used, alignment);

	return q;
}

static void *
omemalign(size_t alignment, size_t sz, int zero_fill, void *f)
{
	struct dir_info *pool = getpool();
	size_t psz;
	void *p;

	if (alignment <= MALLOC_PAGESIZE) {
		/*
		 * max(size, alignment) is enough to assure the requested alignment,
		 * since the allocator always allocates power-of-two blocks.
		 */
		if (sz < alignment)
			sz = alignment;
		return omalloc(sz, zero_fill, f);
	}

	if (sz >= SIZE_MAX - mopts.malloc_guard - MALLOC_PAGESIZE) {
		errno = ENOMEM;
		return NULL;
	}

	sz += mopts.malloc_guard;
	psz = PAGEROUND(sz);

	p = mapalign(pool, alignment, psz, zero_fill);
	if (p == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if (insert(pool, p, sz, f)) {
		unmap(pool, p, psz);
		errno = ENOMEM;
		return NULL;
	}

	if (mopts.malloc_guard) {
		if (mprotect((char *)p + psz - mopts.malloc_guard,
		    mopts.malloc_guard, PROT_NONE))
			wrterror("mprotect", NULL);
		STATS_ADD(pool->malloc_guarded, mopts.malloc_guard);
	}

	if (mopts.malloc_junk == 2) {
		if (zero_fill)
			memset((char *)p + sz - mopts.malloc_guard,
			    SOME_JUNK, psz - sz);
		else
			memset(p, SOME_JUNK, psz - mopts.malloc_guard);
	}

	return p;
}

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	int res, saved_errno = errno;
	void *r;

	/* Make sure that alignment is a large enough power of 2. */
	if (((alignment - 1) & alignment) != 0 || alignment < sizeof(void *))
		return EINVAL;

	_MALLOC_LOCK();
	malloc_func = "posix_memalign():";
	if (getpool() == NULL) {
		if (malloc_init() != 0)
			goto err;
	}
	if (malloc_active++) {
		malloc_recurse();
		goto err;
	}
	r = omemalign(alignment, size, 0, CALLER);
	malloc_active--;
	_MALLOC_UNLOCK();
	if (r == NULL) {
		if (mopts.malloc_xmalloc) {
			wrterror("out of memory", NULL);
			errno = ENOMEM;
		}
		goto err;
	}
	errno = saved_errno;
	*memptr = r;
	return 0;

err:
	res = errno;
	errno = saved_errno;
	return res;
}

#ifdef MALLOC_STATS

struct malloc_leak {
	void (*f)();
	size_t total_size;
	int count;
};

struct leaknode {
	RB_ENTRY(leaknode) entry;
	struct malloc_leak d;
};

static int
leakcmp(struct leaknode *e1, struct leaknode *e2)
{
	return e1->d.f < e2->d.f ? -1 : e1->d.f > e2->d.f;
}

static RB_HEAD(leaktree, leaknode) leakhead;
RB_GENERATE_STATIC(leaktree, leaknode, entry, leakcmp)

static void
putleakinfo(void *f, size_t sz, int cnt)
{
	struct leaknode key, *p;
	static struct leaknode *page;
	static int used;

	if (cnt == 0)
		return;

	key.d.f = f;
	p = RB_FIND(leaktree, &leakhead, &key);
	if (p == NULL) {
		if (page == NULL ||
		    used >= MALLOC_PAGESIZE / sizeof(struct leaknode)) {
			page = MMAP(MALLOC_PAGESIZE);
			if (page == MAP_FAILED)
				return;
			used = 0;
		}
		p = &page[used++];
		p->d.f = f;
		p->d.total_size = sz * cnt;
		p->d.count = cnt;
		RB_INSERT(leaktree, &leakhead, p);
	} else {
		p->d.total_size += sz * cnt;
		p->d.count += cnt;
	}
}

static struct malloc_leak *malloc_leaks;

static void
dump_leaks(int fd)
{
	struct leaknode *p;
	char buf[64];
	int i = 0;

	snprintf(buf, sizeof(buf), "Leak report\n");
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "                 f     sum      #    avg\n");
	write(fd, buf, strlen(buf));
	/* XXX only one page of summary */
	if (malloc_leaks == NULL)
		malloc_leaks = MMAP(MALLOC_PAGESIZE);
	if (malloc_leaks != MAP_FAILED)
		memset(malloc_leaks, 0, MALLOC_PAGESIZE);
	RB_FOREACH(p, leaktree, &leakhead) {
		snprintf(buf, sizeof(buf), "%18p %7zu %6u %6zu\n", p->d.f,
		    p->d.total_size, p->d.count, p->d.total_size / p->d.count);
		write(fd, buf, strlen(buf));
		if (malloc_leaks == MAP_FAILED ||
		    i >= MALLOC_PAGESIZE / sizeof(struct malloc_leak))
			continue;
		malloc_leaks[i].f = p->d.f;
		malloc_leaks[i].total_size = p->d.total_size;
		malloc_leaks[i].count = p->d.count;
		i++;
	}
}

static void
dump_chunk(int fd, struct chunk_info *p, void *f, int fromfreelist)
{
	char buf[64];

	while (p != NULL) {
		snprintf(buf, sizeof(buf), "chunk %18p %18p %4d %d/%d\n",
		    p->page, ((p->bits[0] & 1) ? NULL : f),
		    p->size, p->free, p->total);
		write(fd, buf, strlen(buf));
		if (!fromfreelist) {
			if (p->bits[0] & 1)
				putleakinfo(NULL, p->size, p->total - p->free);
			else {
				putleakinfo(f, p->size, 1);
				putleakinfo(NULL, p->size,
				    p->total - p->free - 1);
			}
			break;
		}
		p = LIST_NEXT(p, entries);
		if (p != NULL) {
			snprintf(buf, sizeof(buf), "        ");
			write(fd, buf, strlen(buf));
		}
	}
}

static void
dump_free_chunk_info(int fd, struct dir_info *d)
{
	char buf[64];
	int i, j, count;
	struct chunk_info *p;

	snprintf(buf, sizeof(buf), "Free chunk structs:\n");
	write(fd, buf, strlen(buf));
	for (i = 0; i <= MALLOC_MAXSHIFT; i++) {
		count = 0;
		LIST_FOREACH(p, &d->chunk_info_list[i], entries)
			count++;
		for (j = 0; j < MALLOC_CHUNK_LISTS; j++) {
			p = LIST_FIRST(&d->chunk_dir[i][j]);
			if (p == NULL && count == 0)
				continue;
			snprintf(buf, sizeof(buf), "%2d) %3d ", i, count);
			write(fd, buf, strlen(buf));
			if (p != NULL)
				dump_chunk(fd, p, NULL, 1);
			else
				write(fd, "\n", 1);
		}
	}

}

static void
dump_free_page_info(int fd, struct dir_info *d)
{
	char buf[64];
	int i;

	snprintf(buf, sizeof(buf), "Free pages cached: %zu\n",
	    d->free_regions_size);
	write(fd, buf, strlen(buf));
	for (i = 0; i < mopts.malloc_cache; i++) {
		if (d->free_regions[i].p != NULL) {
			snprintf(buf, sizeof(buf), "%2d) ", i);
			write(fd, buf, strlen(buf));
			snprintf(buf, sizeof(buf), "free at %p: %zu\n",
			    d->free_regions[i].p, d->free_regions[i].size);
			write(fd, buf, strlen(buf));
		}
	}
}

static void
malloc_dump1(int fd, struct dir_info *d)
{
	char buf[100];
	size_t i, realsize;

	snprintf(buf, sizeof(buf), "Malloc dir of %s at %p\n", __progname, d);
	write(fd, buf, strlen(buf));
	if (d == NULL)
		return;
	snprintf(buf, sizeof(buf), "Region slots free %zu/%zu\n",
		d->regions_free, d->regions_total);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Finds %zu/%zu\n", d->finds,
	    d->find_collisions);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Inserts %zu/%zu\n", d->inserts,
	    d->insert_collisions);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Deletes %zu/%zu\n", d->deletes,
	    d->delete_moves);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Cheap reallocs %zu/%zu\n",
	    d->cheap_reallocs, d->cheap_realloc_tries);
	write(fd, buf, strlen(buf));
	dump_free_chunk_info(fd, d);
	dump_free_page_info(fd, d);
	snprintf(buf, sizeof(buf),
	    "slot)  hash d  type               page                  f size [free/n]\n");
	write(fd, buf, strlen(buf));
	for (i = 0; i < d->regions_total; i++) {
		if (d->r[i].p != NULL) {
			size_t h = hash(d->r[i].p) &
			    (d->regions_total - 1);
			snprintf(buf, sizeof(buf), "%4zx) #%4zx %zd ",
			    i, h, h - i);
			write(fd, buf, strlen(buf));
			REALSIZE(realsize, &d->r[i]);
			if (realsize > MALLOC_MAXCHUNK) {
				putleakinfo(d->r[i].f, realsize, 1);
				snprintf(buf, sizeof(buf),
				    "pages %12p %12p %zu\n", d->r[i].p,
				    d->r[i].f, realsize);
				write(fd, buf, strlen(buf));
			} else
				dump_chunk(fd,
				    (struct chunk_info *)d->r[i].size,
				    d->r[i].f, 0);
		}
	}
	snprintf(buf, sizeof(buf), "In use %zu\n", d->malloc_used);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Guarded %zu\n", d->malloc_guarded);
	write(fd, buf, strlen(buf));
	dump_leaks(fd);
	write(fd, "\n", 1);
}

void
malloc_dump(int fd)
{
	struct dir_info *pool = getpool();
	int i;
	void *p;
	struct region_info *r;
	int saved_errno = errno;

	for (i = 0; i < MALLOC_DELAYED_CHUNK_MASK + 1; i++) {
		p = pool->delayed_chunks[i];
		if (p == NULL)
			continue;
		r = find(pool, p);
		if (r == NULL)
			wrterror("bogus pointer in malloc_dump", p);
		free_bytes(pool, r, p);
		pool->delayed_chunks[i] = NULL;
	}
	/* XXX leak when run multiple times */
	RB_INIT(&leakhead);
	malloc_dump1(fd, pool);
	errno = saved_errno;
}

static void
malloc_exit(void)
{
	static const char q[] = "malloc() warning: Couldn't dump stats\n";
	int save_errno = errno, fd;

	fd = open("malloc.out", O_RDWR|O_APPEND);
	if (fd != -1) {
		malloc_dump(fd);
		close(fd);
	} else
		write(STDERR_FILENO, q, sizeof(q) - 1);
	errno = save_errno;
}

#endif /* MALLOC_STATS */
