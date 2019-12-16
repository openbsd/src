/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: pool_test.c,v 1.1 2019/12/16 16:31:36 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/mem.h>
#include <isc/pool.h>

#include "isctest.h"

static isc_result_t
poolinit(void **target, void *arg) {
	isc_result_t result;

	isc_taskmgr_t *mgr = (isc_taskmgr_t *) arg;
	isc_task_t *task = NULL;
	result = isc_task_create(mgr, 0, &task);
	if (result != ISC_R_SUCCESS)
		return (result);

	*target = (void *) task;
	return (ISC_R_SUCCESS);
}

static void
poolfree(void **target) {
	isc_task_t *task = *(isc_task_t **) target;
	isc_task_destroy(&task);
	*target = NULL;
}

/*
 * Individual unit tests
 */

/* Create a pool */
ATF_TC(create_pool);
ATF_TC_HEAD(create_pool, tc) {
	atf_tc_set_md_var(tc, "descr", "create a pool");
}
ATF_TC_BODY(create_pool, tc) {
	isc_result_t result;
	isc_pool_t *pool = NULL;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_pool_create(mctx, 8, poolfree, poolinit, taskmgr, &pool);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(isc_pool_count(pool), 8);

	isc_pool_destroy(&pool);
	ATF_REQUIRE_EQ(pool, NULL);

	isc_test_end();
}

/* Resize a pool */
ATF_TC(expand_pool);
ATF_TC_HEAD(expand_pool, tc) {
	atf_tc_set_md_var(tc, "descr", "expand a pool");
}
ATF_TC_BODY(expand_pool, tc) {
	isc_result_t result;
	isc_pool_t *pool1 = NULL, *pool2 = NULL, *hold = NULL;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_pool_create(mctx, 10, poolfree, poolinit, taskmgr, &pool1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(isc_pool_count(pool1), 10);

	/* resizing to a smaller size should have no effect */
	hold = pool1;
	result = isc_pool_expand(&pool1, 5, &pool2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(isc_pool_count(pool2), 10);
	ATF_REQUIRE_EQ(pool2, hold);
	ATF_REQUIRE_EQ(pool1, NULL);
	pool1 = pool2;
	pool2 = NULL;

	/* resizing to the same size should have no effect */
	hold = pool1;
	result = isc_pool_expand(&pool1, 10, &pool2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(isc_pool_count(pool2), 10);
	ATF_REQUIRE_EQ(pool2, hold);
	ATF_REQUIRE_EQ(pool1, NULL);
	pool1 = pool2;
	pool2 = NULL;

	/* resizing to larger size should make a new pool */
	hold = pool1;
	result = isc_pool_expand(&pool1, 20, &pool2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(isc_pool_count(pool2), 20);
	ATF_REQUIRE(pool2 != hold);
	ATF_REQUIRE_EQ(pool1, NULL);

	isc_pool_destroy(&pool2);
	ATF_REQUIRE_EQ(pool2, NULL);

	isc_test_end();
}

/* Get objects */
ATF_TC(get_objects);
ATF_TC_HEAD(get_objects, tc) {
	atf_tc_set_md_var(tc, "descr", "get objects");
}
ATF_TC_BODY(get_objects, tc) {
	isc_result_t result;
	isc_pool_t *pool = NULL;
	void *item;
	isc_task_t *task1 = NULL, *task2 = NULL, *task3 = NULL;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_pool_create(mctx, 2, poolfree, poolinit, taskmgr, &pool);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(isc_pool_count(pool), 2);

	item = isc_pool_get(pool);
	ATF_REQUIRE(item != NULL);
	isc_task_attach((isc_task_t *) item, &task1);

	item = isc_pool_get(pool);
	ATF_REQUIRE(item != NULL);
	isc_task_attach((isc_task_t *) item, &task2);

	item = isc_pool_get(pool);
	ATF_REQUIRE(item != NULL);
	isc_task_attach((isc_task_t *) item, &task3);

	isc_task_detach(&task1);
	isc_task_detach(&task2);
	isc_task_detach(&task3);

	isc_pool_destroy(&pool);
	ATF_REQUIRE_EQ(pool, NULL);

	isc_test_end();
}


/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, create_pool);
	ATF_TP_ADD_TC(tp, expand_pool);
	ATF_TP_ADD_TC(tp, get_objects);

	return (atf_no_error());
}

