/*	$OpenBSD: kern_malloc.c,v 1.76 2008/10/05 11:12:19 miod Exp $	*/

/*
 * Copyright (c) 2008 Michael Shalayeff
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_malloc.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/pool.h>
#include <sys/rwlock.h>

#include <uvm/uvm_extern.h>

static struct vm_map kmem_map_store;
struct vm_map *kmem_map = NULL;

#ifdef NKMEMCLUSTERS
#error NKMEMCLUSTERS is obsolete; remove it from your kernel config file and use NKMEMPAGES instead or let the kernel auto-size
#endif

/*
 * Default number of pages in kmem_map.  We attempt to calculate this
 * at run-time, but allow it to be either patched or set in the kernel
 * config file.
 */
#ifndef NKMEMPAGES
#define	NKMEMPAGES	0
#endif
u_int	nkmempages = NKMEMPAGES;

/*
 * Defaults for lower- and upper-bounds for the kmem_map page count.
 * Can be overridden by kernel config options.
 */
#ifndef	NKMEMPAGES_MIN
#define	NKMEMPAGES_MIN	NKMEMPAGES_MIN_DEFAULT
#endif
u_int	nkmempages_min = 0;

#ifndef NKMEMPAGES_MAX
#define	NKMEMPAGES_MAX	NKMEMPAGES_MAX_DEFAULT
#endif
u_int	nkmempages_max = 0;

struct pool mallocpl[MINBUCKET + 16];
char mallocplnames[MINBUCKET + 16][8];	/* wchan for pool */
char mallocplwarn[MINBUCKET + 16][32];  /* warning message for hard limit */

struct kmembuckets bucket[MINBUCKET + 16];
struct kmemstats kmemstats[M_LAST];
struct kmemusage *kmemusage;
char *kmembase, *kmemlimit;
char buckstring[16 * sizeof("123456,")];
int buckstring_init = 0;
#if defined(KMEMSTATS) || defined(DIAGNOSTIC) || defined(FFS_SOFTUPDATES)
char *memname[] = INITKMEMNAMES;
char *memall = NULL;
struct rwlock sysctl_kmemlock = RWLOCK_INITIALIZER("sysctlklk");
#endif

#ifdef DIAGNOSTIC
/*
 * The WEIRD_ADDR is used as known text to copy into free objects so
 * that modifications after frees can be detected.
 */
#ifdef DEADBEEF0
#define WEIRD_ADDR	((unsigned) DEADBEEF0)
#else
#define WEIRD_ADDR	((unsigned) 0xdeadbeef)
#endif
#define MAX_COPY	32

/*
 * Normally the freelist structure is used only to hold the list pointer
 * for free objects.  However, when running with diagnostics, the first
 * 8 bytes of the structure is unused except for diagnostic information,
 * and the free list pointer is at offset 8 in the structure.  Since the
 * first 8 bytes is the portion of the structure most often modified, this
 * helps to detect memory reuse problems and avoid free list corruption.
 */
struct freelist {
	int32_t	spare0;
	int16_t	type;
	int16_t	spare1;
	caddr_t	next;
};
#else /* !DIAGNOSTIC */
struct freelist {
	caddr_t	next;
};
#endif /* DIAGNOSTIC */

#ifndef SMALL_KERNEL
struct timeval malloc_errintvl = { 5, 0 };
struct timeval malloc_lasterr;
#endif

void	*malloc_page_alloc(struct pool *, int);
void	malloc_page_free(struct pool *, void *);
struct pool_allocator pool_allocator_malloc = {
	malloc_page_alloc, malloc_page_free, 0,
};

void *
malloc_page_alloc(struct pool *pp, int flags)
{
	void *v = uvm_km_getpage(flags & M_NOWAIT? 0 : 1);
	struct vm_page *pg;
	paddr_t pa;

	if (!pmap_extract(pmap_kernel(), (vaddr_t)v, &pa))
		panic("malloc_page_alloc: pmap_extract failed");

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		panic("malloc_page_alloc: no page");
	pg->wire_count = BUCKETINDX(pp->pr_size);

	return v;
}

void
malloc_page_free(struct pool *pp, void *v)
{
	struct vm_page *pg;
	paddr_t pa;

	if (!pmap_extract(pmap_kernel(), (vaddr_t)v, &pa))
		panic("malloc_page_free: pmap_extract failed");

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		panic("malloc_page_free: no page");
	pg->wire_count = 0;
	uvm_km_putpage(v);
}

