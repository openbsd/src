/*	$OpenBSD: malloc.c,v 1.128 2011/04/30 14:56:20 otto Exp $	*/
/*
 * Copyright (c) 2008 Otto Moerbeek <otto@drijf.net>
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
 * Parts of this code, mainly the sub page sized chunk management code is
 * derived from the malloc implementation with the following license:
 */
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.  Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/* #define MALLOC_STATS */

#include <sys/types.h>
#include <sys/param.h>
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
#include <fcntl.h>
#endif

#include "thread_private.h"

#define MALLOC_MINSHIFT		4
#define MALLOC_MAXSHIFT		16

#if defined(__sparc__) && !defined(__sparcv9__)
#define MALLOC_PAGESHIFT	(13U)
#elif defined(__mips64__)
#define MALLOC_PAGESHIFT	(14U)
#else
#define MALLOC_PAGESHIFT	(PGSHIFT)
#endif

#define MALLOC_PAGESIZE		(1UL << MALLOC_PAGESHIFT)
#define MALLOC_MINSIZE		(1UL << MALLOC_MINSHIFT)
#define MALLOC_PAGEMASK		(MALLOC_PAGESIZE - 1)
#define MASK_POINTER(p)		((void *)(((uintptr_t)(p)) & ~MALLOC_PAGEMASK))

#define MALLOC_MAXCHUNK		(1 << (MALLOC_PAGESHIFT-1))
#define MALLOC_MAXCACHE		256
#define MALLOC_DELAYED_CHUNKS	15	/* max of getrnibble() */
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

struct region_info {
	void *p;		/* page; low bits used to mark chunks */
	uintptr_t size;		/* size for pages, or chunk_info pointer */
};

LIST_HEAD(chunk_head, chunk_info);

struct dir_info {
	u_int32_t canary1;
	struct region_info *r;		/* region slots */
	size_t regions_total;		/* number of region slots */
	size_t regions_bits;		/* log2 of total */
	size_t regions_free;		/* number of free slots */
					/* list of free chunk info structs */
	struct chunk_head chunk_info_list;
					/* lists of chunks with free slots */
	struct chunk_head chunk_dir[MALLOC_MAXSHIFT];
	size_t free_regions_size;	/* free pages cached */
					/* free pages cache */
	struct region_info free_regions[MALLOC_MAXCACHE];
					/* delayed free chunk slots */
	void *delayed_chunks[MALLOC_DELAYED_CHUNKS + 1];
#ifdef MALLOC_STATS
	size_t inserts;
	size_t insert_collisions;
	size_t finds;
	size_t find_collisions;
	size_t deletes;
	size_t delete_moves;
	size_t cheap_realloc_tries;
	size_t cheap_reallocs;
#define STATS_INC(x) ((x)++)
#define STATS_ZERO(x) ((x) = 0)
#else
#define STATS_INC(x)	/* nothing */
#define STATS_ZERO(x)	/* nothing */
#endif /* MALLOC_STATS */
	u_int32_t canary2;
};
#define DIR_INFO_RSZ	((sizeof(struct dir_info) + MALLOC_PAGEMASK) & \
			~MALLOC_PAGEMASK)

/*
 * This structure describes a page worth of chunks.
 *
 * How many bits per u_long in the bitmap
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
	u_short bits[(MALLOC_PAGESIZE / MALLOC_MINSIZE) / MALLOC_BITS];
};

struct malloc_readonly {
	struct dir_info *g_pool;	/* Main bookkeeping information */
	int	malloc_abort;		/* abort() on error */
	int	malloc_freeprot;	/* mprotect free pages PROT_NONE? */
	int	malloc_hint;		/* call madvice on free pages?  */
	int	malloc_junk;		/* junk fill? */
	int	malloc_move;		/* move allocations to end of page? */
	int	malloc_realloc;		/* always realloc? */
	int	malloc_xmalloc;		/* xmalloc behaviour? */
	int	malloc_zero;		/* zero fill? */
	size_t	malloc_guard;		/* use guard pages after allocations? */
	u_int	malloc_cache;		/* free pages we cache */
