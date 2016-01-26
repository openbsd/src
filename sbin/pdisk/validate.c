/*	$OpenBSD: validate.c,v 1.41 2016/01/26 16:39:00 krw Exp $	*/

/*
 * validate.c -
 *
 * Written by Eryk Vershen
 */

/*
 * Copyright 1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>

#include "dpme.h"
#include "partition_map.h"
#include "validate.h"

enum range_state {
	kUnallocated,
	kAllocated,
	kMultiplyAllocated
};

struct range_list {
	struct range_list      *next;
	struct range_list      *prev;
	enum range_state	state;
	int			valid;
	uint32_t		start;
	uint32_t		end;
};

struct range_list *new_range_list_item(enum range_state state, int, uint32_t,
    uint32_t);

void	initialize_list(struct range_list **);
void	add_range(struct range_list **, uint32_t, uint32_t, int);
void	print_range_list(struct range_list *);
void	coalesce_list(struct range_list *);

struct range_list *
new_range_list_item(enum range_state state, int valid, uint32_t low,
    uint32_t high)
{
	struct range_list *item;

	item = malloc(sizeof(struct range_list));
	item->next = 0;
	item->prev = 0;
	item->state = state;
	item->valid = valid;
	item->start = low;
	item->end = high;
	return item;
}


void
initialize_list(struct range_list **list)
{
	struct range_list *item;

	item = new_range_list_item(kUnallocated, 0, 0, 0xFFFFFFFF);
	*list = item;
}


void
add_range(struct range_list **list, uint32_t base, uint32_t len, int allocate)
{
	struct range_list *item, *cur;
	uint32_t low, high;

	/* XXX initialized list will always have one element */
	if (list == NULL || *list == NULL)
		return;
	low = base;
	high = base + len - 1;
	/* XXX wrapped around */
	if (len == 0 || high < len - 1)
		return;
	cur = *list;
	while (low <= high) {
		if (cur == NULL)
			break;
		if (low <= cur->end) {
			if (cur->start < low) {
				item = new_range_list_item(cur->state,
				    cur->valid, cur->start, low - 1);
				/* insert before here */
				if (cur->prev == NULL) {
					item->prev = NULL;
					*list = item;
				} else {
					item->prev = cur->prev;
					item->prev->next = item;
				}
				cur->prev = item;
				item->next = cur;
				cur->start = low;
			}
			if (high < cur->end) {
				item = new_range_list_item(cur->state,
				    cur->valid, high + 1, cur->end);
				/* insert after here */
				if (cur->next == NULL) {
					item->next = NULL;
				} else {
					item->next = cur->next;
					item->next->prev = item;
				}
				cur->next = item;
				item->prev = cur;

				cur->end = high;
			}
			if (allocate) {
				switch (cur->state) {
				case kUnallocated:
					cur->state = kAllocated;
					break;
				case kAllocated:
				case kMultiplyAllocated:
					cur->state = kMultiplyAllocated;
					break;
				}
			} else {
				cur->valid = 1;
			}
			low = cur->end + 1;
		}
		cur = cur->next;
	}
}


void
coalesce_list(struct range_list *list)
{
	struct range_list *cur, *item;

	for (cur = list; cur != NULL;) {
		item = cur->next;
		if (item == NULL)
			break;
		if (cur->valid == item->valid &&
		    cur->state == item->state) {
			cur->end = item->end;
			cur->next = item->next;
			if (item->next != NULL)
				item->next->prev = cur;
			free(item);
		} else {
			cur = cur->next;
		}
	}
}


void
print_range_list(struct range_list *list)
{
	struct range_list *cur;
	const char *s = NULL;
	int printed;

	if (list == NULL) {
		printf("Empty range list\n");
		return;
	}
	printf("Range list:\n");
	printed = 0;
	for (cur = list; cur != NULL; cur = cur->next) {
		if (cur->valid) {
			switch (cur->state) {
			case kUnallocated:
				s = "unallocated";
				break;
			case kAllocated:
				continue;
			case kMultiplyAllocated:
				s = "multiply allocated";
				break;
			}
			printed = 1;
			printf("\t%u:%u %s\n", cur->start, cur->end, s);
		} else {
			switch (cur->state) {
			case kUnallocated:
				continue;
			case kAllocated:
				s = "allocated";
				break;
			case kMultiplyAllocated:
				s = "multiply allocated";
				break;
			}
			printed = 1;
			printf("\t%u:%u out of range, but %s\n", cur->start,
			    cur->end, s);
		}
	}
	if (printed == 0)
		printf("\tokay\n");
}


void
validate_map(struct partition_map_header *map)
{
	struct partition_map *entry;
	struct range_list *list;
	struct dpme *dpme;
	int i, printed;
	uint32_t limit;

	initialize_list(&list);

	/*
         * XXX signature valid
         * XXX size & count match DeviceCapacity
         * XXX number of descriptors matches array size
         * XXX each descriptor wholly contained in a partition
         * XXX the range below here is in physical blocks but the map is
         *     in logical blocks!!!
         */

	/* subtract one since args are base & len */
	add_range(&list, 1, map->block0->sbBlkCount - 1, 0);

	limit = map->blocks_in_map;
	if (limit < 1) {
		printf("No blocks in map.\n");
		return;
	}

	/* for each entry */
	for (i = 1; i <= limit; i++) {
		printf("block %d:\n", i);

		/* get entry */
		entry = find_entry_by_disk_address(i, map);
		if (entry != NULL)
			dpme = entry->dpme;
		else {
			printf("\tunable to get\n");
			goto post_processing;
		}
		printed = 0;

		/* signature matches */
		if (dpme->dpme_signature != DPME_SIGNATURE) {
			printed = 1;
			printf("\tsignature is 0x%x, should be 0x%x\n",
			    dpme->dpme_signature, DPME_SIGNATURE);
		}
		/* entry count matches */
		if (dpme->dpme_map_entries != limit) {
			printed = 1;
			printf("\tentry count is 0x%x, should be %u\n",
			    dpme->dpme_map_entries, limit);
		}
		/* lblocks contained within physical */
		if (dpme->dpme_lblock_start >= dpme->dpme_pblocks ||
		    dpme->dpme_lblocks > dpme->dpme_pblocks -
		    dpme->dpme_lblock_start) {
			printed = 1;
			printf("\tlogical blocks (%u for %u) not within "
			    "physical size (%u)\n", dpme->dpme_lblock_start,
			    dpme->dpme_lblocks, dpme->dpme_pblocks);
		}
		/* remember stuff for post processing */
		add_range(&list, dpme->dpme_pblock_start, dpme->dpme_pblocks,
		    1);

		/*
		 * XXX type is known type?
		 * XXX no unknown flags?
		 * XXX boot blocks either within or outside of logical
		 * XXX checksum matches contents
		 * XXX other fields zero if boot_bytes  is zero
		 * XXX processor id is known value?
		 * XXX no data in reserved3
		 */
		if (printed == 0)
			printf("\tokay\n");
	}

post_processing:
	/* properties of whole map */

	/* every block on disk in one & only one partition */
	coalesce_list(list);
	print_range_list(list);
	/* there is a partition for the map */
	/* map fits within partition that contains it */

	/* try to detect 512/2048 mixed partition map? */
}
