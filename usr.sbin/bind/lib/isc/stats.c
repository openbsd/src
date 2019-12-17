/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: stats.c,v 1.2 2019/12/17 01:46:34 sthen Exp $ */

/*! \file */

#include <config.h>

#include <string.h>

#include <isc/atomic.h>
#include <isc/buffer.h>
#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/platform.h>
#include <isc/print.h>
#include <isc/rwlock.h>
#include <isc/stats.h>
#include <isc/util.h>

#if defined(ISC_PLATFORM_HAVESTDATOMIC)
#include <stdatomic.h>
#endif

#define ISC_STATS_MAGIC			ISC_MAGIC('S', 't', 'a', 't')
#define ISC_STATS_VALID(x)		ISC_MAGIC_VALID(x, ISC_STATS_MAGIC)

/*%
 * Local macro confirming prescence of 64-bit
 * increment and store operations, just to make
 * the later macros simpler
 */
#if (defined(ISC_PLATFORM_HAVESTDATOMIC) && defined(ATOMIC_LONG_LOCK_FREE)) || \
	(defined(ISC_PLATFORM_HAVEXADDQ) && defined(ISC_PLATFORM_HAVEATOMICSTOREQ))
#define ISC_STATS_HAVEATOMICQ 1
#if (defined(ISC_PLATFORM_HAVESTDATOMIC) && defined(ATOMIC_LONG_LOCK_FREE))
#define ISC_STATS_HAVESTDATOMICQ 1
#endif
#else
#define ISC_STATS_HAVEATOMICQ 0
#endif

/*%
 * Only lock the counters if 64-bit atomic operations are
 * not available but cheap atomic lock operations are.
 * On a modern 64-bit system this should never be the case.
 *
 * Normal locks are too expensive to be used whenever a counter
 * is updated.
 */
#if !ISC_STATS_HAVEATOMICQ && defined(ISC_RWLOCK_HAVEATOMIC)
#define ISC_STATS_LOCKCOUNTERS 1
#else
#define ISC_STATS_LOCKCOUNTERS 0
#endif

/*%
 * If 64-bit atomic operations are not available but
 * 32-bit operations are then split the counter into two,
 * using the atomic operations to try to ensure that any carry
 * from the low word is correctly carried into the high word.
 *
 * Otherwise, just rely on standard 64-bit data types
 * and operations
 */
#if !ISC_STATS_HAVEATOMICQ && ((defined(ISC_PLATFORM_HAVESTDATOMIC) && defined(ATOMIC_INT_LOCK_FREE)) || defined(ISC_PLATFORM_HAVEXADD))
#define ISC_STATS_USEMULTIFIELDS 1
#if (defined(ISC_PLATFORM_HAVESTDATOMIC) && defined(ATOMIC_INT_LOCK_FREE))
#define ISC_STATS_HAVESTDATOMIC 1
#endif
#else
#define ISC_STATS_USEMULTIFIELDS 0
#endif

#if ISC_STATS_USEMULTIFIELDS
typedef struct {
#if defined(ISC_STATS_HAVESTDATOMIC)
	atomic_int_fast32_t hi;
	atomic_int_fast32_t lo;
#else
	isc_uint32_t hi;
	isc_uint32_t lo;
#endif
} isc_stat_t;
#else
#if defined(ISC_STATS_HAVESTDATOMICQ)
typedef atomic_int_fast64_t isc_stat_t;
#else
typedef isc_uint64_t isc_stat_t;
#endif
#endif

struct isc_stats {
	/*% Unlocked */
	unsigned int	magic;
	isc_mem_t	*mctx;
	int		ncounters;

	isc_mutex_t	lock;
	unsigned int	references; /* locked by lock */

	/*%
	 * Locked by counterlock or unlocked if efficient rwlock is not
	 * available.
	 */
#if ISC_STATS_LOCKCOUNTERS
	isc_rwlock_t	counterlock;
#endif
	isc_stat_t	*counters;

	/*%
	 * We don't want to lock the counters while we are dumping, so we first
	 * copy the current counter values into a local array.  This buffer
	 * will be used as the copy destination.  It's allocated on creation
	 * of the stats structure so that the dump operation won't fail due
	 * to memory allocation failure.
	 * XXX: this approach is weird for non-threaded build because the
	 * additional memory and the copy overhead could be avoided.  We prefer
	 * simplicity here, however, under the assumption that this function
	 * should be only rarely called.
	 */
	isc_uint64_t	*copiedcounters;
};

