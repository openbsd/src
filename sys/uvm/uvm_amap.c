/*	$OpenBSD: uvm_amap.c,v 1.82 2020/01/04 16:17:29 beck Exp $	*/
/*	$NetBSD: uvm_amap.c,v 1.27 2000/11/25 06:27:59 chs Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * uvm_amap.c: amap operations
 *
 * this file contains functions that perform operations on amaps.  see
 * uvm_amap.h for a brief explanation of the role of amaps in uvm.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>
#include <uvm/uvm_swap.h>

/*
 * pools for allocation of vm_amap structures.  note that in order to
 * avoid an endless loop, the amap pool's allocator cannot allocate
 * memory from an amap (it currently goes through the kernel uobj, so
 * we are ok).
 */

struct pool uvm_amap_pool;
struct pool uvm_small_amap_pool[UVM_AMAP_CHUNK];
struct pool uvm_amap_chunk_pool;

LIST_HEAD(, vm_amap) amap_list;

static char amap_small_pool_names[UVM_AMAP_CHUNK][9];

/*
 * local functions
 */

static struct vm_amap *amap_alloc1(int, int, int);
static __inline void amap_list_insert(struct vm_amap *);
static __inline void amap_list_remove(struct vm_amap *);   

struct vm_amap_chunk *amap_chunk_get(struct vm_amap *, int, int, int);
void amap_chunk_free(struct vm_amap *, struct vm_amap_chunk *);
void amap_wiperange_chunk(struct vm_amap *, struct vm_amap_chunk *, int, int);

static __inline void
amap_list_insert(struct vm_amap *amap)
{
	LIST_INSERT_HEAD(&amap_list, amap, am_list);
}

static __inline void
amap_list_remove(struct vm_amap *amap)
{ 
	LIST_REMOVE(amap, am_list);
}

/*
 * amap_chunk_get: lookup a chunk for slot. if create is non-zero,
 * the chunk is created if it does not yet exist.
 *
 * => returns the chunk on success or NULL on error
 */
struct vm_amap_chunk *
amap_chunk_get(struct vm_amap *amap, int slot, int create, int waitf)
{
	int bucket = UVM_AMAP_BUCKET(amap, slot);
	int baseslot = AMAP_BASE_SLOT(slot);
	int n;
	struct vm_amap_chunk *chunk, *newchunk, *pchunk = NULL;

	if (UVM_AMAP_SMALL(amap))
		return &amap->am_small;

	for (chunk = amap->am_buckets[bucket]; chunk != NULL;
	    chunk = TAILQ_NEXT(chunk, ac_list)) {
		if (UVM_AMAP_BUCKET(amap, chunk->ac_baseslot) != bucket)
			break;
		if (chunk->ac_baseslot == baseslot)
			return chunk;
		pchunk = chunk;
	}
	if (!create)
		return NULL;

	if (amap->am_nslot - baseslot >= UVM_AMAP_CHUNK)
		n = UVM_AMAP_CHUNK;
	else
		n = amap->am_nslot - baseslot;

	newchunk = pool_get(&uvm_amap_chunk_pool, waitf | PR_ZERO);
	if (newchunk == NULL)
		return NULL;

	if (pchunk == NULL) {
		TAILQ_INSERT_TAIL(&amap->am_chunks, newchunk, ac_list);
		KASSERT(amap->am_buckets[bucket] == NULL);
		amap->am_buckets[bucket] = newchunk;
	} else
		TAILQ_INSERT_AFTER(&amap->am_chunks, pchunk, newchunk,
		    ac_list);

	amap->am_ncused++;
	newchunk->ac_baseslot = baseslot;
	newchunk->ac_nslot = n;
	return newchunk;
}

void
amap_chunk_free(struct vm_amap *amap, struct vm_amap_chunk *chunk)
{
	int bucket = UVM_AMAP_BUCKET(amap, chunk->ac_baseslot);
	struct vm_amap_chunk *nchunk;

	if (UVM_AMAP_SMALL(amap))
		return;

	nchunk = TAILQ_NEXT(chunk, ac_list);
	TAILQ_REMOVE(&amap->am_chunks, chunk, ac_list);
	if (amap->am_buckets[bucket] == chunk) {
		if (nchunk != NULL &&
		    UVM_AMAP_BUCKET(amap, nchunk->ac_baseslot) == bucket)
			amap->am_buckets[bucket] = nchunk;
		else
			amap->am_buckets[bucket] = NULL;

	}
	pool_put(&uvm_amap_chunk_pool, chunk);
	amap->am_ncused--;
}