/*
 * Allocate a block of memory
 */
void *
malloc(unsigned long size, int type, int flags)
{
	struct kmembuckets *kbp;
	struct kmemusage *kup;
	vsize_t indx, allocsize;
	int s;
	void *va;
#ifdef KMEMSTATS
	struct kmemstats *ksp = &kmemstats[type];

	if (((unsigned long)type) >= M_LAST)
		panic("malloc - bogus type");
#endif

#ifdef MALLOC_DEBUG
	if (debug_malloc(size, type, flags, &va)) {
		if ((flags & M_ZERO) && va != NULL)
			memset(va, 0, size);
		return (va);
	}
#endif

	if (size > 65535 * PAGE_SIZE) {
		if (flags & M_CANFAIL) {
#ifndef SMALL_KERNEL
			if (ratecheck(&malloc_lasterr, &malloc_errintvl))
				printf("malloc(): allocation too large, "
				    "type = %d, size = %lu\n", type, size);
#endif
			return (NULL);
		} else
			panic("malloc: allocation too large");
	}

	indx = BUCKETINDX(size);
	kbp = &bucket[indx];
	s = splvm();
#ifdef KMEMSTATS
	while (ksp->ks_memuse >= ksp->ks_limit) {
		if (flags & M_NOWAIT) {
			splx(s);
			return (NULL);
		}
		if (ksp->ks_limblocks < 65535)
			ksp->ks_limblocks++;
		tsleep(ksp, PSWP+2, memname[type], 0);
	}
#endif
	if (size > MAXALLOCSAVE) {
		allocsize = round_page(size);
		va = (void *) uvm_km_kmemalloc(kmem_map, NULL, allocsize,
		    ((flags & M_NOWAIT) ? UVM_KMF_NOWAIT : 0) |
		    ((flags & M_CANFAIL) ? UVM_KMF_CANFAIL : 0));
		if (va == NULL) {
			/*
			 * Kmem_malloc() can return NULL, even if it can
			 * wait, if there is no map space available, because
			 * it can't fix that problem.  Neither can we,
			 * right now.  (We should release pages which
			 * are completely free and which are in buckets
			 * with too many free elements.)
			 */
			if ((flags & (M_NOWAIT|M_CANFAIL)) == 0)
				panic("malloc: out of space in kmem_map");
			splx(s);
			return (NULL);
		}
#ifdef KMEMSTATS
		kbp->kb_total++;
		kbp->kb_calls++;
#endif
		kup = btokup(va);
		kup->ku_indx = indx;
		kup->ku_pagecnt = atop(allocsize);
	} else {
		allocsize = mallocpl[indx].pr_size;
		va = pool_get(&mallocpl[indx], PR_LIMITFAIL |
		    (flags & M_NOWAIT ? 0 : PR_WAITOK));
		if (!va && (flags & (M_NOWAIT|M_CANFAIL)) == 0)
			panic("malloc: out of space in kmem pool");
	}

#ifdef KMEMSTATS
	if (va) {
		ksp->ks_memuse += allocsize;
		if (ksp->ks_memuse > ksp->ks_maxused)
			ksp->ks_maxused = ksp->ks_memuse;
		ksp->ks_size |= 1 << indx;
		ksp->ks_inuse++;
		ksp->ks_calls++;
	}
#endif
	splx(s);

	if ((flags & M_ZERO) && va != NULL)
		memset(va, 0, size);

	return (va);
}

/*
 * Free a block of memory allocated by malloc.
 */
