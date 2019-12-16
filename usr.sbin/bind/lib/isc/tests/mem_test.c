/*
 * Copyright (C) 2015  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <atf-c.h>

#include "isctest.h"

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>

static void *
default_memalloc(void *arg, size_t size) {
	UNUSED(arg);
	if (size == 0U)
		size = 1;
	return (malloc(size));
}

static void
default_memfree(void *arg, void *ptr) {
	UNUSED(arg);
	free(ptr);
}

ATF_TC(isc_mem_total);
ATF_TC_HEAD(isc_mem_total, tc) {
	atf_tc_set_md_var(tc, "descr", "test TotalUse calculation");
}

ATF_TC_BODY(isc_mem_total, tc) {
	isc_result_t result;
	isc_mem_t *mctx2 = NULL;
	size_t before, after;
	ssize_t diff;
	int i;

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* Local alloc, free */
	mctx2 = NULL;
	result = isc_mem_createx2(0, 0, default_memalloc, default_memfree,
				  NULL, &mctx2, 0);
	if (result != ISC_R_SUCCESS)
		goto out;

	before = isc_mem_total(mctx2);

	for (i = 0; i < 100000; i++) {
		void *ptr;

		ptr = isc_mem_allocate(mctx2, 2048);
		isc_mem_free(mctx2, ptr);
	}

	after = isc_mem_total(mctx2);
	diff = after - before;

	printf("total_before=%lu, total_after=%lu, total_diff=%lu\n",
	       (unsigned long)before, (unsigned long)after,
	       (unsigned long)diff);
	/* 2048 +8 bytes extra for size_info */
	ATF_CHECK_EQ(diff, (2048 + 8) * 100000);

	/* ISC_MEMFLAG_INTERNAL */

	before = isc_mem_total(mctx);

	for (i = 0; i < 100000; i++) {
		void *ptr;

		ptr = isc_mem_allocate(mctx, 2048);
		isc_mem_free(mctx, ptr);
	}

	after = isc_mem_total(mctx);
	diff = after - before;

	printf("total_before=%lu, total_after=%lu, total_diff=%lu\n",
	       (unsigned long)before, (unsigned long)after,
	       (unsigned long)diff);
	/* 2048 +8 bytes extra for size_info */
	ATF_CHECK_EQ(diff, (2048 + 8) * 100000);

 out:
	if (mctx2 != NULL)
		isc_mem_destroy(&mctx2);

	isc_test_end();
}

ATF_TC(isc_mem_inuse);
ATF_TC_HEAD(isc_mem_inuse, tc) {
	atf_tc_set_md_var(tc, "descr", "test InUse calculation");
}

ATF_TC_BODY(isc_mem_inuse, tc) {
	isc_result_t result;
	isc_mem_t *mctx2 = NULL;
	size_t before, during, after;
	ssize_t diff;
	void *ptr;

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	mctx2 = NULL;
	result = isc_mem_createx2(0, 0, default_memalloc, default_memfree,
				  NULL, &mctx2, 0);
	if (result != ISC_R_SUCCESS)
		goto out;

	before = isc_mem_inuse(mctx2);
	ptr = isc_mem_allocate(mctx2, 1024000);
	during = isc_mem_inuse(mctx2);
	isc_mem_free(mctx2, ptr);
	after = isc_mem_inuse(mctx2);

	diff = after - before;

	printf("inuse_before=%lu, inuse_during=%lu, inuse_after=%lu\n",
	       (unsigned long)before, (unsigned long)during,
	       (unsigned long)after);
	ATF_REQUIRE_EQ(diff, 0);

 out:
	if (mctx2 != NULL)
		isc_mem_destroy(&mctx2);

	isc_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, isc_mem_total);
	ATF_TP_ADD_TC(tp, isc_mem_inuse);

	return (atf_no_error());
}