#ifdef UVM_AMAP_PPREF
/*
 * what is ppref?   ppref is an _optional_ amap feature which is used
 * to keep track of reference counts on a per-page basis.  it is enabled
 * when UVM_AMAP_PPREF is defined.
 *
 * when enabled, an array of ints is allocated for the pprefs.  this
 * array is allocated only when a partial reference is added to the
 * map (either by unmapping part of the amap, or gaining a reference
 * to only a part of an amap).  if the malloc of the array fails
 * (M_NOWAIT), then we set the array pointer to PPREF_NONE to indicate
 * that we tried to do ppref's but couldn't alloc the array so just
 * give up (after all, this is an optional feature!).
 *
 * the array is divided into page sized "chunks."   for chunks of length 1,
 * the chunk reference count plus one is stored in that chunk's slot.
 * for chunks of length > 1 the first slot contains (the reference count
 * plus one) * -1.    [the negative value indicates that the length is
 * greater than one.]   the second slot of the chunk contains the length
 * of the chunk.   here is an example:
 *
 * actual REFS:  2  2  2  2  3  1  1  0  0  0  4  4  0  1  1  1
 *       ppref: -3  4  x  x  4 -2  2 -1  3  x -5  2  1 -2  3  x
 *              <----------><-><----><-------><----><-><------->
 * (x = don't care)
 *
 * this allows us to allow one int to contain the ref count for the whole
 * chunk.    note that the "plus one" part is needed because a reference
 * count of zero is neither positive or negative (need a way to tell
 * if we've got one zero or a bunch of them).
 * 
 * here are some in-line functions to help us.
 */

static __inline void pp_getreflen(int *, int, int *, int *);
static __inline void pp_setreflen(int *, int, int, int);

/*
 * pp_getreflen: get the reference and length for a specific offset
 */
static __inline void
pp_getreflen(int *ppref, int offset, int *refp, int *lenp)
{

	if (ppref[offset] > 0) {		/* chunk size must be 1 */
		*refp = ppref[offset] - 1;	/* don't forget to adjust */
		*lenp = 1;
	} else {
		*refp = (ppref[offset] * -1) - 1;
		*lenp = ppref[offset+1];
	}
}

/*
 * pp_setreflen: set the reference and length for a specific offset
 */
static __inline void
pp_setreflen(int *ppref, int offset, int ref, int len)
{
	if (len == 1) {
		ppref[offset] = ref + 1;
	} else {
		ppref[offset] = (ref + 1) * -1;
		ppref[offset+1] = len;
	}
}
#endif

/*
 * amap_init: called at boot time to init global amap data structures
 */

void
amap_init(void)
{
	int i;
	size_t size;

	/* Initialize the vm_amap pool. */
	pool_init(&uvm_amap_pool, sizeof(struct vm_amap),
	    0, IPL_NONE, PR_WAITOK, "amappl", NULL);
	pool_sethiwat(&uvm_amap_pool, 4096);

	/* initialize small amap pools */
	for (i = 0; i < nitems(uvm_small_amap_pool); i++) {
		snprintf(amap_small_pool_names[i],
		    sizeof(amap_small_pool_names[0]), "amappl%d", i + 1);
		size = offsetof(struct vm_amap, am_small.ac_anon) +
		    (i + 1) * sizeof(struct vm_anon *);
		pool_init(&uvm_small_amap_pool[i], size, 0,
		    IPL_NONE, 0, amap_small_pool_names[i], NULL);
	}

	pool_init(&uvm_amap_chunk_pool, sizeof(struct vm_amap_chunk) +
	    UVM_AMAP_CHUNK * sizeof(struct vm_anon *),
	    0, IPL_NONE, 0, "amapchunkpl", NULL);
	pool_sethiwat(&uvm_amap_chunk_pool, 4096);
}

/*
 * amap_alloc1: internal function that allocates an amap, but does not
 *	init the overlay.
 */
