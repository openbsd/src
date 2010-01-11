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
/**@file
 * 任意のポインタに関するリスト操作を提供します。
 */
/*
 *	void **list;
 *	list_size;	// list に割り当てたサイズ。
 *	last_idx;	// 最初のインデックス
 *	first_idx;	// 最後のインデックス
 *
 * ・first_idx == last_idx は空を示します。
 * ・fist_idx と last_idx は 0 以上 list_size - 1 以下です。
 * ・使っているサイズは、(last_idx - first_idx) % list_size です。
 *   list_size まで使ってしまうと、空と区別ができず区別しようとすると複雑
 *   になるので、そういう状況を作りません。このため、list に割り当てたサイズ
 *   のうち 1個分は使いません。
 * ・XXX itr_curr が削除されると、
 */
#include <sys/types.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "slist.h"

#define	GROW_SIZE	256
#define	PTR_SIZE	(sizeof(intptr_t))

#ifdef	SLIST_DEBUG
#include <stdio.h>
#define	SLIST_ASSERT(cond)			\
	if (!(cond)) {							\
		fprintf(stderr,						\
		    "\nAssertion failure("#cond") at (%s):%s:%d\n",	\
		    __func__, __FILE__, __LINE__);			\
	}
#else
#define	SLIST_ASSERT(cond)
#endif

/**
 * 内部のインデックスが範囲内ならば、1 を範囲を越えている場合には 0 を
 * を返すマクロです。
 */
#define	VALID_IDX(_list, _idx)					\
	  (((_list)->first_idx <= (_list)->last_idx)			\
	? (((_list)->first_idx <= (_idx) && (_idx) < (_list)->last_idx)? 1 : 0)\
	: (((_list)->first_idx <= (_idx) || (_idx) < (_list)->last_idx)? 1 : 0))

/** インデックスを内部のインデックスに変換します。 */
#define	REAL_IDX(_list, _idx)						\
	(((_list)->first_idx + (_idx)) % (_list)->list_size)

/** 内部のインデックスをインデックスに変換します。 */
#define	VIRT_IDX(_list, _idx)	(((_list)->first_idx <= (_idx))	\
	? (_idx) - (_list)->first_idx				\
	: (_list)->list_size - (_list)->first_idx + (_idx))

/** インデックスを示すメンバー変数を decrement します */
#define	DECR_IDX(_list, _memb)						\
	(_list)->_memb = ((_list)->list_size + --((_list)->_memb))	\
	    % (_list)->list_size
/** インデックスを示すメンバー変数を increment します */
#define	INCR_IDX(_list, _memb)						\
	(_list)->_memb = (++((_list)->_memb)) % (_list)->list_size

static int          slist_grow (slist *);
static int          slist_grow0 (slist *, int);
static __inline void  slist_swap0 (slist *, int, int);

#define	itr_is_valid(list)	((list)->itr_next >= 0)
#define	itr_invalidate(list)	((list)->itr_next = -1)

/** 初期化処理を行います */
void
slist_init(slist *list)
{
	memset(list, 0, sizeof(slist));
	itr_invalidate(list);
}

/**
 * リストのサイズを指定します。1つの要素は内部で利用されるので、必要なサイズ
 * + 1 を指定します。サイズは小さくなりません。
 */
int
slist_set_size(slist *list, int size)
{
	if (size > list->list_size)
		return slist_grow0(list, size - list->list_size);

	return 0;
}

/** 終了化処理を行います */
void
slist_fini(slist *list)
{
	if (list->list != NULL)
		free(list->list);
	slist_init(list);
}

/** このリストの長さ */
int
slist_length(slist *list)
{
	return
	      (list->first_idx <= list->last_idx)
	    ? (list->last_idx - list->first_idx)
	    : (list->list_size - list->first_idx + list->last_idx);
}

