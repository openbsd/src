/*	$OpenBSD: chash_sub_test.c,v 1.1 2025/10/29 13:15:22 claudio Exp $ */

/*
 * Copyright (c) 2025 Claudio Jeker <claudio@openbsd.org>
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
#include "chash.c"

#include <stdio.h>

int fail;

void
ch_sub_flag_test(void)
{
	const uint8_t hash[7] = { 0x80, 0xff, 0x80, 0x82, 0x80, 0x83, 0x42 };
	struct ch_group test;
	uint64_t mask;
	int i, rv, t = 1;

	test.cg_meta = 0;
	for (i = 0; i < 7; i++) {
		test.cg_data[i] = (void *)(0xc0ffee00L + i);
		cg_meta_set_hash(&test, i, hash[i]);
	}

	memset(&mask, 0x80, sizeof(mask));

	/* test that CH_EVER_FULL is properly set */
	printf("%s: test%d\n", __func__, t);
	rv = ch_meta_locate(&test, mask);
	if (rv != 0) {
		printf("%s: test%d: rv = %d\n", __func__, t, rv);
		fail = 1;
	}

	t++;
	/* test return value of cg_meta_set_flags() */
	printf("%s: test%d\n", __func__, t);
	if (cg_meta_set_flags(&test, CH_EVER_FULL) != 0 ||
	    cg_meta_set_flags(&test, CH_EVER_FULL) == 0) {
		printf("%s: test%d: cg_meta_set_flags return error\n",
		    __func__, t);
		fail = 1;
	}
	rv = ch_meta_locate(&test, mask);
	if (rv != CH_EVER_FULL) {
		printf("%s: test%d: rv = %d\n", __func__, t, rv);
		fail = 1;
	}

	t++;
	/* test that ch_meta_locate works */
	printf("%s: test%d\n", __func__, t);
	cg_meta_clear_flags(&test, 0xff);
	cg_meta_set_flags(&test, 0x7f);
	rv = ch_meta_locate(&test, mask);
	if (rv != 0x15) {
		printf("%s: test%d: rv = %d\n", __func__, t, rv);
		fail = 1;
	}

	t++;
	/* test that ch_meta_locate handles CH_EVER_FULL */
	printf("%s: test%d\n", __func__, t);
	cg_meta_clear_flags(&test, 0xff);
	cg_meta_set_flags(&test, 0x73 | CH_EVER_FULL);
	rv = ch_meta_locate(&test, mask);
	if (rv != (0x11 | CH_EVER_FULL)) {
		printf("%s: test%d: rv = %d\n", __func__, t, rv);
		fail = 1;
	}
}

static inline int
test_cmp(const void *lptr, const void *rptr)
{
	uintptr_t l = (uintptr_t)lptr;
	uintptr_t r = (uintptr_t)rptr;

	if (l == r)
		return 1;
	return 0;
}

static inline uint64_t
test_hash(const void *ptr)
{
	uintptr_t h = (uintptr_t)ptr;
	return h;
}

struct ch_type testtype = { test_cmp, test_hash };

void
ch_sub_table_test(void)
{
	struct ch_group table[256] = { 0 };
	struct ch_meta meta = { 0 };
	struct ch_iter iter;
	uintptr_t h;
	void *v;
	int t = 1, i;

	/* insert all in group 0 */
	printf("%s: test%d\n", __func__, t);
	for (i = 0; i < 32; i++) {
		h = i;
		if ((v = ch_sub_insert(&testtype, table, &meta, h,
		    (void *)h)) != NULL) {
			printf("%s: test%d: insert h = %ld rv %p\n",
			    __func__, t, h, v);
			fail = 1;
		}
	}
	/* check group 0 */
	t++;
	printf("%s: test%d\n", __func__, t);
	for (i = 0; i < 32; i++) {
		h = i;
		if ((v = ch_sub_find(&testtype, table, h,
		    (void *)h)) != (void *)h) {
			printf("%s: test%d: find h = %ld != %p\n",
			    __func__, t, h, v);
			fail = 1;
		}
	}
	/* try re-insert in group 0 */
	t++;
	printf("%s: test%d\n", __func__, t);
	for (i = 0; i < 32; i++) {
		h = i;
		if ((v = ch_sub_insert(&testtype, table, &meta, h,
		    (void *)h)) != (void *)h) {
			printf("%s: test%d: reinsert h = %ld != %p\n",
			    __func__, t, h, v);
			fail = 1;
		}
	}

	/* test the iterator */
	t++;
	printf("%s: test%d\n", __func__, t);
	for (h = 0, v = ch_sub_first(&testtype, table, &iter);
	     v != NULL;
	     h++, v = ch_sub_next(&testtype, table, &iter)) {
		if (v != (void *)h) {
			printf("%s: test%d: iter h = %ld != %p\n",
			    __func__, t, h, v);
			fail = 1;
		}
	}

	/* remove some elements */
	t++;
	printf("%s: test%d\n", __func__, t);
	for (i = 6; i < 12; i++) {
		h = i;
		if ((v = ch_sub_remove(&testtype, table, &meta, h,
		    (void *)h)) != (void *)h) {
			printf("%s: test%d: remove h = %ld != %p\n",
			    __func__, t, h, v);
			fail = 1;
		}
	}

	/* lookup again */
	t++;
	printf("%s: test%d\n", __func__, t);
	for (i = 0; i < 32; i++) {
		void *exp;
		h = i;
		if (i < 6 || i >= 12)
			exp = (void *)h;
		else
			exp = NULL;
		if ((v = ch_sub_find(&testtype, table, h,
		    (void *)h)) != exp) {
			printf("%s: test%d: find h = %ld %p != %p\n",
			    __func__, t, h, v, exp);
			fail = 1;
		}
	}

	/* insert dup after hole, insert new data 2 times, recheck dup */
	t++;
	printf("%s: test%d\n", __func__, t);
	h = 16;
	if ((v = ch_sub_insert(&testtype, table, &meta, h,
	    (void *)h)) != (void *)h) {
		printf("%s: test%d: reinsert h = %ld != %p\n",
		    __func__, t, h, v);
		fail = 1;
	}
	h = 42;
	if ((v = ch_sub_insert(&testtype, table, &meta, h,
	    (void *)h)) != NULL) {
		printf("%s: test%d: insert h = %ld != %p\n",
		    __func__, t, h, v);
		fail = 1;
	}
	h = 43;
	if ((v = ch_sub_insert(&testtype, table, &meta, h,
	    (void *)h)) != NULL) {
		printf("%s: test%d: insert h = %ld != %p\n",
		    __func__, t, h, v);
		fail = 1;
	}
	h = 16;
	if ((v = ch_sub_insert(&testtype, table, &meta, h,
	    (void *)h)) != (void *)h) {
		printf("%s: test%d: reinsert h = %ld != %p\n",
		    __func__, t, h, v);
		fail = 1;
	}

	/* test the iterator */
	t++;
	printf("%s: test%d\n", __func__, t);
	for (i = 0, v = ch_sub_first(&testtype, table, &iter);
	     v != NULL;
	     i++, v = ch_sub_next(&testtype, table, &iter)) {
		if (i < 6)
			h = i;
		else if (i < 8)
			h = 42 - 6 + i;
		else
			h = i;
		if (i >= 8 && i < 12)
			h = 0;

		if (v != (void *)h) {
			printf("%s: test%d: iter h = %ld != %p\n",
			    __func__, t, h, v);
			fail = 1;
		}
	}

}

int
main(int argc, char **argv)
{
	ch_sub_flag_test();
	ch_sub_table_test();

	return fail;
}