static inline struct vm_amap *
amap_alloc1(int slots, int waitf, int lazyalloc)
{
	struct vm_amap *amap;
	struct vm_amap_chunk *chunk, *tmp;
	int chunks, log_chunks, chunkperbucket = 1, hashshift = 0;
	int buckets, i, n;
	int pwaitf = (waitf & M_WAITOK) ? PR_WAITOK : PR_NOWAIT;

	KASSERT(slots > 0);

	/*
	 * Cast to unsigned so that rounding up cannot cause integer overflow
	 * if slots is large.
	 */
	chunks = roundup((unsigned int)slots, UVM_AMAP_CHUNK) / UVM_AMAP_CHUNK;

	if (lazyalloc) {
		/*
		 * Basically, the amap is a hash map where the number of
		 * buckets is fixed. We select the number of buckets using the
		 * following strategy:
		 *
		 * 1. The maximal number of entries to search in a bucket upon
		 * a collision should be less than or equal to
		 * log2(slots / UVM_AMAP_CHUNK). This is the worst-case number
		 * of lookups we would have if we could chunk the amap. The
		 * log2(n) comes from the fact that amaps are chunked by
		 * splitting up their vm_map_entries and organizing those
		 * in a binary search tree.
		 *
		 * 2. The maximal number of entries in a bucket must be a
		 * power of two.
		 *
		 * The maximal number of entries per bucket is used to hash
		 * a slot to a bucket.
		 *
		 * In the future, this strategy could be refined to make it
		 * even harder/impossible that the total amount of KVA needed
		 * for the hash buckets of all amaps to exceed the maximal
		 * amount of KVA memory reserved for amaps.
		 */
		for (log_chunks = 1; (chunks >> log_chunks) > 0; log_chunks++)
			continue;

		chunkperbucket = 1 << hashshift;
		while (chunkperbucket + 1 < log_chunks) {
			hashshift++;
			chunkperbucket = 1 << hashshift;
		}
	}

	if (slots > UVM_AMAP_CHUNK)
		amap = pool_get(&uvm_amap_pool, pwaitf);
	else
		amap = pool_get(&uvm_small_amap_pool[slots - 1],
		    pwaitf | PR_ZERO);
	if (amap == NULL)
		return(NULL);

	amap->am_ref = 1;
	amap->am_flags = 0;
#ifdef UVM_AMAP_PPREF
	amap->am_ppref = NULL;
#endif
	amap->am_nslot = slots;
	amap->am_nused = 0;

	if (UVM_AMAP_SMALL(amap)) {
		amap->am_small.ac_nslot = slots;
		return (amap);
	}

	amap->am_ncused = 0;
	TAILQ_INIT(&amap->am_chunks);
	amap->am_hashshift = hashshift;
	amap->am_buckets = NULL;

	buckets = howmany(chunks, chunkperbucket);
	amap->am_buckets = mallocarray(buckets, sizeof(*amap->am_buckets),
	    M_UVMAMAP, waitf | (lazyalloc ? M_ZERO : 0));
	if (amap->am_buckets == NULL)
		goto fail1;
	amap->am_nbuckets = buckets;

	if (!lazyalloc) {
		for (i = 0; i < buckets; i++) {
			if (i == buckets - 1) {
				n = slots % UVM_AMAP_CHUNK;
				if (n == 0)
					n = UVM_AMAP_CHUNK;
			} else
				n = UVM_AMAP_CHUNK;

			chunk = pool_get(&uvm_amap_chunk_pool,
			    PR_ZERO | pwaitf);
			if (chunk == NULL)
				goto fail1;

			amap->am_buckets[i] = chunk;
			amap->am_ncused++;
			chunk->ac_baseslot = i * UVM_AMAP_CHUNK;
			chunk->ac_nslot = n;
			TAILQ_INSERT_TAIL(&amap->am_chunks, chunk, ac_list);
		}
	}

	return(amap);

fail1:
	free(amap->am_buckets, M_UVMAMAP, buckets * sizeof(*amap->am_buckets));
	TAILQ_FOREACH_SAFE(chunk, &amap->am_chunks, ac_list, tmp)
		pool_put(&uvm_amap_chunk_pool, chunk);
	pool_put(&uvm_amap_pool, amap);
	return (NULL);
}

/*
 * amap_alloc: allocate an amap to manage "sz" bytes of anonymous VM
 *
 * => caller should ensure sz is a multiple of PAGE_SIZE
 * => reference count to new amap is set to one
 */
struct vm_amap *
amap_alloc(vaddr_t sz, int waitf, int lazyalloc)
{
	struct vm_amap *amap;
	size_t slots;

	AMAP_B2SLOT(slots, sz);		/* load slots */
	if (slots > INT_MAX)
		return (NULL);

	amap = amap_alloc1(slots, waitf, lazyalloc);
	if (amap)
		amap_list_insert(amap);

	return(amap);
}


/*
 * amap_free: free an amap
 *
 * => the amap should have a zero reference count and be empty
 */
void
amap_free(struct vm_amap *amap)
{
	struct vm_amap_chunk *chunk, *tmp;

	KASSERT(amap->am_ref == 0 && amap->am_nused == 0);
	KASSERT((amap->am_flags & AMAP_SWAPOFF) == 0);

#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE)
		free(amap->am_ppref, M_UVMAMAP, amap->am_nslot * sizeof(int));
#endif

	if (UVM_AMAP_SMALL(amap))
		pool_put(&uvm_small_amap_pool[amap->am_nslot - 1], amap);
	else {
		TAILQ_FOREACH_SAFE(chunk, &amap->am_chunks, ac_list, tmp)
		    pool_put(&uvm_amap_chunk_pool, chunk);
		free(amap->am_buckets, M_UVMAMAP,
		    amap->am_nbuckets * sizeof(*amap->am_buckets));
		pool_put(&uvm_amap_pool, amap);
	}
}

/*
 * amap_wipeout: wipeout all anon's in an amap; then free the amap!
 *
 * => called from amap_unref when the final reference to an amap is
 *	discarded (i.e. when reference count == 1)
 */

