/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: mempool_test.c,v 1.12 2001/01/09 21:41:19 bwelling Exp $ */

#include <config.h>

#include <isc/mem.h>
#include <isc/util.h>

isc_mem_t *mctx;

int
main(int argc, char *argv[]) {
	void *items1[50];
	void *items2[50];
	void *tmp;
	isc_mempool_t *mp1, *mp2;
	unsigned int i, j;
	isc_mutex_t lock;

	UNUSED(argc);
	UNUSED(argv);

	isc_mem_debugging = 2;

	RUNTIME_CHECK(isc_mutex_init(&lock) == ISC_R_SUCCESS);

	mctx = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	mp1 = NULL;
	RUNTIME_CHECK(isc_mempool_create(mctx, 24, &mp1) == ISC_R_SUCCESS);

	mp2 = NULL;
	RUNTIME_CHECK(isc_mempool_create(mctx, 31, &mp2) == ISC_R_SUCCESS);

	isc_mempool_associatelock(mp1, &lock);
	isc_mempool_associatelock(mp2, &lock);

	isc_mem_stats(mctx, stderr);

	isc_mempool_setfreemax(mp1, 10);
	isc_mempool_setfillcount(mp1, 10);
	isc_mempool_setmaxalloc(mp1, 30);

	/*
	 * Allocate 30 items from the pool.  This is our max.
	 */
	for (i = 0 ; i < 30 ; i++) {
		items1[i] = isc_mempool_get(mp1);
		RUNTIME_CHECK(items1[i] != NULL);
	}

	/*
	 * Try to allocate one more.  This should fail.
	 */
	tmp = isc_mempool_get(mp1);
	RUNTIME_CHECK(tmp == NULL);

	/*
	 * Free the first 11 items.  Verify that there are 10 free items on
	 * the free list (which is our max).
	 */

	for (i = 0 ; i < 11 ; i++) {
		isc_mempool_put(mp1, items1[i]);
		items1[i] = NULL;
	}

	RUNTIME_CHECK(isc_mempool_getfreecount(mp1) == 10);
	RUNTIME_CHECK(isc_mempool_getallocated(mp1) == 19);

	isc_mem_stats(mctx, stderr);

	/*
	 * Now, beat up on mp2 for a while.  Allocate 50 items, then free
	 * them, then allocate 50 more, etc.
	 */
	isc_mempool_setfreemax(mp2, 25);
	isc_mempool_setfillcount(mp2, 25);
	for (j = 0 ; j < 5000 ; j++) {
		for (i = 0 ; i < 50 ; i++) {
			items2[i] = isc_mempool_get(mp2);
			RUNTIME_CHECK(items2[i] != NULL);
		}
		for (i = 0 ; i < 50 ; i++) {
			isc_mempool_put(mp2, items2[i]);
			items2[i] = NULL;
		}
	}

	/*
	 * Free all the other items and blow away this pool.
	 */
	for (i = 11 ; i < 30 ; i++) {
		isc_mempool_put(mp1, items1[i]);
		items1[i] = NULL;
	}

	isc_mempool_destroy(&mp1);

	isc_mem_stats(mctx, stderr);

	isc_mempool_destroy(&mp2);

	isc_mem_stats(mctx, stderr);

	isc_mem_destroy(&mctx);

	DESTROYLOCK(&lock);

	return (0);
}