static isc_result_t
create_stats(isc_mem_t *mctx, int ncounters, isc_stats_t **statsp) {
	isc_stats_t *stats;
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(statsp != NULL && *statsp == NULL);

	stats = isc_mem_get(mctx, sizeof(*stats));
	if (stats == NULL)
		return (ISC_R_NOMEMORY);

	result = isc_mutex_init(&stats->lock);
	if (result != ISC_R_SUCCESS)
		goto clean_stats;

	stats->counters = isc_mem_get(mctx, sizeof(isc_stat_t) * ncounters);
	if (stats->counters == NULL) {
		result = ISC_R_NOMEMORY;
		goto clean_mutex;
	}
	stats->copiedcounters = isc_mem_get(mctx,
					    sizeof(isc_uint64_t) * ncounters);
	if (stats->copiedcounters == NULL) {
		result = ISC_R_NOMEMORY;
		goto clean_counters;
	}

#if ISC_STATS_LOCKCOUNTERS
	result = isc_rwlock_init(&stats->counterlock, 0, 0);
	if (result != ISC_R_SUCCESS)
		goto clean_copiedcounters;
#endif

	stats->references = 1;
	memset(stats->counters, 0, sizeof(isc_stat_t) * ncounters);
	stats->mctx = NULL;
	isc_mem_attach(mctx, &stats->mctx);
	stats->ncounters = ncounters;
	stats->magic = ISC_STATS_MAGIC;

	*statsp = stats;

	return (result);

clean_counters:
	isc_mem_put(mctx, stats->counters, sizeof(isc_stat_t) * ncounters);

#if ISC_STATS_LOCKCOUNTERS
clean_copiedcounters:
	isc_mem_put(mctx, stats->copiedcounters,
		    sizeof(isc_stat_t) * ncounters);
#endif

clean_mutex:
	DESTROYLOCK(&stats->lock);

clean_stats:
	isc_mem_put(mctx, stats, sizeof(*stats));

	return (result);
}

void
isc_stats_attach(isc_stats_t *stats, isc_stats_t **statsp) {
	REQUIRE(ISC_STATS_VALID(stats));
	REQUIRE(statsp != NULL && *statsp == NULL);

	LOCK(&stats->lock);
	stats->references++;
	UNLOCK(&stats->lock);

	*statsp = stats;
}

void
isc_stats_detach(isc_stats_t **statsp) {
	isc_stats_t *stats;

	REQUIRE(statsp != NULL && ISC_STATS_VALID(*statsp));

	stats = *statsp;
	*statsp = NULL;

	LOCK(&stats->lock);
	stats->references--;

	if (stats->references == 0) {
		isc_mem_put(stats->mctx, stats->copiedcounters,
			    sizeof(isc_stat_t) * stats->ncounters);
		isc_mem_put(stats->mctx, stats->counters,
			    sizeof(isc_stat_t) * stats->ncounters);
		UNLOCK(&stats->lock);
		DESTROYLOCK(&stats->lock);
#if ISC_STATS_LOCKCOUNTERS
		isc_rwlock_destroy(&stats->counterlock);
#endif
		isc_mem_putanddetach(&stats->mctx, stats, sizeof(*stats));
		return;
	}

	UNLOCK(&stats->lock);
}

int
isc_stats_ncounters(isc_stats_t *stats) {
	REQUIRE(ISC_STATS_VALID(stats));

	return (stats->ncounters);
}

static inline void
incrementcounter(isc_stats_t *stats, int counter) {
	isc_int32_t prev;

#if ISC_STATS_LOCKCOUNTERS
	/*
	 * We use a "read" lock to prevent other threads from reading the
	 * counter while we "writing" a counter field.  The write access itself
	 * is protected by the atomic operation.
	 */
	isc_rwlock_lock(&stats->counterlock, isc_rwlocktype_read);
#endif

#if ISC_STATS_USEMULTIFIELDS
#if defined(ISC_STATS_HAVESTDATOMIC)
	prev = atomic_fetch_add_explicit(&stats->counters[counter].lo, 1,
					 memory_order_relaxed);
#else
	prev = isc_atomic_xadd((isc_int32_t *)&stats->counters[counter].lo, 1);
#endif
	/*
	 * If the lower 32-bit field overflows, increment the higher field.
	 * Note that it's *theoretically* possible that the lower field
	 * overlaps again before the higher field is incremented.  It doesn't
	 * matter, however, because we don't read the value until
	 * isc_stats_copy() is called where the whole process is protected
	 * by the write (exclusive) lock.
	 */
	if (prev == (isc_int32_t)0xffffffff) {
#if defined(ISC_STATS_HAVESTDATOMIC)
		atomic_fetch_add_explicit(&stats->counters[counter].hi, 1,
					  memory_order_relaxed);
#else
		isc_atomic_xadd((isc_int32_t *)&stats->counters[counter].hi, 1);
#endif
	}
#elif ISC_STATS_HAVEATOMICQ
	UNUSED(prev);
#if defined(ISC_STATS_HAVESTDATOMICQ)
	atomic_fetch_add_explicit(&stats->counters[counter], 1,
				  memory_order_relaxed);
#else
	isc_atomic_xaddq((isc_int64_t *)&stats->counters[counter], 1);
#endif
#else
	UNUSED(prev);
	stats->counters[counter]++;
#endif

#if ISC_STATS_LOCKCOUNTERS
	isc_rwlock_unlock(&stats->counterlock, isc_rwlocktype_read);
#endif
}