void
amap_wipeout(struct vm_amap *amap)
{
	int slot;
	struct vm_anon *anon;
	struct vm_amap_chunk *chunk;
	struct pglist pgl;

	KASSERT(amap->am_ref == 0);

	if (__predict_false((amap->am_flags & AMAP_SWAPOFF) != 0)) {
		/* amap_swap_off will call us again. */
		return;
	}

	TAILQ_INIT(&pgl);

	amap_list_remove(amap);

	AMAP_CHUNK_FOREACH(chunk, amap) {
		int i, refs, map = chunk->ac_usedmap;

		for (i = ffs(map); i != 0; i = ffs(map)) {
			slot = i - 1;
			map ^= 1 << slot;
			anon = chunk->ac_anon[slot];

			if (anon == NULL || anon->an_ref == 0)
				panic("amap_wipeout: corrupt amap");

			refs = --anon->an_ref;
			if (refs == 0) {
				/*
				 * we had the last reference to a vm_anon.
				 * free it.
				 */
				uvm_anfree_list(anon, &pgl);
			}
		}
	}
	/* free the pages */
	uvm_pglistfree(&pgl);

	/* now we free the map */
	amap->am_ref = 0;	/* ... was one */
	amap->am_nused = 0;
	amap_free(amap);	/* will free amap */
}

/*
 * amap_copy: ensure that a map entry's "needs_copy" flag is false
 *	by copying the amap if necessary.
 * 
 * => an entry with a null amap pointer will get a new (blank) one.
 * => if canchunk is true, then we may clip the entry into a chunk
 * => "startva" and "endva" are used only if canchunk is true.  they are
 *     used to limit chunking (e.g. if you have a large space that you
 *     know you are going to need to allocate amaps for, there is no point
 *     in allowing that to be chunked)
 */

void
amap_copy(struct vm_map *map, struct vm_map_entry *entry, int waitf,
    boolean_t canchunk, vaddr_t startva, vaddr_t endva)
{
	struct vm_amap *amap, *srcamap;
	int slots, lcv, lazyalloc = 0;
	vaddr_t chunksize;
	int i, j, k, n, srcslot;
	struct vm_amap_chunk *chunk = NULL, *srcchunk = NULL;

	/* is there a map to copy?   if not, create one from scratch. */
	if (entry->aref.ar_amap == NULL) {
		/*
		 * check to see if we have a large amap that we can
		 * chunk.  we align startva/endva to chunk-sized
		 * boundaries and then clip to them.
		 *
		 * if we cannot chunk the amap, allocate it in a way
		 * that makes it grow or shrink dynamically with
		 * the number of slots.
		 */
		if (atop(entry->end - entry->start) >= UVM_AMAP_LARGE) {
			if (canchunk) {
				/* convert slots to bytes */
				chunksize = UVM_AMAP_CHUNK << PAGE_SHIFT;
				startva = (startva / chunksize) * chunksize;
				endva = roundup(endva, chunksize);
				UVM_MAP_CLIP_START(map, entry, startva);
				/* watch out for endva wrap-around! */
				if (endva >= startva)
					UVM_MAP_CLIP_END(map, entry, endva);
			} else
				lazyalloc = 1;
		}

		entry->aref.ar_pageoff = 0;
		entry->aref.ar_amap = amap_alloc(entry->end - entry->start,
		    waitf, lazyalloc);
		if (entry->aref.ar_amap != NULL)
			entry->etype &= ~UVM_ET_NEEDSCOPY;
		return;
	}

	/*
	 * first check and see if we are the only map entry
	 * referencing the amap we currently have.  if so, then we can
	 * just take it over rather than copying it.  the value can only
	 * be one if we have the only reference to the amap
	 */
	if (entry->aref.ar_amap->am_ref == 1) {
		entry->etype &= ~UVM_ET_NEEDSCOPY;
		return;
	}

	/* looks like we need to copy the map. */
	AMAP_B2SLOT(slots, entry->end - entry->start);
	if (!UVM_AMAP_SMALL(entry->aref.ar_amap) &&
	    entry->aref.ar_amap->am_hashshift != 0)
		lazyalloc = 1;
	amap = amap_alloc1(slots, waitf, lazyalloc);
	if (amap == NULL)
		return;
	srcamap = entry->aref.ar_amap;

	/*
	 * need to double check reference count now.  the reference count
	 * could have changed while we were in malloc.  if the reference count
	 * dropped down to one we take over the old map rather than
	 * copying the amap.
	 */
	if (srcamap->am_ref == 1) {		/* take it over? */
		entry->etype &= ~UVM_ET_NEEDSCOPY;
		amap->am_ref--;		/* drop final reference to map */
		amap_free(amap);	/* dispose of new (unused) amap */
		return;
	}

	/* we must copy it now. */
	for (lcv = 0; lcv < slots; lcv += n) {
		srcslot = entry->aref.ar_pageoff + lcv;
		i = UVM_AMAP_SLOTIDX(lcv);
		j = UVM_AMAP_SLOTIDX(srcslot);
		n = UVM_AMAP_CHUNK;
		if (i > j)
			n -= i;
		else
			n -= j;
		if (lcv + n > slots)
			n = slots - lcv;

		srcchunk = amap_chunk_get(srcamap, srcslot, 0, PR_NOWAIT);
		if (srcchunk == NULL)
			continue;

		chunk = amap_chunk_get(amap, lcv, 1, PR_NOWAIT);
		if (chunk == NULL) {
			amap->am_ref = 0;
			amap_wipeout(amap);
			return;
		}

		for (k = 0; k < n; i++, j++, k++) {
			chunk->ac_anon[i] = srcchunk->ac_anon[j];
			if (chunk->ac_anon[i] == NULL)
				continue;

			chunk->ac_usedmap |= (1 << i);
			chunk->ac_anon[i]->an_ref++;
			amap->am_nused++;
		}
	}

	/*
	 * drop our reference to the old amap (srcamap).
	 * we know that the reference count on srcamap is greater than
	 * one (we checked above), so there is no way we could drop
	 * the count to zero.  [and no need to worry about freeing it]
	 */
	srcamap->am_ref--;
	if (srcamap->am_ref == 1 && (srcamap->am_flags & AMAP_SHARED) != 0)
		srcamap->am_flags &= ~AMAP_SHARED;   /* clear shared flag */
#ifdef UVM_AMAP_PPREF
	if (srcamap->am_ppref && srcamap->am_ppref != PPREF_NONE) {
		amap_pp_adjref(srcamap, entry->aref.ar_pageoff, 
		    (entry->end - entry->start) >> PAGE_SHIFT, -1);
	}
#endif

	/* install new amap. */
	entry->aref.ar_pageoff = 0;
	entry->aref.ar_amap = amap;
	entry->etype &= ~UVM_ET_NEEDSCOPY;

	amap_list_insert(amap);
}