#ifdef MALLOC_STATS
	int	malloc_stats;		/* dump statistics at end */
#endif
	u_int32_t malloc_canary;	/* Matched against ones in g_pool */
};

/* This object is mapped PROT_READ after initialisation to prevent tampering */
static union {
	struct malloc_readonly mopts;
	u_char _pad[MALLOC_PAGESIZE];
} malloc_readonly __attribute__((aligned(MALLOC_PAGESIZE)));
#define mopts	malloc_readonly.mopts
#define g_pool	mopts.g_pool

char		*malloc_options;	/* compile-time options */

static char	*malloc_func;		/* current function */
static int	malloc_active;		/* status of malloc */

static size_t	malloc_guarded;		/* bytes used for guards */
static size_t	malloc_used;		/* bytes allocated */

static size_t rnibblesused;		/* random nibbles used */
static u_char rbytes[512];		/* random bytes */
static u_char getrnibble(void);

extern char	*__progname;

/* low bits of r->p determine size: 0 means >= page size and p->size holding
 *  real size, otherwise r->size is a shift count, or 1 for malloc(0)
 */
#define REALSIZE(sz, r) 					\
	(sz) = (uintptr_t)(r)->p & MALLOC_PAGEMASK,		\
	(sz) = ((sz) == 0 ? (r)->size : ((sz) == 1 ? 0 : (1 << ((sz)-1))))

static inline size_t
hash(void *p)
{
	size_t sum;
	union {
		uintptr_t p;
		unsigned short a[sizeof(void *) / sizeof(short)];
	} u;
	u.p = (uintptr_t)p >> MALLOC_PAGESHIFT;
	sum = u.a[0];
	sum = (sum << 7) - sum + u.a[1];
#ifdef __LP64__
	sum = (sum << 7) - sum + u.a[2];
	sum = (sum << 7) - sum + u.a[3];
#endif
	return sum;
}

#ifdef MALLOC_STATS
static void
dump_chunk(int fd, struct chunk_info *p, int fromfreelist)
{
	char buf[64];

	while (p != NULL) {
		snprintf(buf, sizeof(buf), "chunk %d %d/%d %p\n", p->size,
		    p->free, p->total, p->page);
		write(fd, buf, strlen(buf));
		if (!fromfreelist)
			break;
		p = LIST_NEXT(p, entries);
		if (p != NULL) {
			snprintf(buf, sizeof(buf), "    ");
			write(fd, buf, strlen(buf));
		}
	}
}

static void
dump_free_chunk_info(int fd, struct dir_info *d)
{
	char buf[64];
	int i;

	snprintf(buf, sizeof(buf), "Free chunk structs:\n");
	write(fd, buf, strlen(buf));
	for (i = 0; i < MALLOC_MAXSHIFT; i++) {
		struct chunk_info *p = LIST_FIRST(&d->chunk_dir[i]);
		if (p != NULL) {
			snprintf(buf, sizeof(buf), "%2d) ", i);
			write(fd, buf, strlen(buf));
			dump_chunk(fd, p, 1);
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
	char buf[64];
	size_t i, realsize;

	snprintf(buf, sizeof(buf), "Malloc dir of %s at %p\n", __progname, d);
	write(fd, buf, strlen(buf));
	if (d == NULL)
		return;
	snprintf(buf, sizeof(buf), "Regions slots %zu\n", d->regions_total);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Finds %zu/%zu %f\n", d->finds,
	    d->find_collisions,
	    1.0 + (double)d->find_collisions / d->finds);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Inserts %zu/%zu %f\n", d->inserts,
	    d->insert_collisions,
	    1.0 + (double)d->insert_collisions / d->inserts);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Deletes %zu/%zu\n", d->deletes,
	     d->delete_moves);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Cheap reallocs %zu/%zu\n",
	    d->cheap_reallocs, d->cheap_realloc_tries);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Regions slots free %zu\n", d->regions_free);
	write(fd, buf, strlen(buf));
	for (i = 0; i < d->regions_total; i++) {
		if (d->r[i].p != NULL) {
			size_t h = hash(d->r[i].p) &
			    (d->regions_total - 1);
			snprintf(buf, sizeof(buf), "%4zx) #%zx %zd ",
			    i, h, h - i);
			write(fd, buf, strlen(buf));
			REALSIZE(realsize, &d->r[i]);
			if (realsize > MALLOC_MAXCHUNK) {
				snprintf(buf, sizeof(buf),
				    "%p: %zu\n", d->r[i].p, realsize);
				write(fd, buf, strlen(buf));
			} else
				dump_chunk(fd,
				    (struct chunk_info *)d->r[i].size, 0);
		}
	}
	dump_free_chunk_info(fd, d);
	dump_free_page_info(fd, d);
	snprintf(buf, sizeof(buf), "In use %zu\n", malloc_used);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Guarded %zu\n", malloc_guarded);
	write(fd, buf, strlen(buf));
}