void
free(void *addr, int type)
{
	struct kmembuckets *kbp;
	struct kmemusage *kup;
	struct vm_page *pg;
	paddr_t pa;
	long size;
	int s;
#ifdef KMEMSTATS
	struct kmemstats *ksp = &kmemstats[type];
#endif

#ifdef MALLOC_DEBUG
	if (debug_free(addr, type))
		return;
#endif

	s = splvm();
	if (addr >= (void *)kmembase && addr < (void *)kmemlimit) {
		kup = btokup(addr);
		kbp = &bucket[kup->ku_indx];
		size = ptoa(kup->ku_pagecnt);
#ifdef DIAGNOSTIC
		if ((vaddr_t)addr != round_page((vaddr_t)addr))
			panic("free: unaligned addr %p, size %ld, type %s",
			    addr, size, memname[type]);
#endif /* DIAGNOSTIC */
		uvm_km_free(kmem_map, (vaddr_t)addr, size);
#ifdef KMEMSTATS
		kup->ku_indx = 0;
		kup->ku_pagecnt = 0;
		kbp->kb_total--;
#endif
	} else {
		if (!pmap_extract(pmap_kernel(), (vaddr_t)addr, &pa))
			panic("free: pmap_extract failed");
		pg = PHYS_TO_VM_PAGE(pa);
		if (pg == NULL)
			panic("free: no page");
#ifdef DIAGNOSTIC
		if (pg->pg_flags & PQ_FREE)
			panic("free: page %p is free", pg);
		if (pg->wire_count < MINBUCKET ||
		    (1 << pg->wire_count) > MAXALLOCSAVE)
			panic("free: invalid page bucket %d", pg->wire_count);
#endif
		size = mallocpl[pg->wire_count].pr_size;
		pool_put(&mallocpl[pg->wire_count], addr);
	}

#ifdef KMEMSTATS
	ksp->ks_inuse--;
	ksp->ks_memuse -= size;
	if (ksp->ks_memuse + size >= ksp->ks_limit &&
	    ksp->ks_memuse < ksp->ks_limit)
		wakeup(ksp);		/* unnecessary for pool, whatever */
#endif

	splx(s);
}

/*
 * Compute the number of pages that kmem_map will map, that is,
 * the size of the kernel malloc arena.
 */
void
kmeminit_nkmempages(void)
{
	u_int npages;

	if (nkmempages != 0) {
		/*
		 * It's already been set (by us being here before, or
		 * by patching or kernel config options), bail out now.
		 */
		return;
	}

	/*
	 * We can't initialize these variables at compilation time, since
	 * the page size may not be known (on sparc GENERIC kernels, for
	 * example). But we still want the MD code to be able to provide
	 * better values.
	 */
	if (nkmempages_min == 0)
		nkmempages_min = NKMEMPAGES_MIN;
	if (nkmempages_max == 0)
		nkmempages_max = NKMEMPAGES_MAX;

	/*
	 * We use the following (simple) formula:
	 *
	 *	- Starting point is physical memory / 4.
	 *
	 *	- Clamp it down to nkmempages_max.
	 *
	 *	- Round it up to nkmempages_min.
	 */
	npages = physmem / 4;

	if (npages > nkmempages_max)
		npages = nkmempages_max;

	if (npages < nkmempages_min)
		npages = nkmempages_min;

	nkmempages = npages;
}

/*
 * Initialize the kernel memory allocator
 */
void
kmeminit(void)
{
	vaddr_t base, limit;
	int i;

#ifdef DIAGNOSTIC
	if (sizeof(struct freelist) > (1 << MINBUCKET))
		panic("kmeminit: minbucket too small/struct freelist too big");
#endif

	/*
	 * Compute the number of kmem_map pages, if we have not
	 * done so already.
	 */
	kmeminit_nkmempages();
	base = vm_map_min(kernel_map);
	kmem_map = uvm_km_suballoc(kernel_map, &base, &limit,
	    (vsize_t)(nkmempages * PAGE_SIZE), VM_MAP_INTRSAFE, FALSE,
	    &kmem_map_store);
	kmembase = (char *)base;
	kmemlimit = (char *)limit;
	kmemusage = (struct kmemusage *) uvm_km_zalloc(kernel_map,
		(vsize_t)(nkmempages * sizeof(struct kmemusage)));

	/*
	 * init all the sub-page pools
	 */
	for (i = MINBUCKET; (1 << i) <= MAXALLOCSAVE; i++) {
		snprintf(mallocplnames[i], sizeof(mallocplnames[i]),
		    "kmem%d", i);
		pool_init(&mallocpl[i], 1 << i, 1 << i, 0, PR_LIMITFAIL,
		    mallocplnames[i], &pool_allocator_malloc);
	}

#ifdef KMEMSTATS
	for (i = 0; i < MINBUCKET + 16; i++) {
		if (1 << i >= PAGE_SIZE)
			bucket[i].kb_elmpercl = 1;
		else
			bucket[i].kb_elmpercl = PAGE_SIZE / (1 << i);
		bucket[i].kb_highwat = 5 * bucket[i].kb_elmpercl;
	}
	for (i = 0; i < M_LAST; i++)
		kmemstats[i].ks_limit = nkmempages * PAGE_SIZE * 6 / 10;;
#endif
#ifdef MALLOC_DEBUG
	debug_malloc_init();
#endif
}

