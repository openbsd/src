/*	$OpenBSD: bitmap.c,v 1.4 2026/05/11 12:14:38 claudio Exp $	*/
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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

#include "bgpd.h"

#define BITMAP_BITS		64
#define BITMAP_ALLOCSIZE	4
#define BITMAP_ROUNDUP(x, y)	((((x) + (y))/(y)) * (y))
#define BITMAP_SETPTR(x)	((uint64_t)(uintptr_t)(x) | 0x1)
#define BITMAP_GETPTR(x)	(uint64_t *)((uintptr_t)(x) & ~0x1)

size_t bitmap_size;
uint64_t bitmap_cnt;

static inline void
bitmap_getset(struct bitmap *map, uint64_t **ptr, uint32_t *max)
{
	if ((map->data[0] & 0x1) == 0) {
		*ptr = map->data;
		*max = nitems(map->data);
	} else {
		*ptr = BITMAP_GETPTR(map->data[0]);
		*max = map->data[1];
	}
}

static void
bitmap_free(struct bitmap *map)
{
	uint64_t *ptr;
	uint32_t  max;

	if (map->data[0] & 0x1) {
		bitmap_getset(map, &ptr, &max);
		bitmap_size -= max * sizeof(uint64_t);
		bitmap_cnt--;
		free(ptr);
	}
}

static int
bitmap_resize(struct bitmap *map, uint32_t elm)
{
	uint64_t *ptr, *new;
	uint32_t oldmax, newmax;

	bitmap_getset(map, &ptr, &oldmax);

	/* get new map */
	newmax = BITMAP_ROUNDUP(elm, BITMAP_ALLOCSIZE);
	if (newmax < oldmax) {
		errno = ERANGE;
		return -1;
	}
	if ((new = reallocarray(NULL, newmax, sizeof(*new))) == NULL)
		return -1;

	/* copy data over */
	for (elm = 0; elm < oldmax; elm++)
		new[elm] = ptr[elm];
	for ( ; elm < newmax; elm++)
		new[elm] = 0;

	/* free old data */
	bitmap_free(map);

	/* set new data */
	map->data[0] = BITMAP_SETPTR(new);
	map->data[1] = newmax;
	bitmap_size += newmax * sizeof(*new);
	bitmap_cnt++;

	return 0;
}

/*
 * Set bit at position bid in map. Extending map if needed.
 * Returns 0 on success and -1 on failure.
 */
int
bitmap_set(struct bitmap *map, uint32_t bid)
{
	uint64_t *ptr;
	uint32_t  max, elm;

	bitmap_getset(map, &ptr, &max);
	elm = bid / BITMAP_BITS;

	if (bid == 0) {
		errno = EINVAL;
		return -1;
	}

	if (elm >= max) {
		if (bitmap_resize(map, elm) == -1)
			return -1;
		bitmap_getset(map, &ptr, &max);
	}

	bid %= BITMAP_BITS;
	ptr[elm] |= (1ULL << bid);
	return 0;
}

/*
 * Test if bit at position bid is set in map.
 */
int
bitmap_test(struct bitmap *map, uint32_t bid)
{
	uint64_t *ptr;
	uint32_t  max, elm;

	bitmap_getset(map, &ptr, &max);
	elm = bid / BITMAP_BITS;

	if (elm >= max || bid == 0)
		return 0;

	bid %= BITMAP_BITS;
	return (ptr[elm] & (1ULL << bid)) != 0;
}

/*
 * Clear bit at position bid in map.
 */
void
bitmap_clear(struct bitmap *map, uint32_t bid)
{
	uint64_t *ptr;
	uint32_t  max, elm;

	bitmap_getset(map, &ptr, &max);
	elm = bid / BITMAP_BITS;

	if (elm >= max || bid == 0)
		return;

	bid %= BITMAP_BITS;
	ptr[elm] &= ~(1ULL << bid);
}

/*
 * Check if bitmap has nothing set (all zero).
 * Returns 1 if empty else 0.
 */
int
bitmap_empty(struct bitmap *map)
{
	uint64_t *ptr, m;
	uint32_t elm, max;

	bitmap_getset(map, &ptr, &max);

	for (elm = 0; elm < max; elm++) {
		m = ptr[elm];
		if (elm == 0)
			m &= ~0x1;	/* skip inline marker */
		if (m != 0)
			return 0;
	}
	return 1;
}

/*
 * Allocate the lowest free id in a map.
 */
int
bitmap_id_get(struct bitmap *map, uint32_t *bid)
{
	uint64_t *ptr, m;
	uint32_t elm, max;

	bitmap_getset(map, &ptr, &max);

	for (elm = 0; elm < max; elm++) {
		m = ~ptr[elm];
		if (elm == 0)
			m &= ~0x1;	/* skip inline marker */
		if (m == 0)
			continue;

		*bid = elm * BITMAP_BITS + ffsll(m) - 1;
		if (bitmap_set(map, *bid) == -1)
			return -1;
		return 0;
	}

	*bid = max * BITMAP_BITS;
	if (bitmap_set(map, *bid) == -1)
		return -1;

	return 0;
}

/*
 * Release an id, so it can be reallocated.
 */
void
bitmap_id_put(struct bitmap *map, uint32_t bid)
{
	bitmap_clear(map, bid);
}

/*
 * Init a map by setting both data values to 0.
 */
void
bitmap_init(struct bitmap *map)
{
	map->data[0] = 0;
	map->data[1] = 0;
}

/*
 * Reset a map back to initial state, freeing any memory that was allocated.
 */
void
bitmap_reset(struct bitmap *map)
{
	bitmap_free(map);
	bitmap_init(map);
}

void
bitmap_get_stats(long long *cnt, long long *size)
{
	*cnt = bitmap_cnt;
	*size = bitmap_size;
}