void
malloc_dump(int fd)
{
	malloc_dump1(fd, g_pool);
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


static void
wrterror(char *msg, void *p)
{
	char		*q = " error: ";
	struct iovec	iov[6];
	char		buf[20];

	iov[0].iov_base = __progname;
	iov[0].iov_len = strlen(__progname);
	iov[1].iov_base = malloc_func;
	iov[1].iov_len = strlen(malloc_func);
	iov[2].iov_base = q;
	iov[2].iov_len = strlen(q);
	iov[3].iov_base = msg;
	iov[3].iov_len = strlen(msg);
	iov[4].iov_base = buf;
	if (p == NULL)
		iov[4].iov_len = 0;
	else {
		snprintf(buf, sizeof(buf), " %p", p);
		iov[4].iov_len = strlen(buf);
	}
	iov[5].iov_base = "\n";
	iov[5].iov_len = 1;
	writev(STDERR_FILENO, iov, 6);

#ifdef MALLOC_STATS
	if (mopts.malloc_stats)
		malloc_dump(STDERR_FILENO);
#endif /* MALLOC_STATS */
	//malloc_active--;
	if (mopts.malloc_abort)
		abort();
}

static void
rbytes_init(void)
{
	arc4random_buf(rbytes, sizeof(rbytes));
	rnibblesused = 0;
}

static inline u_char
getrnibble(void)
{
	u_char x;

	if (rnibblesused >= 2 * sizeof(rbytes))
		rbytes_init();
	x = rbytes[rnibblesused++ / 2];
	return (rnibblesused & 1 ? x & 0xf : x >> 4);
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
		if (munmap(p, sz))
			wrterror("munmap", p);
		malloc_used -= sz;
		return;
	}
	tounmap = 0;
	rsz = mopts.malloc_cache - d->free_regions_size;
	if (psz > rsz)
		tounmap = psz - rsz;
	offset = getrnibble();
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
			malloc_used -= rsz;
		}
	}
	if (tounmap > 0)
		wrterror("malloc cache underflow", NULL);
	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[i];
		if (r->p == NULL) {
			if (mopts.malloc_hint)
				madvise(p, sz, MADV_FREE);
			if (mopts.malloc_freeprot)
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
zapcacheregion(struct dir_info *d, void *p)
{
	u_int i;
	struct region_info *r;
	size_t rsz;

	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[i];
		if (r->p == p) {
			rsz = r->size << MALLOC_PAGESHIFT;
			if (munmap(r->p, rsz))
				wrterror("munmap", r->p);
			r->p = NULL;
			d->free_regions_size -= r->size;
			r->size = 0;
			malloc_used -= rsz;
		}
	}
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
		wrterror("internal struct corrupt", NULL);
	if (sz != PAGEROUND(sz)) {
		wrterror("map round", NULL);
		return NULL;
	}
	if (psz > d->free_regions_size) {
		p = MMAP(sz);
		if (p != MAP_FAILED)
			malloc_used += sz;
		/* zero fill not needed */
		return p;
	}
	offset = getrnibble();
	for (i = 0; i < mopts.malloc_cache; i++) {
		r = &d->free_regions[(i + offset) & (mopts.malloc_cache - 1)];
		if (r->p != NULL) {
			if (r->size == psz) {
				p = r->p;
				if (mopts.malloc_freeprot)
					mprotect(p, sz, PROT_READ | PROT_WRITE);
				if (mopts.malloc_hint)
					madvise(p, sz, MADV_NORMAL);
				r->p = NULL;
				r->size = 0;
				d->free_regions_size -= psz;
				if (zero_fill)
					memset(p, 0, sz);
				else if (mopts.malloc_junk &&
				    mopts.malloc_freeprot)
					memset(p, SOME_FREEJUNK, sz);
				return p;
			} else if (r->size > psz)
				big = r;
		}
	}
	if (big != NULL) {
		r = big;
		p = (char *)r->p + ((r->size - psz) << MALLOC_PAGESHIFT);
		if (mopts.malloc_freeprot)
			mprotect(p, sz, PROT_READ | PROT_WRITE);
		if (mopts.malloc_hint)
			madvise(p, sz, MADV_NORMAL);
		r->size -= psz;
		d->free_regions_size -= psz;
		if (zero_fill)
			memset(p, 0, sz);
		else if (mopts.malloc_junk && mopts.malloc_freeprot)
			memset(p, SOME_FREEJUNK, sz);
		return p;
	}
	p = MMAP(sz);
	if (p != MAP_FAILED)
		malloc_used += sz;
	if (d->free_regions_size > mopts.malloc_cache)
		wrterror("malloc cache", NULL);
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

	rbytes_init();

	/*
	 * Default options
	 */
	mopts.malloc_abort = 1;
	mopts.malloc_move = 1;
	mopts.malloc_cache = 64;

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
				mopts.malloc_freeprot = 0;
				break;
			case 'F':
				mopts.malloc_freeprot = 1;
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
				mopts.malloc_junk = 1;
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
			case 'S':
				mopts.malloc_freeprot = mopts.malloc_junk = 1;
				mopts.malloc_guard = MALLOC_PAGESIZE;
				break;
			case 'x':
				mopts.malloc_xmalloc = 0;
				break;
			case 'X':
				mopts.malloc_xmalloc = 1;
				break;
			case 'z':
				mopts.malloc_zero = 0;
				break;
			case 'Z':
				mopts.malloc_zero = 1;
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

	/*
	 * We want junk in the entire allocation, and zero only in the part
	 * the user asked for.
	 */
	if (mopts.malloc_zero)
		mopts.malloc_junk = 1;

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

	d->regions_bits = 9;
	d->regions_free = d->regions_total = 1 << d->regions_bits;
	regioninfo_size = d->regions_total * sizeof(struct region_info);
	d->r = MMAP(regioninfo_size);
	if (d->r == MAP_FAILED) {
		wrterror("malloc init mmap failed", NULL);
		d->regions_total = 0;
		return 1;
	}
	LIST_INIT(&d->chunk_info_list);
	for (i = 0; i < MALLOC_MAXSHIFT; i++)
		LIST_INIT(&d->chunk_dir[i]);
	malloc_used += regioninfo_size;
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
	size_t newbits;
	size_t newtotal;
	size_t newsize;
	size_t mask;
	size_t i;
	struct region_info *p;

	if (d->regions_total > SIZE_MAX / sizeof(struct region_info) / 2 )
		return 1;

	newbits = d->regions_bits + 1;
	newtotal = d->regions_total * 2;
	newsize = newtotal * sizeof(struct region_info);
	mask = newtotal - 1;

	p = MMAP(newsize);
	if (p == MAP_FAILED)
		return 1;
	
	malloc_used += newsize;
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
		malloc_used -= d->regions_total * sizeof(struct region_info);
	d->regions_free = d->regions_free + d->regions_total;
	d->regions_total = newtotal;
	d->regions_bits = newbits;
	d->r = p;
	return 0;
}

