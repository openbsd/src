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

/* $ISC: t_mem.c,v 1.9 2001/01/09 21:42:00 bwelling Exp $ */

#include <config.h>

#include <isc/mem.h>

#include <tests/t_api.h>

/*
 * Adapted from the original mempool_test.c program.
 */
isc_mem_t *mctx;

#define	MP1_FREEMAX	10
#define	MP1_FILLCNT	10
#define	MP1_MAXALLOC	30

#define	MP2_FREEMAX	25
#define	MP2_FILLCNT	25

static int
memtest(void) {
	int		nfails;
	void		*items1[50];
	void		*items2[50];
	void		*tmp;
	isc_mempool_t	*mp1, *mp2;
	isc_result_t	isc_result;
	unsigned int	i, j;
	int		rval;


	nfails = 0;
	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		++nfails;
		return(nfails);
	}

	mp1 = NULL;
	isc_result = isc_mempool_create(mctx, 24, &mp1);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mempool_create failed %s\n",
		       isc_result_totext(isc_result));
		++nfails;
		return(nfails);
	}

	mp2 = NULL;
	isc_result = isc_mempool_create(mctx, 31, &mp2);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mempool_create failed %s\n",
		       isc_result_totext(isc_result));
		++nfails;
		return(nfails);
	}

	if (T_debug)
		isc_mem_stats(mctx, stderr);

	t_info("setting freemax to %d\n", MP1_FREEMAX);
	isc_mempool_setfreemax(mp1, MP1_FREEMAX);
	t_info("setting fillcount to %d\n", MP1_FILLCNT);
	isc_mempool_setfillcount(mp1, MP1_FILLCNT);
	t_info("setting maxalloc to %d\n", MP1_MAXALLOC);
	isc_mempool_setmaxalloc(mp1, MP1_MAXALLOC);

	/*
	 * Allocate MP1_MAXALLOC items from the pool.  This is our max.
	 */
	for (i = 0 ; i < MP1_MAXALLOC ; i++) {
		items1[i] = isc_mempool_get(mp1);
		if (items1[i] == NULL) {
			t_info("isc_mempool_get unexpectedly failed\n");
			++nfails;
		}
	}

	/*
	 * Try to allocate one more.  This should fail.
	 */
	tmp = isc_mempool_get(mp1);
	if (tmp != NULL) {
		t_info("isc_mempool_get unexpectedly succeeded\n");
		++nfails;
	}

	/*
	 * Free the first 11 items.  Verify that there are 10 free items on
	 * the free list (which is our max).
	 */

	for (i = 0 ; i < 11 ; i++) {
		isc_mempool_put(mp1, items1[i]);
		items1[i] = NULL;
	}

	rval = isc_mempool_getfreecount(mp1);
	if (rval != 10) {
		t_info("isc_mempool_getfreecount returned %d, expected %d\n",
				rval, MP1_FREEMAX);
		++nfails;
	}

	rval = isc_mempool_getallocated(mp1);
	if (rval != 19) {
		t_info("isc_mempool_getallocated returned %d, expected %d\n",
				rval, MP1_MAXALLOC - 11);
		++nfails;
	}

	if (T_debug)
		isc_mem_stats(mctx, stderr);

	/*
	 * Now, beat up on mp2 for a while.  Allocate 50 items, then free
	 * them, then allocate 50 more, etc.
	 */

	t_info("setting freemax to %d\n", MP2_FREEMAX);
	isc_mempool_setfreemax(mp2, 25);
	t_info("setting fillcount to %d\n", MP2_FILLCNT);
	isc_mempool_setfillcount(mp2, 25);

	t_info("exercising the memory pool\n");
	for (j = 0 ; j < 500000 ; j++) {
		for (i = 0 ; i < 50 ; i++) {
			items2[i] = isc_mempool_get(mp2);
			if (items2[i] == NULL) {
				t_info("items2[%d] is unexpectedly null\n", i);
				++nfails;
			}
		}
		for (i = 0 ; i < 50 ; i++) {
			isc_mempool_put(mp2, items2[i]);
			items2[i] = NULL;
		}
		if (j % 50000 == 0)
			t_info("...\n");
	}

	/*
	 * Free all the other items and blow away this pool.
	 */
	for (i = 11 ; i < MP1_MAXALLOC ; i++) {
		isc_mempool_put(mp1, items1[i]);
		items1[i] = NULL;
	}

	isc_mempool_destroy(&mp1);

	if (T_debug)
		isc_mem_stats(mctx, stderr);

	isc_mempool_destroy(&mp2);

	if (T_debug)
		isc_mem_stats(mctx, stderr);

	isc_mem_destroy(&mctx);

	return(0);
}

static const char *a1 =
		"the memory module supports the creation of memory contexts "
		"and the management of memory pools.";
static void
t1(void) {
	int	rval;
	int	result;

	t_assert("mem", 1, T_REQUIRED, a1);

	rval = memtest();

	if (rval == 0)
		result = T_PASS;
	else
		result = T_FAIL;
	t_result(result);
}

testspec_t	T_testlist[] = {
	{	t1,	"basic memory subsystem"	},
	{	NULL,	NULL				}
};