/*
 * Return kernel malloc statistics information.
 */
int
sysctl_malloc(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	struct kmembuckets kb;
	int i, siz;

	if (namelen != 2 && name[0] != KERN_MALLOC_BUCKETS &&
	    name[0] != KERN_MALLOC_KMEMNAMES)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case KERN_MALLOC_BUCKETS:
		/* Initialize the first time */
		if (buckstring_init == 0) {
			buckstring_init = 1;
			bzero(buckstring, sizeof(buckstring));
			for (siz = 0, i = MINBUCKET; i < MINBUCKET + 16; i++) {
				snprintf(buckstring + siz,
				    sizeof buckstring - siz,
				    "%d,", (u_int)(1<<i));
				siz += strlen(buckstring + siz);
			}
			/* Remove trailing comma */
			if (siz)
				buckstring[siz - 1] = '\0';
		}
		return (sysctl_rdstring(oldp, oldlenp, newp, buckstring));

	case KERN_MALLOC_BUCKET:
		bcopy(&bucket[BUCKETINDX(name[1])], &kb, sizeof(kb));
		return (sysctl_rdstruct(oldp, oldlenp, newp, &kb, sizeof(kb)));
	case KERN_MALLOC_KMEMSTATS:
#ifdef KMEMSTATS
		if ((name[1] < 0) || (name[1] >= M_LAST))
			return (EINVAL);
		return (sysctl_rdstruct(oldp, oldlenp, newp,
		    &kmemstats[name[1]], sizeof(struct kmemstats)));
#else
		return (EOPNOTSUPP);
#endif
	case KERN_MALLOC_KMEMNAMES:
#if defined(KMEMSTATS) || defined(DIAGNOSTIC) || defined(FFS_SOFTUPDATES)
		if (memall == NULL) {
			int totlen;

			i = rw_enter(&sysctl_kmemlock, RW_WRITE|RW_INTR);
			if (i)
				return (i);

			/* Figure out how large a buffer we need */
			for (totlen = 0, i = 0; i < M_LAST; i++) {
				if (memname[i])
					totlen += strlen(memname[i]);
				totlen++;
			}
			memall = malloc(totlen + M_LAST, M_SYSCTL,
			    M_WAITOK|M_ZERO);
			bzero(memall, totlen + M_LAST);
			for (siz = 0, i = 0; i < M_LAST; i++) {
				snprintf(memall + siz,
				    totlen + M_LAST - siz,
				    "%s,", memname[i] ? memname[i] : "");
				siz += strlen(memall + siz);
			}
			/* Remove trailing comma */
			if (siz)
				memall[siz - 1] = '\0';

			/* Now, convert all spaces to underscores */
			for (i = 0; i < totlen; i++)
				if (memall[i] == ' ')
					memall[i] = '_';
			rw_exit_write(&sysctl_kmemlock);
		}
		return (sysctl_rdstring(oldp, oldlenp, newp, memall));
#else
		return (EOPNOTSUPP);
#endif
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Round up a size to how much malloc would actually allocate.
 */
size_t
malloc_roundup(size_t sz)
{
	if (sz > MAXALLOCSAVE)
		return round_page(sz);

	return (1 << BUCKETINDX(sz));
}

#if defined(DDB)
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

void
malloc_printit(int (*pr)(const char *, ...))
{
#ifdef KMEMSTATS
	struct kmemstats *km;
	int i;

	(*pr)("%15s %5s  %6s  %7s  %6s %9s %8s %8s\n",
	    "Type", "InUse", "MemUse", "HighUse", "Limit", "Requests",
	    "Type Lim", "Kern Lim");
	for (i = 0, km = kmemstats; i < M_LAST; i++, km++) {
		if (!km->ks_calls || !memname[i])
			continue;

		(*pr)("%15s %5ld %6ldK %7ldK %6ldK %9ld %8d %8d\n",
		    memname[i], km->ks_inuse, km->ks_memuse / 1024,
		    km->ks_maxused / 1024, km->ks_limit / 1024,
		    km->ks_calls, km->ks_limblocks, km->ks_mapblocks);
	}
#else
	(*pr)("No KMEMSTATS compiled in\n");
#endif
}
#endif /* DDB */