static struct chunk_info *
alloc_chunk_info(struct dir_info *d)
{
	struct chunk_info *p;
	int i;
	
	if (LIST_EMPTY(&d->chunk_info_list)) {
		p = MMAP(MALLOC_PAGESIZE);
		if (p == MAP_FAILED)
			return NULL;
		malloc_used += MALLOC_PAGESIZE;
		for (i = 0; i < MALLOC_PAGESIZE / sizeof(*p); i++)
			LIST_INSERT_HEAD(&d->chunk_info_list, &p[i], entries);
	}
	p = LIST_FIRST(&d->chunk_info_list);
	LIST_REMOVE(p, entries);
	memset(p, 0, sizeof *p);
	p->canary = d->canary1;
	return p;
}

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
	STATS_INC(d->inserts);
	while (q != NULL) {
		index = (index - 1) & mask;
		q = d->r[index].p;
		STATS_INC(d->insert_collisions);
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
	return q == p ? &d->r[index] : NULL;
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
	STATS_INC(g_pool->deletes);

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
			STATS_INC(g_pool->delete_moves);
			break;
		}

	}
}
 
/*
 * Allocate a page of chunks
 */
static struct chunk_info *
omalloc_make_chunks(struct dir_info *d, int bits)
{
	struct chunk_info *bp;
	void		*pp;
	long		i, k;

	/* Allocate a new bucket */
	pp = map(d, MALLOC_PAGESIZE, 0);
	if (pp == MAP_FAILED)
		return NULL;

	bp = alloc_chunk_info(d);
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
			LIST_INSERT_HEAD(&d->chunk_info_list, bp, entries);
			return NULL;
		}
	} else {
		bp->size = (1UL << bits);
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

	LIST_INSERT_HEAD(&d->chunk_dir[bits], bp, entries);

	bits++;
	if ((uintptr_t)pp & bits)
		wrterror("pp & bits", pp);

	insert(d, (void *)((uintptr_t)pp | bits), (uintptr_t)bp);
	return bp;
}