/** リストがいっぱいの場合に割り当てサイズを成長させます。 */
static int
slist_grow0(slist *list, int grow_sz)
{
	int size_new;
	void **list_new = NULL;

	/* ひとつ追加できるか。できるならそのまま抜ける。*/
	if (slist_length(list) + 1 < list->list_size)
		/* list_size == slist_length() という状況は作らない */
		return 0;

	size_new = list->list_size + grow_sz;
	if ((list_new = realloc(list->list, PTR_SIZE * size_new))
	    == NULL)
		return -1;

	memset(&list_new[list->list_size], 0, 
	    PTR_SIZE * (size_new - list->list_size));

	list->list = list_new;
	if (list->last_idx < list->first_idx && list->last_idx >= 0) {
		/*
		 * 空きが真ん中にある状況で右側に空きを作ったので、左側
		 * を右にもっていく。
		 */
		if (list->last_idx <= grow_sz) {
			/*
			 * 左側を右側にもっていく場合に十分なスペースがある
			 * のですべて移動
			 */
			memmove(&list->list[list->list_size],
			    &list->list[0], PTR_SIZE * list->last_idx);
			list->last_idx = list->list_size + list->last_idx;
		} else {
			/* 左側をできるかぎり右端に copy */
			memmove(&list->list[list->list_size],
			    &list->list[0], PTR_SIZE * grow_sz);
			/* 左側、copy した分を左にずらす */
			memmove(&list->list[0], &list->list[grow_sz],
			    PTR_SIZE *(list->last_idx - grow_sz));

			list->last_idx -= grow_sz; 
		}
	}
	list->list_size = size_new;

	return 0;
}

static int
slist_grow(slist *list)
{
	return slist_grow0(list, GROW_SIZE);
}

/** リストの末尾に要素を追加します。*/
void *
slist_add(slist *list, void *item)
{
	if (slist_grow(list) != 0)
		return NULL;

	list->list[list->last_idx] = item;

	if (list->itr_next == -2) {
		/* the iterator points the last, update it. */
		list->itr_next = list->last_idx;
	}

	INCR_IDX(list, last_idx);

	return item;
}

#define slist_get0(list_, idx)	((list_)->list[REAL_IDX((list_), (idx))])

/** リストの末尾に指定したリストの要素全てを追加します */
int
slist_add_all(slist *list, slist *add_items)
{
	int i, n;

	n = slist_length(add_items);
	for (i = 0; i < n; i++) {
		if (slist_add(list, slist_get0(add_items, i)) ==  NULL)
			return 1;
	}

	return 0;
}

/** idx番目の要素を返します。*/
void *
slist_get(slist *list, int idx)
{
	SLIST_ASSERT(idx >= 0);
	SLIST_ASSERT(slist_length(list) > idx);

	if (idx < 0 || slist_length(list) <= idx)
		return NULL;

	return slist_get0(list, idx);
}

/** idx番目の要素をセットします。*/
int 
slist_set(slist *list, int idx, void *item)
{
	SLIST_ASSERT(idx >= 0);
	SLIST_ASSERT(slist_length(list) > idx);

	if (idx < 0 || slist_length(list) <= idx)
		return -1;

	list->list[REAL_IDX(list, idx)] = item;

	return 0;
}

/** 1番目の要素を削除して取り出します。*/
void *
slist_remove_first(slist *list)
{
	void *oldVal;

	if (slist_length(list) <= 0)
		return NULL;
	
	oldVal = list->list[list->first_idx];

	if (itr_is_valid(list) && list->itr_next == list->first_idx)
		INCR_IDX(list, itr_next);

	if (!VALID_IDX(list, list->itr_next))
		itr_invalidate(list);

	INCR_IDX(list, first_idx);

	return oldVal;
}




/** 最後の要素を削除して取り出します。*/
void *
slist_remove_last(slist *list)
{
	if (slist_length(list) <= 0)
		return NULL;

	DECR_IDX(list, last_idx);
	if (!VALID_IDX(list, list->itr_next))
		itr_invalidate(list);

	return list->list[list->last_idx];
}

/** 全て要素を削除します */
void
slist_remove_all(slist *list)
{
	void **list0 = list->list;
	
	slist_init(list);

	list->list = list0;
}

/* this doesn't check boudary. */
static __inline void
slist_swap0(slist *list, int m, int n)
{
	void *m0;

	itr_invalidate(list);	/* イテレータ無効 */

	m0 = list->list[REAL_IDX(list, m)];
	list->list[REAL_IDX(list, m)] = list->list[REAL_IDX(list, n)];
	list->list[REAL_IDX(list, n)] = m0;
}

/** リストの m 番目の要素と n 番目の要素を入れ換えます。 */
void
slist_swap(slist *list, int m, int n)
{
	int len;

	len = slist_length(list);
	SLIST_ASSERT(m >= 0);
	SLIST_ASSERT(n >= 0);
	SLIST_ASSERT(len > m);
	SLIST_ASSERT(len > n);

	if (m < 0 || n < 0)
		return;	
	if (m >= len || n >= len)
		return;	

	slist_swap0(list, m, n);
}

