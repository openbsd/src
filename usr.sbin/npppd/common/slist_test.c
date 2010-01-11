/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*

 cc -g -Wall -o slist_test slist_test.c slist.c
 ./slist_test


 */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "slist.h"

#define	TEST(f)			\
    {				\
	printf("%-10s .. ", #f);	\
	f();			\
	printf("ok\n");		\
    }

#define ASSERT(x)	\
	if (!(x)) { \
	    fprintf(stderr, \
		"\nASSERT(%s) failed on %s() at %s:%d.\n" \
		, #x, __func__, __FILE__, __LINE__); \
	    dump(l);				    \
	    abort(); \
	}

static void
dump(slist *l)
{
	int i;

	fprintf(stderr, 
	    "\tl->itr_curr = %d\n"
	    "\tl->itr_next = %d\n"
	    "\tl->first_idx = %d\n"
	    "\tl->last_idx = %d\n"
	    "\tl->list_size = %d\n"
	    , l->itr_curr, l->itr_next, l->first_idx, l->last_idx
	    , l->list_size);
	for (i = 0; i < slist_length(l); i++) {
		if ((i % 16) == 0)
			fprintf(stderr, "%08x ", i);
		fprintf(stderr, " %3d", (int)slist_get(l, i));
		if ((i % 16) == 7)
			fprintf(stderr, " -");
		if ((i % 16) == 15)
			fprintf(stderr, "\n");
	}
	if ((i % 16) != 0)
		fprintf(stderr, "\n");
}

// まんなかに空きの場合に削除系のテスト
static void
test_01a()
{
	int i, f;
	slist sl;
	slist *l = &sl;

	slist_init(&sl);
	slist_add(&sl, (void *)1);

	ASSERT(sl.list_size == 256);

#define	SETUP()						\
    {							\
	l->last_idx =  64;				\
	l->first_idx = 192;				\
	for (i = 0; i < slist_length(l); i++) {		\
		slist_set(l, i, (void *)i);		\
	}						\
    }

	// 先頭要素削除
	SETUP();
	f = 0;
	while (slist_length(l) > 0) {
		slist_remove(l, 0);
		f++;
		for (i = 0; i < slist_length(l); i++) {
			ASSERT((int)slist_get(l, i) == i + f);
		}
	}

	// 最終要素削除
	SETUP();
	while (slist_length(l) > 0) {
		slist_remove(l, slist_length(l) - 1);
		for (i = 0; i < slist_length(l); i++) {
			ASSERT((int)slist_get(l, i) == i);
		}
	}
	// 最終要素-1削除
	SETUP();
	while (slist_length(l) > 1) {
		slist_remove(l, slist_length(l) - 2);
		for (i = 0; i < slist_length(l) - 1; i++) {
			ASSERT((int)slist_get(l, i) == i);
		}
		if (slist_length(l) > 0) {
			ASSERT((int)slist_get(l, slist_length(l) - 1) == 127);
		}
	}
	slist_remove(l, slist_length(l) - 1);
	ASSERT(slist_length(l) == 0);
}

static void
test_01()
{
	int i;
	slist sl;
	slist *l = &sl;

	slist_init(&sl);


	for (i = 0; i < 255; i++) {
		slist_add(&sl, (void *)i);
	}
	for (i = 0; i < 128; i++) {
		slist_remove_first(&sl);
	}
	for (i = 0; i < 128; i++) {
		slist_add(&sl, (void *)(i + 255));
	}
	ASSERT((int)slist_get(&sl, 127) == 255);
	ASSERT((int)slist_get(&sl, 254) == 129 + 253);
	ASSERT((int)slist_length(&sl) == 255);

	//dump(&sl);
	//printf("==\n");
	slist_add(&sl, (void *)(128 + 255));
	ASSERT((int)slist_get(&sl, 127) == 255);
	//ASSERT((int)slist_get(&sl, 255) == 128 + 255);
	ASSERT((int)slist_length(&sl) == 256);
	//dump(&sl);
}

static void
test_02()
{
	int i;
	slist sl;
	slist *l = &sl;

	slist_init(&sl);


	// 内部配置が、左側に 300 個、右側に 211 個になるように配置
	for (i = 0; i < 511; i++)
		slist_add(&sl, (void *)i);
	for (i = 0; i <= 300; i++)
		slist_remove_first(&sl);
	for (i = 0; i <= 300; i++)
		slist_add(&sl, (void *)i);


	// index 番号になるように再度割り当て
	for (i = 0; i < slist_length(&sl); i++)
		slist_set(&sl, i, (void *)(i + 1));

	ASSERT(slist_length(&sl) == 511);	//論理サイズは511
	ASSERT((int)sl.list[511] == 211);	//右端が 211番目
	ASSERT((int)sl.list[0] == 212);		//左端が 212番目
	ASSERT(sl.list_size == 512);		//物理サイズは 512

	slist_add(&sl, (void *)512);		// 512番めを追加

	ASSERT(sl.list_size == 768);		//物理サイズが拡大
	ASSERT(slist_length(&sl) == 512);	//論理サイズは512
	ASSERT((int)sl.list[511] == 211);	//繋め
	ASSERT((int)sl.list[512] == 212);	//繋め
	ASSERT((int)sl.list[767] == 467);	//右端が 467番目
	ASSERT((int)sl.list[0] == 468);		//左端が 468番目

	//全部チェック
	for (i = 0; i < slist_length(&sl); i++)
		ASSERT((int)slist_get(&sl, i) == i + 1);	// チェック
}

static void
test_03()
{
	int i;
	slist sl;
	slist *l = &sl;

	slist_init(&sl);
	slist_add(&sl, (void *)1);

	for (i = 0; i < 255; i++) {
		slist_add(&sl, (void *)1);
		ASSERT(sl.last_idx >= 0 && sl.last_idx < sl.list_size);
		slist_remove_first(&sl);
		ASSERT(sl.last_idx >= 0 && sl.last_idx < sl.list_size);
	}
	slist_remove(&sl, 0);
	ASSERT(slist_length(&sl) == 0);
	//dump(&sl);
	//TEST(test_02);
}

static void
test_itr_subr_01(slist *l)
{
	int i;

	for (i = 0; i < slist_length(l); i++)
		slist_set(l, i, (void *)(i + 1));

	slist_itr_first(l);
	ASSERT((int)slist_itr_next(l) == 1);	// 普通にイテレート
	ASSERT((int)slist_itr_next(l) == 2);	// 普通にイテレート
	slist_remove(l, 2);			// next を削除
						// "3" が削除
	ASSERT((int)slist_itr_next(l) == 4);	// 削除したものはスキップ
	slist_remove(l, 1);			// 通りすぎたところを削除
						// "2" を削除
	ASSERT((int)slist_itr_next(l) == 5);	// 影響なし
	ASSERT((int)slist_get(l, 0) == 1);	// 削除確認
	ASSERT((int)slist_get(l, 1) == 4);	// 削除確認
	ASSERT((int)slist_get(l, 2) == 5);	// 削除確認

	// 255 アイテム中 2 個削除し、4回イテレートし、1回の削除は通りすぎ
	// たあとなので、残り 250回


	for (i = 0; i < 249; i++)
		ASSERT(slist_itr_next(l) != NULL);
	ASSERT(slist_itr_next(l) != NULL);
	ASSERT(slist_itr_next(l) == NULL);

	// 上記と同じだが、最後を取り出す前に削除

	// リセット (253アイテム)
	for (i = 0; i < slist_length(l); i++)
		slist_set(l, i, (void *)(i + 1));
	slist_itr_first(l);

	ASSERT(slist_length(l) == 253);

	for (i = 0; i < 252; i++)
		ASSERT(slist_itr_next(l) != NULL);

	slist_remove(l, 252);
	ASSERT(slist_itr_next(l) == NULL);	// 最後を指してたけど、NULL

	slist_itr_first(l);
	while (slist_length(l) > 0)
		slist_remove_first(l);
	ASSERT(slist_length(l) == 0);
	ASSERT(slist_itr_next(l) == NULL);
}

static void
test_04()
{
	int i;
	slist sl;
	slist *l = &sl;

	slist_init(&sl);
	for (i = 0; i < 255; i++)
		slist_add(&sl, (void *)(i + 1));

	test_itr_subr_01(&sl);

	for (i = 0; i < 256; i++) {
		// ローテーションして、どんな物理配置でも成功すること確認
		sl.first_idx = i;
		sl.last_idx = sl.first_idx + 255;
		sl.last_idx %= sl.list_size;
		ASSERT(slist_length(&sl) == 255);
		test_itr_subr_01(&sl);
	}
}

// 物理配置の一番最後の要素を削除しても、大丈夫か。
static void
test_05()
{
	int i;
	slist sl;
	slist *l = &sl;

	slist_init(&sl);
	// ぎりぎりまで追加
	for (i = 0; i < 255; i++) {
		slist_add(&sl, (void *)i);
	}
	// 254 個削除
	for (i = 0; i < 254; i++) {
		slist_remove_first(&sl);
	}
	slist_set(l, 0, (void *)0);
	// 7個追加
	for (i = 0; i < 8; i++) {
		slist_add(&sl, (void *)i + 1);
	}
	ASSERT(sl.first_idx == 254);
	ASSERT(sl.last_idx == 7);

	slist_remove(l, 0);
	ASSERT((int)slist_get(l, 0) == 1);
	ASSERT((int)slist_get(l, 1) == 2);
	ASSERT((int)slist_get(l, 2) == 3);
	ASSERT((int)slist_get(l, 3) == 4);
	ASSERT((int)slist_get(l, 4) == 5);
	ASSERT((int)slist_get(l, 5) == 6);
	ASSERT((int)slist_get(l, 6) == 7);
	ASSERT((int)slist_get(l, 7) == 8);
	ASSERT(l->first_idx == 255);

	slist_remove(l, 0);
	ASSERT((int)slist_get(l, 0) == 2);
	ASSERT((int)slist_get(l, 1) == 3);
	ASSERT((int)slist_get(l, 2) == 4);
	ASSERT((int)slist_get(l, 3) == 5);
	ASSERT((int)slist_get(l, 4) == 6);
	ASSERT((int)slist_get(l, 5) == 7);
	ASSERT((int)slist_get(l, 6) == 8);
	ASSERT(l->first_idx == 0);
}

static void
test_06()
{
	int i, j;
	slist sl;
	slist *l = &sl;

	slist_init(l);
	for (i = 0; i < 255; i++)
		slist_add(l, (void *)i);

	i = 255;

	for (slist_itr_first(l); slist_itr_has_next(l); ) {
		ASSERT(slist_length(l) == i);
		slist_itr_next(l);
		ASSERT((int)slist_itr_remove(l) == 255 - i);
		ASSERT(slist_length(l) == i - 1);
		for (j = i; j < slist_length(l); j++)
			ASSERT((int)slist_get(l, j) == i + j);
		i--;
	}
}

static void
test_07()
{
	int i;
	slist sl;
	slist *l = &sl;

	slist_init(l);
	slist_add(l, (void *)1);
	slist_remove_first(l);
	l->first_idx = 120;
	l->last_idx = 120;
	for (i = 0; i < 255; i++)
		slist_add(l, (void *)i);


	for (i = 0, slist_itr_first(l); slist_itr_has_next(l); i++) {
		ASSERT((int)slist_itr_next(l) == i);
		if (i > 200)
		    ASSERT((int)slist_itr_remove(l) == i);
	}
}

static void
test_08()
{
	//int i, x;
	slist sl;
	slist *l = &sl;

	slist_init(l);
	slist_set_size(l, 4);
	slist_add(l, (void *)1);
	slist_add(l, (void *)2);
	slist_add(l, (void *)3);

	/* [1, 2, 3] */

	slist_itr_first(l);
	slist_itr_has_next(l);
	slist_itr_next(l);
	slist_itr_remove(l);
	/* [2, 3] */

	slist_add(l, (void *)4);
	/* [2, 3, 4] */
	ASSERT((int)slist_get(l, 0) == 2);
	ASSERT((int)slist_get(l, 1) == 3);
	ASSERT((int)slist_get(l, 2) == 4);
	slist_add(l, (void *)5);

	/* [2, 3, 4, 5] */
	ASSERT((int)slist_get(l, 0) == 2);
	ASSERT((int)slist_get(l, 1) == 3);
	ASSERT((int)slist_get(l, 2) == 4);
	ASSERT((int)slist_get(l, 3) == 5);

	//dump(l);
}

static void
test_09()
{
	slist sl;
	slist *l = &sl;

	/*
	 * #1
	 */
	slist_init(l);
	slist_set_size(l, 3);
	slist_add(l, (void *)1);
	slist_add(l, (void *)2);
	slist_add(l, (void *)3);

	slist_itr_first(l);
	ASSERT((int)slist_itr_next(l) == 1);		/* 1 */
	ASSERT((int)slist_itr_next(l) == 2);		/* 2 */
	ASSERT((int)slist_itr_next(l) == 3);		/* 3 */
							/* reaches the last */
	slist_add(l, (void *)4);			/* add a new item */
	ASSERT(slist_itr_has_next(l));			/* iterates the new*/
	ASSERT((int)slist_itr_next(l) == 4);		
	slist_fini(l);


	/*
	 * #2
	 */
	slist_init(l);
	slist_set_size(l, 3);
	slist_add(l, (void *)1);
	slist_add(l, (void *)2);
	slist_add(l, (void *)3);

	slist_itr_first(l);
	ASSERT((int)slist_itr_next(l) == 1);		/* 1 */
	ASSERT((int)slist_itr_next(l) == 2);		/* 2 */
	ASSERT((int)slist_itr_next(l) == 3);		/* 3 */
							/* reaches the last */
	//dump(l);
	slist_itr_remove(l);				/* and remove the last*/
	//dump(l);
	slist_add(l, (void *)4);			/* add 4 (new last)*/
	//dump(l);
	ASSERT(slist_itr_has_next(l));			/* */
	ASSERT((int)slist_itr_next(l) == 4);		/* 4 */
	slist_fini(l);

	/*
	 * #3
	 */
	slist_init(l);
	slist_set_size(l, 3);
	slist_add(l, (void *)1);
	slist_add(l, (void *)2);
	slist_add(l, (void *)3);

	slist_itr_first(l);
	ASSERT((int)slist_itr_next(l) == 1);		/* 1 */
	ASSERT((int)slist_itr_next(l) == 2);		/* 2 */
	ASSERT((int)slist_itr_next(l) == 3);		/* 3 */
							/* reaches the last */
	slist_add(l, (void *)4);			/* add a new */
	slist_itr_remove(l);
	ASSERT(slist_itr_has_next(l));
	ASSERT((int)slist_itr_next(l) == 4);		/* 4 */
	slist_fini(l);

	/*
	 * #4 - remove iterator's next and it is the last
	 */
	slist_init(l);
	slist_set_size(l, 3);
	slist_add(l, (void *)1);
	slist_add(l, (void *)2);
	slist_add(l, (void *)3);

	slist_itr_first(l);
	ASSERT((int)slist_itr_next(l) == 1);		/* 1 */
	ASSERT((int)slist_itr_next(l) == 2);		/* 2 */
	slist_remove(l, 2);				/* remove the next */
	slist_add(l, (void *)4);			/* add the new next */
	ASSERT(slist_itr_has_next(l));			/* iterates the new */
	ASSERT((int)slist_itr_next(l) == 4);
	slist_fini(l);
}
static void
test_10()
{
	int i;
	slist sl;
	slist *l = &sl;

	slist_init(l);
	slist_add(l, (void *)1);
	slist_add(l, (void *)2);
	slist_add(l, (void *)3);
	slist_itr_first(l);
	ASSERT((int)slist_itr_next(l) == 1);
	ASSERT((int)slist_itr_next(l) == 2);
	for (i = 4; i < 10000; i++) {
		ASSERT(slist_itr_has_next(l));
		ASSERT((int)slist_itr_next(l) == i - 1);
		if (i % 3 == 1)
			slist_add(l, (void *)i);
		if (i % 3 == 0)
			ASSERT((int)slist_itr_remove(l) == i - 1);
		if (i % 3 != 1)
			slist_add(l, (void *)i);
	}
	slist_itr_first(l);
	while (slist_itr_has_next(l)) {
		slist_itr_next(l);
		slist_itr_remove(l);
	}
	ASSERT((int)slist_length(l) == 0);

	slist_fini(l);
}

static void
test_11()
{
	slist sl;
	slist *l = &sl;

	slist_init(l);
	slist_add(l, (void *)1);
	slist_add(l, (void *)2);
	ASSERT((int)slist_remove_last(l) == 2);
	ASSERT((int)slist_length(l) == 1);
	ASSERT((int)slist_remove_last(l) == 1);
	ASSERT((int)slist_length(l) == 0);
}

int
main(int argc, char *argv[])
{
	TEST(test_01);
	TEST(test_01a);
	TEST(test_02);
	TEST(test_03);
	TEST(test_04);
	TEST(test_05);
	TEST(test_06);
	TEST(test_07);
	TEST(test_08);
	TEST(test_09);
	TEST(test_10);
	TEST(test_11);
	return 0;
}