/*
 * Allocate a chunk
 */
static void *
malloc_bytes(struct dir_info *d, size_t size)
{
	int		i, j;
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

	/* If it's empty, make a page more of that size chunks */
	if (LIST_EMPTY(&d->chunk_dir[j])) {
		bp = omalloc_make_chunks(d, j);
		if (bp == NULL)
			return NULL;
	} else
		bp = LIST_FIRST(&d->chunk_dir[j]);

	if (bp->canary != d->canary1)
		wrterror("chunk info corrupted", NULL);
	/* Find first word of bitmap which isn't empty */
	for (lp = bp->bits; !*lp; lp++)
		/* EMPTY */;

	/* Find that bit, and tweak it */
	u = 1;
	k = 0;
	while (!(*lp & u)) {
		u += u;
		k++;
	}

	/* advance a random # of positions */
	if (bp->free > 1) {
		i = getrnibble() % bp->free;
		while (i > 0) {
			u += u;
			k++;
			if (k >= MALLOC_BITS) {
				while (!*++lp)
					/* EMPTY */;
				u = 1;
				k = 0;
				if (lp - bp->bits > (bp->total - 1) /
				    MALLOC_BITS) {
					wrterror("chunk overflow", NULL);
					errno = EFAULT;
					return (NULL);
				}
			}
			if (*lp & u)
				i--;
		}
	}

	*lp ^= u;

	/* If there are no more free, remove from free-list */
	if (!--bp->free)
		LIST_REMOVE(bp, entries);

	/* Adjust to the real offset of that chunk */
	k += (lp - bp->bits) * MALLOC_BITS;
	k <<= bp->shift;

	if (mopts.malloc_junk && bp->size > 0)
		memset((char *)bp->page + k, SOME_JUNK, bp->size);
	return ((char *)bp->page + k);
}


/*
 * Free a chunk, and possibly the page it's on, if the page becomes empty.
 */