/*
 * amap_cow_now: resolve all copy-on-write faults in an amap now for fork(2)
 *
 *	called during fork(2) when the parent process has a wired map
 *	entry.   in that case we want to avoid write-protecting pages
 *	in the parent's map (e.g. like what you'd do for a COW page)
 *	so we resolve the COW here.
 *
 * => assume parent's entry was wired, thus all pages are resident.
 * => caller passes child's map/entry in to us
 * => XXXCDC: out of memory should cause fork to fail, but there is
 *	currently no easy way to do this (needs fix)
 */

void
amap_cow_now(struct vm_map *map, struct vm_map_entry *entry)
{
	struct vm_amap *amap = entry->aref.ar_amap;
	int slot;
	struct vm_anon *anon, *nanon;
	struct vm_page *pg, *npg;
	struct vm_amap_chunk *chunk;

	/*
	 * note that if we wait, we must ReStart the "lcv" for loop because
	 * some other process could reorder the anon's in the
	 * am_anon[] array on us.
	 */
ReStart:
	AMAP_CHUNK_FOREACH(chunk, amap) {
		int i, map = chunk->ac_usedmap;

		for (i = ffs(map); i != 0; i = ffs(map)) {
			slot = i - 1;
			map ^= 1 << slot;
			anon = chunk->ac_anon[slot];
			pg = anon->an_page;

			/* page must be resident since parent is wired */
			if (pg == NULL)
				panic("amap_cow_now: non-resident wired page"
				    " in anon %p", anon);

			/*
			 * if the anon ref count is one, we are safe (the child
			 * has exclusive access to the page).
			 */
			if (anon->an_ref <= 1)
				continue;

			/*
			 * if the page is busy then we have to wait for
			 * it and then restart.
			 */
			if (pg->pg_flags & PG_BUSY) {
				atomic_setbits_int(&pg->pg_flags, PG_WANTED);
				tsleep_nsec(pg, PVM, "cownow", INFSLP);
				goto ReStart;
			}

			/* ok, time to do a copy-on-write to a new anon */
			nanon = uvm_analloc();
			if (nanon) {
				npg = uvm_pagealloc(NULL, 0, nanon, 0);
			} else
				npg = NULL;	/* XXX: quiet gcc warning */

			if (nanon == NULL || npg == NULL) {
				/* out of memory */
				/*
				 * XXXCDC: we should cause fork to fail, but
				 * we can't ...
				 */
				if (nanon) {
					uvm_anfree(nanon);
				}
				uvm_wait("cownowpage");
				goto ReStart;
			}

			/*
			 * got it... now we can copy the data and replace anon
			 * with our new one...
			 */
			uvm_pagecopy(pg, npg);		/* old -> new */
			anon->an_ref--;			/* can't drop to zero */
			chunk->ac_anon[slot] = nanon;	/* replace */

			/*
			 * drop PG_BUSY on new page ... since we have had its
			 * owner locked the whole time it can't be
			 * PG_RELEASED | PG_WANTED.
			 */
			atomic_clearbits_int(&npg->pg_flags, PG_BUSY|PG_FAKE);
			UVM_PAGE_OWN(npg, NULL);
			uvm_lock_pageq();
			uvm_pageactivate(npg);
			uvm_unlock_pageq();
		}
	}
}

/*
 * amap_splitref: split a single reference into two separate references
 *
 * => called from uvm_map's clip routines
 */