/** idx 番目の要素を削除します */
void *
slist_remove(slist *list, int idx)
{
	int first, last, idx0, reset_itr;
	void *oldVal;

	SLIST_ASSERT(idx >= 0);
	SLIST_ASSERT(slist_length(list) > idx);

	if (idx < 0 || slist_length(list) <= idx)
		return NULL;

	idx0 = REAL_IDX(list, idx);
	oldVal = list->list[idx0];
	reset_itr = 0;

	first = -1;
	last = -1;

	if (list->itr_next == idx0) {
		INCR_IDX(list, itr_next);
		if (!VALID_IDX(list, list->itr_next))
			list->itr_next = -2;	/* on the last item */
	}

	/* last 側を縮めるか、first 側を縮めるか。*/
	if (list->first_idx < list->last_idx) {
		/* いちおう短い方を選択 */
		if (idx0 - list->first_idx < list->last_idx - idx0) {
			first = list->first_idx;	
			INCR_IDX(list, first_idx);
		} else {
			last = list->last_idx;
			DECR_IDX(list, last_idx);
		}
	} else {
		/*
		 * 0 < last (未使用) first < idx < size なので first 側を縮める
		 */
		if (list->first_idx <= idx0) {
			first = list->first_idx;	
			INCR_IDX(list, first_idx);
		} else {
			last = list->last_idx;
			DECR_IDX(list, last_idx);
		}
	}

	/* last側 */
	if (last != -1 && last != 0 && last != idx0) {

		/* idx0 〜 last を左にひとつずらす */
		if (itr_is_valid(list) &&
		    idx0 <= list->itr_next && list->itr_next <= last) {
			DECR_IDX(list, itr_next);
			if (!VALID_IDX(list, list->itr_next))
				itr_invalidate(list);
		}

		memmove(&list->list[idx0], &list->list[idx0 + 1], 
		    (PTR_SIZE) * (last - idx0));
	}
	/* first側 */
	if (first != -1 && first != idx0) {

		/* first 〜 idx0 を右にひとつずらす */
		if (itr_is_valid(list) &&
		    first <= list->itr_next && list->itr_next <= idx0) {
			INCR_IDX(list, itr_next);
			if (!VALID_IDX(list, list->itr_next))
				itr_invalidate(list);
		}

		memmove(&list->list[first + 1], &list->list[first], 
		    (PTR_SIZE) * (idx0 - first));
	}
	if (list->first_idx == list->last_idx) {
		list->first_idx = 0;
		list->last_idx = 0;
	}

	return oldVal;
}

/**
 * シャッフルします。
 * <p>
 * <b>slist_shuffle は random(3) を使ってます。使用前に srandom(3) してく
 * ださい。</b></p>
 */
void
slist_shuffle(slist *list)
{
	int i, len;

	len = slist_length(list);
	for (i = len; i > 1; i--)
		slist_swap0(list, i - 1, (int)(random() % i));
}

/**
 * イテレータを初期化します。ひとつの slist インスタンスでひとつしか使えません。
 */
void
slist_itr_first(slist *list)
{
	list->itr_next = list->first_idx;
	if (!VALID_IDX(list, list->itr_next))
		itr_invalidate(list);
}

/**
 * イテレータが次の要素に進めるかどうかを返します。
 * @return イテレータが次の要素を返すことができる場合に 1 を返します。
 *	終端に達したか、イテレータが最後まで達したか、リスト構造の変更があって、
 *	続行不能な場合には 0 が返ります。
 */
int
slist_itr_has_next(slist *list)
{
	if (list->itr_next < 0)
		return 0;
	return VALID_IDX(list, list->itr_next);
}

/** イテレータの次の要素を取り出しつつ、次の要素に進めます。 */
void *
slist_itr_next(slist *list)
{
	void *rval;

	if (!itr_is_valid(list))
		return NULL;
	SLIST_ASSERT(VALID_IDX(list, list->itr_next));

	if (list->list == NULL)
		return NULL;

	rval = list->list[list->itr_next];
	list->itr_curr = list->itr_next;
	INCR_IDX(list, itr_next);

	if (!VALID_IDX(list, list->itr_next))
		list->itr_next = -2;	/* on the last item */

	return rval;
}

/** イテレータの現在の要素を削除します */
void *
slist_itr_remove(slist *list)
{
	SLIST_ASSERT(list != NULL);

	return slist_remove(list, VIRT_IDX(list, list->itr_curr));
}