static void
free_bytes(struct dir_info *d, struct region_info *r, void *ptr)
{
	struct chunk_head *mp;
	struct chunk_info *info;
	long i;

	info = (struct chunk_info *)r->size;
	if (info->canary != d->canary1)
		wrterror("chunk info corrupted", NULL);

	/* Find the chunk number on the page */
	i = ((uintptr_t)ptr & MALLOC_PAGEMASK) >> info->shift;

	if ((uintptr_t)ptr & ((1UL << (info->shift)) - 1)) {
		wrterror("modified chunk-pointer", ptr);
		return;
	}
	if (info->bits[i / MALLOC_BITS] & (1UL << (i % MALLOC_BITS))) {
		wrterror("chunk is already free", ptr);
		return;
	}

	info->bits[i / MALLOC_BITS] |= 1UL << (i % MALLOC_BITS);
	info->free++;

	if (info->size != 0)
		mp = d->chunk_dir + info->shift;
	else
		mp = d->chunk_dir;

	if (info->free == 1) {
		/* Page became non-full */
		LIST_INSERT_HEAD(mp, info, entries);
		return;
	}
	if (info->free != info->total)
		return;

	LIST_REMOVE(info, entries);

	if (info->size == 0 && !mopts.malloc_freeprot)
		mprotect(info->page, MALLOC_PAGESIZE, PROT_READ | PROT_WRITE);
	unmap(d, info->page, MALLOC_PAGESIZE);

	delete(d, r);
	LIST_INSERT_HEAD(&d->chunk_info_list, info, entries);
}



