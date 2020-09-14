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

/* $Id: heap.c,v 1.7 2020/09/14 08:40:44 florian Exp $ */

/*! \file
 * Heap implementation of priority queues adapted from the following:
 *
 *	\li "Introduction to Algorithms," Cormen, Leiserson, and Rivest,
 *	MIT Press / McGraw Hill, 1990, ISBN 0-262-03141-8, chapter 7.
 *
 *	\li "Algorithms," Second Edition, Sedgewick, Addison-Wesley, 1988,
 *	ISBN 0-201-06673-4, chapter 11.
 */

#include <stdlib.h>
#include <isc/heap.h>
#include <string.h>
#include <isc/util.h>

/*@{*/
/*%
 * Note: to make heap_parent and heap_left easy to compute, the first
 * element of the heap array is not used; i.e. heap subscripts are 1-based,
 * not 0-based.  The parent is index/2, and the left-child is index*2.
 * The right child is index*2+1.
 */
#define heap_parent(i)			((i) >> 1)
#define heap_left(i)			((i) << 1)
/*@}*/

#define SIZE_INCREMENT			1024

/*%
 * When the heap is in a consistent state, the following invariant
 * holds true: for every element i > 1, heap_parent(i) has a priority
 * higher than or equal to that of i.
 */
#define HEAPCONDITION(i) ((i) == 1 || \
			  ! heap->compare(heap->array[(i)], \
					  heap->array[heap_parent(i)]))

/*% ISC heap structure. */
struct isc_heap {
	unsigned int			size;
	unsigned int			size_increment;
	unsigned int			last;
	void				**array;
	isc_heapcompare_t		compare;
	isc_heapindex_t			index;
};

isc_result_t
isc_heap_create(isc_heapcompare_t compare,
		isc_heapindex_t idx, unsigned int size_increment,
		isc_heap_t **heapp)
{
	isc_heap_t *heap;

	REQUIRE(heapp != NULL && *heapp == NULL);
	REQUIRE(compare != NULL);

	heap = malloc(sizeof(*heap));
	if (heap == NULL)
		return (ISC_R_NOMEMORY);
	heap->size = 0;
	if (size_increment == 0)
		heap->size_increment = SIZE_INCREMENT;
	else
		heap->size_increment = size_increment;
	heap->last = 0;
	heap->array = NULL;
	heap->compare = compare;
	heap->index = idx;

	*heapp = heap;

	return (ISC_R_SUCCESS);
}

void
isc_heap_destroy(isc_heap_t **heapp) {
	isc_heap_t *heap;

	REQUIRE(heapp != NULL);
	heap = *heapp;

	free(heap->array);
	free(heap);

	*heapp = NULL;
}

static int
resize(isc_heap_t *heap) {
	void **new_array;
	unsigned int new_size;

	new_size = heap->size + heap->size_increment;
	new_array = reallocarray(NULL, new_size, sizeof(void *));
	if (new_array == NULL)
		return (0);
	if (heap->array != NULL) {
		memmove(new_array, heap->array, heap->size * sizeof(void *));
		free(heap->array);
	}
	heap->size = new_size;
	heap->array = new_array;

	return (1);
}

static void
float_up(isc_heap_t *heap, unsigned int i, void *elt) {
	unsigned int p;

	for (p = heap_parent(i) ;
	     i > 1 && heap->compare(elt, heap->array[p]) ;
	     i = p, p = heap_parent(i)) {
		heap->array[i] = heap->array[p];
		if (heap->index != NULL)
			(heap->index)(heap->array[i], i);
	}
	heap->array[i] = elt;
	if (heap->index != NULL)
		(heap->index)(heap->array[i], i);

	INSIST(HEAPCONDITION(i));
}

static void
sink_down(isc_heap_t *heap, unsigned int i, void *elt) {
	unsigned int j, size, half_size;
	size = heap->last;
	half_size = size / 2;
	while (i <= half_size) {
		/* Find the smallest of the (at most) two children. */
		j = heap_left(i);
		if (j < size && heap->compare(heap->array[j+1],
					      heap->array[j]))
			j++;
		if (heap->compare(elt, heap->array[j]))
			break;
		heap->array[i] = heap->array[j];
		if (heap->index != NULL)
			(heap->index)(heap->array[i], i);
		i = j;
	}
	heap->array[i] = elt;
	if (heap->index != NULL)
		(heap->index)(heap->array[i], i);

	INSIST(HEAPCONDITION(i));
}

isc_result_t
isc_heap_insert(isc_heap_t *heap, void *elt) {
	unsigned int new_last;

	new_last = heap->last + 1;
	RUNTIME_CHECK(new_last > 0); /* overflow check */
	if (new_last >= heap->size && !resize(heap))
		return (ISC_R_NOMEMORY);
	heap->last = new_last;

	float_up(heap, new_last, elt);

	return (ISC_R_SUCCESS);
}

void
isc_heap_delete(isc_heap_t *heap, unsigned int idx) {
	void *elt;
	int less;

	REQUIRE(idx >= 1 && idx <= heap->last);

	if (heap->index != NULL)
		(heap->index)(heap->array[idx], 0);
	if (idx == heap->last) {
		heap->array[heap->last] = NULL;
		heap->last--;
	} else {
		elt = heap->array[heap->last];
		heap->array[heap->last] = NULL;
		heap->last--;

		less = heap->compare(elt, heap->array[idx]);
		heap->array[idx] = elt;
		if (less)
			float_up(heap, idx, heap->array[idx]);
		else
			sink_down(heap, idx, heap->array[idx]);
	}
}

void
isc_heap_increased(isc_heap_t *heap, unsigned int idx) {
	REQUIRE(idx >= 1 && idx <= heap->last);

	float_up(heap, idx, heap->array[idx]);
}

void
isc_heap_decreased(isc_heap_t *heap, unsigned int idx) {
	REQUIRE(idx >= 1 && idx <= heap->last);

	sink_down(heap, idx, heap->array[idx]);
}

void *
isc_heap_element(isc_heap_t *heap, unsigned int idx) {
	REQUIRE(idx >= 1);

	if (idx <= heap->last)
		return (heap->array[idx]);
	return (NULL);
}