static inline void
decrementcounter(isc_stats_t *stats, int counter) {
	isc_int32_t prev;

#if ISC_STATS_LOCKCOUNTERS
	isc_rwlock_lock(&stats->counterlock, isc_rwlocktype_read);
#endif

#if ISC_STATS_USEMULTIFIELDS
#if defined(ISC_STATS_HAVESTDATOMIC)
	prev = atomic_fetch_sub_explicit(&stats->counters[counter].lo, 1,
					 memory_order_relaxed);
#else
	prev = isc_atomic_xadd((isc_int32_t *)&stats->counters[counter].lo, -1);
#endif
	if (prev == 0) {
#if defined(ISC_STATS_HAVESTDATOMIC)
		atomic_fetch_sub_explicit(&stats->counters[counter].hi, 1,
					  memory_order_relaxed);
#else
		isc_atomic_xadd((isc_int32_t *)&stats->counters[counter].hi,
				-1);
#endif
	}
#elif ISC_STATS_HAVEATOMICQ
	UNUSED(prev);
#if defined(ISC_STATS_HAVESTDATOMICQ)
	atomic_fetch_sub_explicit(&stats->counters[counter], 1,
				  memory_order_relaxed);
#else
	isc_atomic_xaddq((isc_int64_t *)&stats->counters[counter], -1);
#endif
#else
	UNUSED(prev);
	stats->counters[counter]--;
#endif

#if ISC_STATS_LOCKCOUNTERS
	isc_rwlock_unlock(&stats->counterlock, isc_rwlocktype_read);
#endif
}

static void
copy_counters(isc_stats_t *stats) {
	int i;

#if ISC_STATS_LOCKCOUNTERS
	/*
	 * We use a "write" lock before "reading" the statistics counters as
	 * an exclusive lock.
	 */
	isc_rwlock_lock(&stats->counterlock, isc_rwlocktype_write);
#endif

	for (i = 0; i < stats->ncounters; i++) {
#if ISC_STATS_USEMULTIFIELDS
		stats->copiedcounters[i] =
			(isc_uint64_t)(stats->counters[i].hi) << 32 |
			stats->counters[i].lo;
#elif ISC_STATS_HAVEATOMICQ
#if defined(ISC_STATS_HAVESTDATOMICQ)
		stats->copiedcounters[i] =
			atomic_load_explicit(&stats->counters[i],
					     memory_order_relaxed);
#else
		/* use xaddq(..., 0) as an atomic load */
		stats->copiedcounters[i] =
			(isc_uint64_t)isc_atomic_xaddq((isc_int64_t *)&stats->counters[i], 0);
#endif
#else
		stats->copiedcounters[i] = stats->counters[i];
#endif
	}

#if ISC_STATS_LOCKCOUNTERS
	isc_rwlock_unlock(&stats->counterlock, isc_rwlocktype_write);
#endif
}

isc_result_t
isc_stats_create(isc_mem_t *mctx, isc_stats_t **statsp, int ncounters) {
	REQUIRE(statsp != NULL && *statsp == NULL);

	return (create_stats(mctx, ncounters, statsp));
}

void
isc_stats_increment(isc_stats_t *stats, isc_statscounter_t counter) {
	REQUIRE(ISC_STATS_VALID(stats));
	REQUIRE(counter < stats->ncounters);

	incrementcounter(stats, (int)counter);
}

void
isc_stats_decrement(isc_stats_t *stats, isc_statscounter_t counter) {
	REQUIRE(ISC_STATS_VALID(stats));
	REQUIRE(counter < stats->ncounters);

	decrementcounter(stats, (int)counter);
}

void
isc_stats_dump(isc_stats_t *stats, isc_stats_dumper_t dump_fn,
	       void *arg, unsigned int options)
{
	int i;

	REQUIRE(ISC_STATS_VALID(stats));

	copy_counters(stats);

	for (i = 0; i < stats->ncounters; i++) {
		if ((options & ISC_STATSDUMP_VERBOSE) == 0 &&
		    stats->copiedcounters[i] == 0)
				continue;
		dump_fn((isc_statscounter_t)i, stats->copiedcounters[i], arg);
	}
}

void
isc_stats_set(isc_stats_t *stats, isc_uint64_t val,
	      isc_statscounter_t counter)
{
	REQUIRE(ISC_STATS_VALID(stats));
	REQUIRE(counter < stats->ncounters);

#if ISC_STATS_LOCKCOUNTERS
	/*
	 * We use a "write" lock before "reading" the statistics counters as
	 * an exclusive lock.
	 */
	isc_rwlock_lock(&stats->counterlock, isc_rwlocktype_write);
#endif

#if ISC_STATS_USEMULTIFIELDS
	stats->counters[counter].hi = (isc_uint32_t)((val >> 32) & 0xffffffff);
	stats->counters[counter].lo = (isc_uint32_t)(val & 0xffffffff);
#elif ISC_STATS_HAVEATOMICQ
#if defined(ISC_STATS_HAVESTDATOMICQ)
	atomic_store_explicit(&stats->counters[counter], val,
			      memory_order_relaxed);
#else
	isc_atomic_storeq((isc_int64_t *)&stats->counters[counter], val);
#endif
#else
	stats->counters[counter] = val;
#endif

#if ISC_STATS_LOCKCOUNTERS
	isc_rwlock_unlock(&stats->counterlock, isc_rwlocktype_write);
#endif
}
