/*	$OpenBSD: kern_malloc.c,v 1.61 2005/11/19 02:18:01 pedro Exp $	*/
/*	$NetBSD: kern_malloc.c,v 1.15.4.2 1996/06/13 17:10:56 cgd Exp $	*/

/*
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

#include <uvm/uvm_extern.h>

static struct vm_map_intrsafe kmem_map_store;
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

struct kmembuckets bucket[MINBUCKET + 16];
struct kmemstats kmemstats[M_LAST];
struct kmemusage *kmemusage;
char *kmembase, *kmemlimit;
char buckstring[16 * sizeof("123456,")];
int buckstring_init = 0;
#if defined(KMEMSTATS) || defined(DIAGNOSTIC) || defined(FFS_SOFTUPDATES)
char *memname[] = INITKMEMNAMES;
char *memall = NULL;
extern struct lock sysctl_kmemlock;
#endif

#ifdef DIAGNOSTIC
/*
 * This structure provides a set of masks to catch unaligned frees.
 */
const long addrmask[] = { 0,
	0x00000001, 0x00000003, 0x00000007, 0x0000000f,
	0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
	0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
	0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
};

/*
 * The WEIRD_ADDR is used as known text to copy into free objects so
 * that modifications after frees can be detected.
 */
#define WEIRD_ADDR	((unsigned) 0xdeadbeef)
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

/*
 * Allocate a block of memory
 */