void
amap_splitref(struct vm_aref *origref, struct vm_aref *splitref, vaddr_t offset)
{
	int leftslots;

	AMAP_B2SLOT(leftslots, offset);
	if (leftslots == 0)
		panic("amap_splitref: split at zero offset");

	/* now: we have a valid am_mapped array. */
	if (origref->ar_amap->am_nslot - origref->ar_pageoff - leftslots <= 0)
		panic("amap_splitref: map size check failed");

#ifdef UVM_AMAP_PPREF
        /* establish ppref before we add a duplicate reference to the amap */
	if (origref->ar_amap->am_ppref == NULL)
		amap_pp_establish(origref->ar_amap);
#endif

	splitref->ar_amap = origref->ar_amap;
	splitref->ar_amap->am_ref++;		/* not a share reference */
	splitref->ar_pageoff = origref->ar_pageoff + leftslots;
}

#ifdef UVM_AMAP_PPREF

/*
 * amap_pp_establish: add a ppref array to an amap, if possible
 */
void
amap_pp_establish(struct vm_amap *amap)
{

	amap->am_ppref = mallocarray(amap->am_nslot, sizeof(int),
	    M_UVMAMAP, M_NOWAIT|M_ZERO);

	/* if we fail then we just won't use ppref for this amap */
	if (amap->am_ppref == NULL) {
		amap->am_ppref = PPREF_NONE;	/* not using it */
		return;
	}

	/* init ppref */
	pp_setreflen(amap->am_ppref, 0, amap->am_ref, amap->am_nslot);
}

/*
 * amap_pp_adjref: adjust reference count to a part of an amap using the
 * per-page reference count array.
 *
 * => caller must check that ppref != PPREF_NONE before calling
 */
void
amap_pp_adjref(struct vm_amap *amap, int curslot, vsize_t slotlen, int adjval)
{
 	int stopslot, *ppref, lcv, prevlcv;
 	int ref, len, prevref, prevlen;

	stopslot = curslot + slotlen;
	ppref = amap->am_ppref;
 	prevlcv = 0;

	/*
 	 * first advance to the correct place in the ppref array,
 	 * fragment if needed.
	 */
	for (lcv = 0 ; lcv < curslot ; lcv += len) {
		pp_getreflen(ppref, lcv, &ref, &len);
		if (lcv + len > curslot) {     /* goes past start? */
			pp_setreflen(ppref, lcv, ref, curslot - lcv);
			pp_setreflen(ppref, curslot, ref, len - (curslot -lcv));
			len = curslot - lcv;   /* new length of entry @ lcv */
		}
		prevlcv = lcv;
	}
	if (lcv != 0)
		pp_getreflen(ppref, prevlcv, &prevref, &prevlen);
	else {
		/* Ensure that the "prevref == ref" test below always
		 * fails, since we're starting from the beginning of
		 * the ppref array; that is, there is no previous
		 * chunk.  
		 */
		prevref = -1;
		prevlen = 0;
	}

	/*
	 * now adjust reference counts in range.  merge the first
	 * changed entry with the last unchanged entry if possible.
	 */
	if (lcv != curslot)
		panic("amap_pp_adjref: overshot target");

	for (/* lcv already set */; lcv < stopslot ; lcv += len) {
		pp_getreflen(ppref, lcv, &ref, &len);
		if (lcv + len > stopslot) {     /* goes past end? */
			pp_setreflen(ppref, lcv, ref, stopslot - lcv);
			pp_setreflen(ppref, stopslot, ref,
			    len - (stopslot - lcv));
			len = stopslot - lcv;
		}
		ref += adjval;
		if (ref < 0)
			panic("amap_pp_adjref: negative reference count");
		if (lcv == prevlcv + prevlen && ref == prevref) {
			pp_setreflen(ppref, prevlcv, ref, prevlen + len);
		} else {
			pp_setreflen(ppref, lcv, ref, len);
		}
		if (ref == 0)
			amap_wiperange(amap, lcv, len);
	}

}

void
amap_wiperange_chunk(struct vm_amap *amap, struct vm_amap_chunk *chunk,
    int slotoff, int slots)
{
	int curslot, i, map;
	int startbase, endbase;
	struct vm_anon *anon;

	startbase = AMAP_BASE_SLOT(slotoff);
	endbase = AMAP_BASE_SLOT(slotoff + slots - 1);

	map = chunk->ac_usedmap;
	if (startbase == chunk->ac_baseslot)
		map &= ~((1 << (slotoff - startbase)) - 1);
	if (endbase == chunk->ac_baseslot)
		map &= (1 << (slotoff + slots - endbase)) - 1;

	for (i = ffs(map); i != 0; i = ffs(map)) {
		int refs;

		curslot = i - 1;
		map ^= 1 << curslot;
		chunk->ac_usedmap ^= 1 << curslot;
		anon = chunk->ac_anon[curslot];

		/* remove it from the amap */
		chunk->ac_anon[curslot] = NULL;

		amap->am_nused--;

		/* drop anon reference count */
		refs = --anon->an_ref;
		if (refs == 0) {
			/*
			 * we just eliminated the last reference to an
			 * anon.  free it.
			 */
			uvm_anfree(anon);
		}
	}
}