static void *
omalloc(size_t sz, int zero_fill)
{
	void *p;
	size_t psz;

	if (sz > MALLOC_MAXCHUNK) {
		if (sz >= SIZE_MAX - mopts.malloc_guard - MALLOC_PAGESIZE) {
			errno = ENOMEM;
			return NULL;
		}
		sz += mopts.malloc_guard;
		psz = PAGEROUND(sz);
		p = map(g_pool, psz, zero_fill);
		if (p == MAP_FAILED) {
			errno = ENOMEM;
			return NULL;
		}
		if (insert(g_pool, p, sz)) {
			unmap(g_pool, p, psz);
			errno = ENOMEM;
			return NULL;
		}
		if (mopts.malloc_guard) {
			if (mprotect((char *)p + psz - mopts.malloc_guard,
			    mopts.malloc_guard, PROT_NONE))
				wrterror("mprotect", NULL);
			malloc_guarded += mopts.malloc_guard;
		}

		if (mopts.malloc_move &&
		    sz - mopts.malloc_guard < MALLOC_PAGESIZE -
		    MALLOC_LEEWAY) {
			/* fill whole allocation */
			if (mopts.malloc_junk)
				memset(p, SOME_JUNK, psz - mopts.malloc_guard);
			/* shift towards the end */
			p = ((char *)p) + ((MALLOC_PAGESIZE - MALLOC_LEEWAY -
			    (sz - mopts.malloc_guard)) & ~(MALLOC_MINSIZE-1));
			/* fill zeros if needed and overwritten above */
			if (zero_fill && mopts.malloc_junk)
				memset(p, 0, sz - mopts.malloc_guard);
		} else {
			if (mopts.malloc_junk) {
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
		p = malloc_bytes(g_pool, sz);
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
	if (omalloc_init(&g_pool)) {
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
	malloc_func = " in malloc():";
	if (g_pool == NULL) {
		if (malloc_init() != 0)
			return NULL;
	}
	if (malloc_active++) {
		malloc_recurse();
		return NULL;
	}
	r = omalloc(size, mopts.malloc_zero);
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
	struct region_info *r;
	size_t sz;

	r = find(g_pool, p);
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
			if (!mopts.malloc_freeprot) {
				if (mprotect((char *)p + PAGEROUND(sz) -
				    mopts.malloc_guard, mopts.malloc_guard,
				    PROT_READ | PROT_WRITE))
					wrterror("mprotect", NULL);
			}
			malloc_guarded -= mopts.malloc_guard;
		}
		if (mopts.malloc_junk && !mopts.malloc_freeprot)
			memset(p, SOME_FREEJUNK,
			    PAGEROUND(sz) - mopts.malloc_guard);
		unmap(g_pool, p, PAGEROUND(sz));
		delete(g_pool, r);
	} else {
		void *tmp;
		int i;

		if (mopts.malloc_junk && sz > 0)
			memset(p, SOME_FREEJUNK, sz);
		if (!mopts.malloc_freeprot) {
			i = getrnibble();
			tmp = p;
			p = g_pool->delayed_chunks[i];
			g_pool->delayed_chunks[i] = tmp;
		}
		if (p != NULL) {
			r = find(g_pool, p);
			if (r == NULL) {
				wrterror("bogus pointer (double free?)", p);
				return;
			}
			free_bytes(g_pool, r, p);
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
	malloc_func = " in free():";  
	if (g_pool == NULL) {
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
orealloc(void *p, size_t newsz)
{
	struct region_info *r;
	size_t oldsz, goldsz, gnewsz;
	void *q;

	if (p == NULL)
		return omalloc(newsz, 0);

	r = find(g_pool, p);
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
				STATS_INC(g_pool->cheap_realloc_tries);
				zapcacheregion(g_pool, (char *)p + roldsz);
				q = MMAPA((char *)p + roldsz, rnewsz - roldsz);
				if (q == (char *)p + roldsz) {
					malloc_used += rnewsz - roldsz;
					if (mopts.malloc_junk)
						memset(q, SOME_JUNK,
						    rnewsz - roldsz);
					r->size = newsz;
					STATS_INC(g_pool->cheap_reallocs);
					return p;
				} else if (q != MAP_FAILED)
					munmap(q, rnewsz - roldsz);
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
			unmap(g_pool, (char *)p + rnewsz, roldsz - rnewsz);
			r->size = gnewsz;
			return p;
		} else {
			if (newsz > oldsz && mopts.malloc_junk)
				memset((char *)p + newsz, SOME_JUNK,
				    rnewsz - mopts.malloc_guard - newsz);
			r->size = gnewsz;
			return p;
		}
	}
	if (newsz <= oldsz && newsz > oldsz / 2 && !mopts.malloc_realloc) {
		if (mopts.malloc_junk && newsz > 0)
			memset((char *)p + newsz, SOME_JUNK, oldsz - newsz);
		return p;
	} else if (newsz != oldsz || mopts.malloc_realloc) {
		q = omalloc(newsz, 0);
		if (q == NULL)
			return NULL;
		if (newsz != 0 && oldsz != 0)
			memcpy(q, p, oldsz < newsz ? oldsz : newsz);
		ofree(p);
		return q;
	} else
		return p;
}

void *
realloc(void *ptr, size_t size)
{
	void *r;
	int saved_errno = errno;
  
	_MALLOC_LOCK();
	malloc_func = " in realloc():";  
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
	_MALLOC_UNLOCK();
	if (r == NULL && mopts.malloc_xmalloc) {
		wrterror("out of memory", NULL);
		errno = ENOMEM;
	}
	if (r != NULL)
		errno = saved_errno;
	return r;
}


#define MUL_NO_OVERFLOW	(1UL << (sizeof(size_t) * 4))

void *
calloc(size_t nmemb, size_t size)
{
	void *r;
	int saved_errno = errno;

	_MALLOC_LOCK();
	malloc_func = " in calloc():";  
	if (g_pool == NULL) {
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
	r = omalloc(size, 1);
  
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

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *result;

	/* Make sure that alignment is a large enough power of 2. */
	if (((alignment - 1) & alignment) != 0 || alignment < sizeof(void *) ||
	    alignment > MALLOC_PAGESIZE)
		return EINVAL;

	/* 
	 * max(size, alignment) is enough to assure the requested alignment,
	 * since the allocator always allocates power-of-two blocks.
	 */
	if (size < alignment)
		size = alignment;
	result = malloc(size);

	if (result == NULL)
		return ENOMEM;

	*memptr = result;
	return 0;
}