void *
malloc(size, type, flags)
	unsigned long size;
	int type, flags;
{
	register struct kmembuckets *kbp;
	register struct kmemusage *kup;
	register struct freelist *freep;
	long indx, npg, allocsize;
	int s;
	caddr_t va, cp, savedlist;
#ifdef DIAGNOSTIC
	int32_t *end, *lp;
	int copysize;
	char *savedtype;
#endif
#ifdef KMEMSTATS
	register struct kmemstats *ksp = &kmemstats[type];

	if (((unsigned long)type) >= M_LAST)
		panic("malloc - bogus type");
#endif

#ifdef MALLOC_DEBUG
	if (debug_malloc(size, type, flags, (void **)&va))
		return ((void *) va);
#endif

	if (size > 65535 * PAGE_SIZE)
		panic("malloc: allocation too large");
	indx = BUCKETINDX(size);
	kbp = &bucket[indx];
	s = splvm();
#ifdef KMEMSTATS
	while (ksp->ks_memuse >= ksp->ks_limit) {
		if (flags & M_NOWAIT) {
			splx(s);
			return ((void *) NULL);
		}
		if (ksp->ks_limblocks < 65535)
			ksp->ks_limblocks++;
		tsleep(ksp, PSWP+2, memname[type], 0);
	}
	ksp->ks_size |= 1 << indx;
#endif
#ifdef DIAGNOSTIC
	copysize = 1 << indx < MAX_COPY ? 1 << indx : MAX_COPY;
#endif
	if (kbp->kb_next == NULL) {
		kbp->kb_last = NULL;
		if (size > MAXALLOCSAVE)
			allocsize = round_page(size);
		else
			allocsize = 1 << indx;
		npg = btoc(allocsize);
		va = (caddr_t) uvm_km_kmemalloc(kmem_map, uvmexp.kmem_object,
		    (vsize_t)ctob(npg), 
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
		kbp->kb_total += kbp->kb_elmpercl;
#endif
		kup = btokup(va);
		kup->ku_indx = indx;
		if (allocsize > MAXALLOCSAVE) {
			kup->ku_pagecnt = npg;
#ifdef KMEMSTATS
			ksp->ks_memuse += allocsize;
#endif
			goto out;
		}
#ifdef KMEMSTATS
		kup->ku_freecnt = kbp->kb_elmpercl;
		kbp->kb_totalfree += kbp->kb_elmpercl;
#endif
		/*
		 * Just in case we blocked while allocating memory,
		 * and someone else also allocated memory for this
		 * bucket, don't assume the list is still empty.
		 */
		savedlist = kbp->kb_next;
		kbp->kb_next = cp = va + (npg * PAGE_SIZE) - allocsize;
		for (;;) {
			freep = (struct freelist *)cp;
#ifdef DIAGNOSTIC
			/*
			 * Copy in known text to detect modification
			 * after freeing.
			 */
			end = (int32_t *)&cp[copysize];
			for (lp = (int32_t *)cp; lp < end; lp++)
				*lp = WEIRD_ADDR;
			freep->type = M_FREE;
#endif /* DIAGNOSTIC */
			if (cp <= va)
				break;
			cp -= allocsize;
			freep->next = cp;
		}
		freep->next = savedlist;
		if (kbp->kb_last == NULL)
			kbp->kb_last = (caddr_t)freep;
	}
	va = kbp->kb_next;
	kbp->kb_next = ((struct freelist *)va)->next;
#ifdef DIAGNOSTIC
	freep = (struct freelist *)va;
	savedtype = (unsigned)freep->type < M_LAST ?
		memname[freep->type] : "???";
	if (kbp->kb_next) {
		int rv;
		vaddr_t addr = (vaddr_t)kbp->kb_next;

		vm_map_lock(kmem_map);
		rv = uvm_map_checkprot(kmem_map, addr,
		    addr + sizeof(struct freelist), VM_PROT_WRITE);
		vm_map_unlock(kmem_map);

		if (!rv)  {
		printf("%s %d of object %p size 0x%lx %s %s (invalid addr %p)\n",
			"Data modified on freelist: word", 
			(int32_t *)&kbp->kb_next - (int32_t *)kbp, va, size,
			"previous type", savedtype, kbp->kb_next);
		kbp->kb_next = NULL;
		}
	}

	/* Fill the fields that we've used with WEIRD_ADDR */
#if BYTE_ORDER == BIG_ENDIAN
	freep->type = WEIRD_ADDR >> 16;
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
	freep->type = (short)WEIRD_ADDR;
#endif
	end = (int32_t *)&freep->next +
	    (sizeof(freep->next) / sizeof(int32_t));
	for (lp = (int32_t *)&freep->next; lp < end; lp++)
		*lp = WEIRD_ADDR;

	/* and check that the data hasn't been modified. */
	end = (int32_t *)&va[copysize];
	for (lp = (int32_t *)va; lp < end; lp++) {
		if (*lp == WEIRD_ADDR)
			continue;
		printf("%s %d of object %p size 0x%lx %s %s (0x%x != 0x%x)\n",
			"Data modified on freelist: word", lp - (int32_t *)va,
			va, size, "previous type", savedtype, *lp, WEIRD_ADDR);
		break;
	}

	freep->spare0 = 0;
#endif /* DIAGNOSTIC */
#ifdef KMEMSTATS
	kup = btokup(va);
	if (kup->ku_indx != indx)
		panic("malloc: wrong bucket");
	if (kup->ku_freecnt == 0)
		panic("malloc: lost data");
	kup->ku_freecnt--;
	kbp->kb_totalfree--;
	ksp->ks_memuse += 1 << indx;
out:
	kbp->kb_calls++;
	ksp->ks_inuse++;
	ksp->ks_calls++;
	if (ksp->ks_memuse > ksp->ks_maxused)
		ksp->ks_maxused = ksp->ks_memuse;
#else
out:
#endif
	splx(s);
	return ((void *) va);
}

/*
 * Free a block of memory allocated by malloc.
 */
void
free(addr, type)
	void *addr;
	int type;
{
	register struct kmembuckets *kbp;
	register struct kmemusage *kup;
	register struct freelist *freep;
	long size;
	int s;
#ifdef DIAGNOSTIC
	caddr_t cp;
	int32_t *end, *lp;
	long alloc, copysize;
#endif
#ifdef KMEMSTATS
	register struct kmemstats *ksp = &kmemstats[type];
#endif

#ifdef MALLOC_DEBUG
	if (debug_free(addr, type))
		return;
#endif

#ifdef DIAGNOSTIC
	if (addr < (void *)kmembase || addr >= (void *)kmemlimit)
		panic("free: non-malloced addr %p type %s", addr,
		    memname[type]);
#endif

	kup = btokup(addr);
	size = 1 << kup->ku_indx;
	kbp = &bucket[kup->ku_indx];
	s = splvm();
#ifdef DIAGNOSTIC
	/*
	 * Check for returns of data that do not point to the
	 * beginning of the allocation.
	 */
	if (size > PAGE_SIZE)
		alloc = addrmask[BUCKETINDX(PAGE_SIZE)];
	else
		alloc = addrmask[kup->ku_indx];
	if (((u_long)addr & alloc) != 0)
		panic("free: unaligned addr %p, size %ld, type %s, mask %ld",
			addr, size, memname[type], alloc);
#endif /* DIAGNOSTIC */
	if (size > MAXALLOCSAVE) {
		uvm_km_free(kmem_map, (vaddr_t)addr, ctob(kup->ku_pagecnt));
#ifdef KMEMSTATS
		size = kup->ku_pagecnt << PGSHIFT;
		ksp->ks_memuse -= size;
		kup->ku_indx = 0;
		kup->ku_pagecnt = 0;
		if (ksp->ks_memuse + size >= ksp->ks_limit &&
		    ksp->ks_memuse < ksp->ks_limit)
			wakeup(ksp);
		ksp->ks_inuse--;
		kbp->kb_total -= 1;
#endif
		splx(s);
		return;
	}
	freep = (struct freelist *)addr;
#ifdef DIAGNOSTIC
	/*
	 * Check for multiple frees. Use a quick check to see if
	 * it looks free before laboriously searching the freelist.
	 */
	if (freep->spare0 == WEIRD_ADDR) {
		for (cp = kbp->kb_next; cp;
		    cp = ((struct freelist *)cp)->next) {
			if (addr != cp)
				continue;
			printf("multiply freed item %p\n", addr);
			panic("free: duplicated free");
		}
	}
	/*
	 * Copy in known text to detect modification after freeing
	 * and to make it look free. Also, save the type being freed
	 * so we can list likely culprit if modification is detected
	 * when the object is reallocated.
	 */
	copysize = size < MAX_COPY ? size : MAX_COPY;
	end = (int32_t *)&((caddr_t)addr)[copysize];
	for (lp = (int32_t *)addr; lp < end; lp++)
		*lp = WEIRD_ADDR;
	freep->type = type;
#endif /* DIAGNOSTIC */
#ifdef KMEMSTATS
	kup->ku_freecnt++;
	if (kup->ku_freecnt >= kbp->kb_elmpercl) {
		if (kup->ku_freecnt > kbp->kb_elmpercl)
			panic("free: multiple frees");
		else if (kbp->kb_totalfree > kbp->kb_highwat)
			kbp->kb_couldfree++;
	}
	kbp->kb_totalfree++;
	ksp->ks_memuse -= size;
	if (ksp->ks_memuse + size >= ksp->ks_limit &&
	    ksp->ks_memuse < ksp->ks_limit)
		wakeup(ksp);
	ksp->ks_inuse--;
#endif
	if (kbp->kb_next == NULL)
		kbp->kb_next = addr;
	else
		((struct freelist *)kbp->kb_last)->next = addr;
	freep->next = NULL;
	kbp->kb_last = addr;
	splx(s);
}

/*
 * Compute the number of pages that kmem_map will map, that is,
 * the size of the kernel malloc arena.
 */
void
kmeminit_nkmempages()
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
kmeminit()
{
	vaddr_t base, limit;
#ifdef KMEMSTATS
	long indx;
#endif

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
	    &kmem_map_store.vmi_map);
	kmembase = (char *)base;
	kmemlimit = (char *)limit;
	kmemusage = (struct kmemusage *) uvm_km_zalloc(kernel_map,
		(vsize_t)(nkmempages * sizeof(struct kmemusage)));
#ifdef KMEMSTATS
	for (indx = 0; indx < MINBUCKET + 16; indx++) {
		if (1 << indx >= PAGE_SIZE)
			bucket[indx].kb_elmpercl = 1;
		else
			bucket[indx].kb_elmpercl = PAGE_SIZE / (1 << indx);
		bucket[indx].kb_highwat = 5 * bucket[indx].kb_elmpercl;
	}
	for (indx = 0; indx < M_LAST; indx++)
		kmemstats[indx].ks_limit = nkmempages * PAGE_SIZE * 6 / 10;
#endif
#ifdef MALLOC_DEBUG
	debug_malloc_init();
#endif
}

/*
 * Return kernel malloc statistics information.
 */
int
sysctl_malloc(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
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
		kb.kb_next = kb.kb_last = 0;
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

			i = lockmgr(&sysctl_kmemlock, LK_EXCLUSIVE, NULL);
			if (i)
				return (i);

			/* Figure out how large a buffer we need */
			for (totlen = 0, i = 0; i < M_LAST; i++) {
				if (memname[i])
					totlen += strlen(memname[i]);
				totlen++;
			}
			memall = malloc(totlen + M_LAST, M_SYSCTL, M_WAITOK);
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
			lockmgr(&sysctl_kmemlock, LK_RELEASE, NULL);
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