/*
 * amap_wiperange: wipe out a range of an amap
 * [different from amap_wipeout because the amap is kept intact]
 */
void
amap_wiperange(struct vm_amap *amap, int slotoff, int slots)
{
	int bucket, startbucket, endbucket;
	struct vm_amap_chunk *chunk, *nchunk;

	startbucket = UVM_AMAP_BUCKET(amap, slotoff);
	endbucket = UVM_AMAP_BUCKET(amap, slotoff + slots - 1);

	/*
	 * we can either traverse the amap by am_chunks or by am_buckets
	 * depending on which is cheaper.    decide now.
	 */
	if (UVM_AMAP_SMALL(amap))
		amap_wiperange_chunk(amap, &amap->am_small, slotoff, slots);
	else if (endbucket + 1 - startbucket >= amap->am_ncused) {
		TAILQ_FOREACH_SAFE(chunk, &amap->am_chunks, ac_list, nchunk) {
			if (chunk->ac_baseslot + chunk->ac_nslot <= slotoff)
				continue;
			if (chunk->ac_baseslot >= slotoff + slots)
				continue;

			amap_wiperange_chunk(amap, chunk, slotoff, slots);
			if (chunk->ac_usedmap == 0)
				amap_chunk_free(amap, chunk);
		}
	} else {
		for (bucket = startbucket; bucket <= endbucket; bucket++) {
			for (chunk = amap->am_buckets[bucket]; chunk != NULL;
			    chunk = nchunk) {
				nchunk = TAILQ_NEXT(chunk, ac_list);

				if (UVM_AMAP_BUCKET(amap, chunk->ac_baseslot) !=
				    bucket)
					break;
				if (chunk->ac_baseslot + chunk->ac_nslot <=
				    slotoff)
					continue;
				if (chunk->ac_baseslot >= slotoff + slots)
					continue;

				amap_wiperange_chunk(amap, chunk, slotoff,
				    slots);
				if (chunk->ac_usedmap == 0)
					amap_chunk_free(amap, chunk);
			}
		}
	}
}

#endif

/*
 * amap_swap_off: pagein anonymous pages in amaps and drop swap slots.
 *
 * => note that we don't always traverse all anons.
 *    eg. amaps being wiped out, released anons.
 * => return TRUE if failed.
 */

boolean_t
amap_swap_off(int startslot, int endslot)
{
	struct vm_amap *am;
	struct vm_amap *am_next;
	boolean_t rv = FALSE;

	for (am = LIST_FIRST(&amap_list); am != NULL && !rv; am = am_next) {
		int i, map;
		struct vm_amap_chunk *chunk;

again:
		AMAP_CHUNK_FOREACH(chunk, am) {
			map = chunk->ac_usedmap;

			for (i = ffs(map); i != 0; i = ffs(map)) {
				int swslot;
				int slot = i - 1;
				struct vm_anon *anon;

				map ^= 1 << slot;
				anon = chunk->ac_anon[slot];

				swslot = anon->an_swslot;
				if (swslot < startslot || endslot <= swslot) {
					continue;
				}

				am->am_flags |= AMAP_SWAPOFF;

				rv = uvm_anon_pagein(anon);

				am->am_flags &= ~AMAP_SWAPOFF;
				if (rv || amap_refs(am) == 0)
					goto nextamap;
				goto again;
			}
		}

nextamap:
		am_next = LIST_NEXT(am, am_list);
		if (amap_refs(am) == 0)
			amap_wipeout(am);
	}

	return rv;
}

/*
 * amap_lookup: look up a page in an amap
 */
struct vm_anon *
amap_lookup(struct vm_aref *aref, vaddr_t offset)
{
	int slot;
	struct vm_amap *amap = aref->ar_amap;
	struct vm_amap_chunk *chunk;

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if (slot >= amap->am_nslot)
		panic("amap_lookup: offset out of range");

	chunk = amap_chunk_get(amap, slot, 0, PR_NOWAIT);
	if (chunk == NULL)
		return NULL;

	return chunk->ac_anon[UVM_AMAP_SLOTIDX(slot)];
}

/*
 * amap_lookups: look up a range of pages in an amap
 *
 * => XXXCDC: this interface is biased toward array-based amaps.  fix.
 */
void
amap_lookups(struct vm_aref *aref, vaddr_t offset,
    struct vm_anon **anons, int npages)
{
	int i, lcv, n, slot;
	struct vm_amap *amap = aref->ar_amap;
	struct vm_amap_chunk *chunk = NULL;

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if ((slot + (npages - 1)) >= amap->am_nslot)
		panic("amap_lookups: offset out of range");

	for (i = 0, lcv = slot; lcv < slot + npages; i += n, lcv += n) {
		n = UVM_AMAP_CHUNK - UVM_AMAP_SLOTIDX(lcv);
		if (lcv + n > slot + npages)
			n = slot + npages - lcv;

		chunk = amap_chunk_get(amap, lcv, 0, PR_NOWAIT);
		if (chunk == NULL)
			memset(&anons[i], 0, n * sizeof(*anons));
		else
			memcpy(&anons[i],
			    &chunk->ac_anon[UVM_AMAP_SLOTIDX(lcv)],
			    n * sizeof(*anons));
	}
}

/*
 * amap_populate: ensure that the amap can store an anon for the page at
 * offset. This function can sleep until memory to store the anon is
 * available.
 */
void
amap_populate(struct vm_aref *aref, vaddr_t offset)
{
	int slot;
	struct vm_amap *amap = aref->ar_amap;
	struct vm_amap_chunk *chunk;

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if (slot >= amap->am_nslot)
		panic("amap_populate: offset out of range");

	chunk = amap_chunk_get(amap, slot, 1, PR_WAITOK);
	KASSERT(chunk != NULL);
}

/*
 * amap_add: add (or replace) a page to an amap
 *
 * => returns 0 if adding the page was successful or 1 when not.
 */
int
amap_add(struct vm_aref *aref, vaddr_t offset, struct vm_anon *anon,
    boolean_t replace)
{
	int slot;
	struct vm_amap *amap = aref->ar_amap;
	struct vm_amap_chunk *chunk;

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if (slot >= amap->am_nslot)
		panic("amap_add: offset out of range");
	chunk = amap_chunk_get(amap, slot, 1, PR_NOWAIT);
	if (chunk == NULL)
		return 1;

	slot = UVM_AMAP_SLOTIDX(slot);
	if (replace) {
		if (chunk->ac_anon[slot] == NULL)
			panic("amap_add: replacing null anon");
		if (chunk->ac_anon[slot]->an_page != NULL &&
		    (amap->am_flags & AMAP_SHARED) != 0) {
			pmap_page_protect(chunk->ac_anon[slot]->an_page,
			    PROT_NONE);
			/*
			 * XXX: suppose page is supposed to be wired somewhere?
			 */
		}
	} else {   /* !replace */
		if (chunk->ac_anon[slot] != NULL)
			panic("amap_add: slot in use");

		chunk->ac_usedmap |= 1 << slot;
		amap->am_nused++;
	}
	chunk->ac_anon[slot] = anon;

	return 0;
}

/*
 * amap_unadd: remove a page from an amap
 */
void
amap_unadd(struct vm_aref *aref, vaddr_t offset)
{
	int slot;
	struct vm_amap *amap = aref->ar_amap;
	struct vm_amap_chunk *chunk;

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if (slot >= amap->am_nslot)
		panic("amap_unadd: offset out of range");
	chunk = amap_chunk_get(amap, slot, 0, PR_NOWAIT);
	if (chunk == NULL)
		panic("amap_unadd: chunk for slot %d not present", slot);

	slot = UVM_AMAP_SLOTIDX(slot);
	if (chunk->ac_anon[slot] == NULL)
		panic("amap_unadd: nothing there");

	chunk->ac_anon[slot] = NULL;
	chunk->ac_usedmap &= ~(1 << slot);
	amap->am_nused--;

	if (chunk->ac_usedmap == 0)
		amap_chunk_free(amap, chunk);
}

/*
 * amap_ref: gain a reference to an amap
 *
 * => "offset" and "len" are in units of pages
 * => called at fork time to gain the child's reference
 */
void
amap_ref(struct vm_amap *amap, vaddr_t offset, vsize_t len, int flags)
{

	amap->am_ref++;
	if (flags & AMAP_SHARED)
		amap->am_flags |= AMAP_SHARED;
#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref == NULL && (flags & AMAP_REFALL) == 0 &&
	    len != amap->am_nslot)
		amap_pp_establish(amap);
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
		if (flags & AMAP_REFALL)
			amap_pp_adjref(amap, 0, amap->am_nslot, 1);
		else
			amap_pp_adjref(amap, offset, len, 1);
	}
#endif
}

/*
 * amap_unref: remove a reference to an amap
 *
 * => caller must remove all pmap-level references to this amap before
 *	dropping the reference
 * => called from uvm_unmap_detach [only]  ... note that entry is no
 *	longer part of a map
 */
void
amap_unref(struct vm_amap *amap, vaddr_t offset, vsize_t len, boolean_t all)
{

	/* if we are the last reference, free the amap and return. */
	if (amap->am_ref-- == 1) {
		amap_wipeout(amap);	/* drops final ref and frees */
		return;
	}

	/* otherwise just drop the reference count(s) */
	if (amap->am_ref == 1 && (amap->am_flags & AMAP_SHARED) != 0)
		amap->am_flags &= ~AMAP_SHARED;	/* clear shared flag */
#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref == NULL && all == 0 && len != amap->am_nslot)
		amap_pp_establish(amap);
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
		if (all)
			amap_pp_adjref(amap, 0, amap->am_nslot, -1);
		else
			amap_pp_adjref(amap, offset, len, -1);
	}
#endif
}
