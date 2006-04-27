//
// validate.c -
//
// Written by Eryk Vershen
//

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


// for *printf()
#include <stdio.h>
// for malloc(), free()
#ifndef __linux__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
// for O_RDONLY
#include <fcntl.h>
// for errno
#include <errno.h>

#include "validate.h"
#include "deblock_media.h"
#include "pathname.h"
#include "convert.h"
#include "io.h"
#include "errors.h"


//
// Defines
//


//
// Types
//
enum range_state {
    kUnallocated,
    kAllocated,
    kMultiplyAllocated
};

struct range_list {
    struct range_list *next;
    struct range_list *prev;
    enum range_state state;
    int valid;
    u32 start;
    u32 end;
};
typedef struct range_list range_list;


//
// Global Constants
//


//
// Global Variables
//
static char *buffer;
static Block0 *b0;
static DPME *mb;
static partition_map_header *the_map;
static MEDIA the_media;
static int g;


//
// Forward declarations
//
int get_block_zero(void);
int get_block_n(int n);
range_list *new_range_list_item(enum range_state state, int valid, u32 low, u32 high);
void initialize_list(range_list **list);
void add_range(range_list **list, u32 base, u32 len, int allocate);
void print_range_list(range_list *list);
void delete_list(range_list *list);
void coalesce_list(range_list *list);


//
// Routines
//
int
get_block_zero(void)
{
    int rtn_value;

    if (the_map != NULL) {
	b0 = the_map->misc;
	rtn_value = 1;
    } else {
	if (read_media(the_media, (long long) 0, PBLOCK_SIZE, buffer) == 0) {
	    rtn_value = 0;
	} else {
	    b0 = (Block0 *) buffer;
	    convert_block0(b0, 1);
	    rtn_value = 1;
	}
    }
    return rtn_value;
}


int
get_block_n(int n)
{
    partition_map * entry;
    int rtn_value;

    if (the_map != NULL) {
	entry = find_entry_by_disk_address(n, the_map);
	if (entry != 0) {
	    mb = entry->data;
	    rtn_value = 1;
	} else {
	    rtn_value = 0;
	}
    } else {
	if (read_media(the_media, ((long long) n) * g, PBLOCK_SIZE, (void *)buffer) == 0) {
	    rtn_value = 0;
	} else {
	    mb = (DPME *) buffer;
	    convert_dpme(mb, 1);
	    rtn_value = 1;
	}
    }
    return rtn_value;
}


range_list *
new_range_list_item(enum range_state state, int valid, u32 low, u32 high)
{
    range_list *item;

    item = (range_list *) malloc(sizeof(struct range_list));
    item->next = 0;
    item->prev = 0;
    item->state = state;
    item->valid = valid;
    item->start = low;
    item->end = high;
    return item;
}


void
initialize_list(range_list **list)
{
    range_list *item;

    item = new_range_list_item(kUnallocated, 0, 0, 0xFFFFFFFF);
    *list = item;
}


void
delete_list(range_list *list)
{
    range_list *item;
    range_list *cur;

    for (cur = list; cur != 0; ) {
	item = cur;
	cur = cur->next;
	free(item);
    }
}


void
add_range(range_list **list, u32 base, u32 len, int allocate)
{
    range_list *item;
    range_list *cur;
    u32 low;
    u32 high;

    if (list == 0 || *list == 0) {
    	/* XXX initialized list will always have one element */
    	return;
    }

    low = base;
    high = base + len - 1;
    if (len == 0 || high < len - 1) {
	/* XXX wrapped around */
	return;
    }

    cur = *list;
    while (low <= high) {
	if (cur == 0) {
	    /* XXX should never occur */
	    break;
	}
	if (low <= cur->end) {
	    if (cur->start < low) {
		item = new_range_list_item(cur->state, cur->valid, cur->start, low-1);
		/* insert before here */
		if (cur->prev == 0) {
		    item->prev = 0;
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
		item = new_range_list_item(cur->state, cur->valid, high+1, cur->end);
		/* insert after here */
		if (cur->next == 0) {
		    item->next = 0;
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
coalesce_list(range_list *list)
{
    range_list *cur;
    range_list *item;

    for (cur = list; cur != 0; ) {
	item = cur->next;
	if (item == 0) {
	    break;
	}
	if (cur->valid == item->valid
		&& cur->state == item->state) {
	    cur->end = item->end;
	    cur->next = item->next;
	    if (item->next != 0) {
		item->next->prev = cur;
	    }
	    free(item);
	} else {
	    cur = cur->next;
	}
    }
}


void
print_range_list(range_list *list)
{
    range_list *cur;
    int printed;
    const char *s = NULL;

    if (list == 0) {
	printf("Empty range list\n");
	return;
    }
    printf("Range list:\n");
    printed = 0;
    for (cur = list; cur != 0; cur = cur->next) {
	if (cur->valid) {
	    switch (cur->state) {
	    case kUnallocated:
		s = "unallocated";
		break;
	    case kAllocated:
		continue;
		//s = "allocated";
		//break;
	    case kMultiplyAllocated:
		s = "multiply allocated";
		break;
	    }
	    printed = 1;
	    printf("\t%lu:%lu %s\n", cur->start, cur->end, s);
	} else {
	    switch (cur->state) {
	    case kUnallocated:
		continue;
		//s = "unallocated";
		//break;
	    case kAllocated:
		s = "allocated";
		break;
	    case kMultiplyAllocated:
		s = "multiply allocated";
		break;
	    }
	    printed = 1;
	    printf("\t%lu:%lu out of range, but %s\n", cur->start, cur->end, s);
	}
    }
    if (printed == 0) {
	printf("\tokay\n");
    }
}


void
validate_map(partition_map_header *map)
{
    range_list *list;
    char *name;
    int i;
    u32 limit;
    int printed;

    //printf("Validation not implemented yet.\n");

    if (map == NULL) {
    	the_map = 0;
	if (get_string_argument("Name of device: ", &name, 1) == 0) {
	    bad_input("Bad name");
	    return;
	}
	the_media = open_pathname_as_media(name, O_RDONLY);
	if (the_media == 0) {
	    error(errno, "can't open file '%s'", name);
	    free(name);
	    return;
	}
	g = media_granularity(the_media);
	if (g < PBLOCK_SIZE) {
	    g = PBLOCK_SIZE;
	}
   	the_media = open_deblock_media(PBLOCK_SIZE, the_media);

	buffer = malloc(PBLOCK_SIZE);
	if (buffer == NULL) {
	    error(errno, "can't allocate memory for disk buffer");
	    goto done;
	}

    } else {
    	name = 0;
	the_map = map;
	g = map->logical_block;
    }

    initialize_list(&list);

    // get block 0
    if (get_block_zero() == 0) {
	printf("unable to read block 0\n");
	goto check_map;
    }
    // XXX signature valid
    // XXX size & count match DeviceCapacity
    // XXX number of descriptors matches array size
    // XXX each descriptor wholly contained in a partition
    // XXX the range below here is in physical blocks but the map is in logical blocks!!!
    add_range(&list, 1, b0->sbBlkCount-1, 0);	/* subtract one since args are base & len */

check_map:
    // compute size of map
    if (map != NULL) {
	limit = the_map->blocks_in_map;
    } else {
	if (get_block_n(1) == 0) {
	    printf("unable to get first block\n");
	    goto done;
	} else {
	    if (mb->dpme_signature != DPME_SIGNATURE) {
	        limit = -1;
	    } else {
		limit = mb->dpme_map_entries;
	    }
	}
    }

    // for each entry
    for (i = 1; ; i++) {
	if (limit < 0) {
	    /* XXX what to use for end of list? */
	    if (i > 5) {
	    	break;
	    }
	} else if (i > limit) {
	    break;
	}

	printf("block %d:\n", i);

	// get entry
	if (get_block_n(i) == 0) {
	    printf("\tunable to get\n");
	    goto post_processing;
	}
	printed = 0;
	
	// signature matches
	if (mb->dpme_signature != DPME_SIGNATURE) {
	    printed = 1;
	    printf("\tsignature is 0x%x, should be 0x%x\n", mb->dpme_signature, DPME_SIGNATURE);
	}
	// reserved1 == 0
	if (mb->dpme_reserved_1 != 0) {
	    printed = 1;
	    printf("\treserved word is 0x%x, should be 0\n", mb->dpme_reserved_1);
	}
	// entry count matches
	if (limit < 0) {
	    printed = 1;
	    printf("\tentry count is 0x%lx, real value unknown\n", mb->dpme_map_entries);
	} else if (mb->dpme_map_entries != limit) {
	    printed = 1;
	    printf("\tentry count is 0x%lx, should be %ld\n", mb->dpme_map_entries, limit);
	}
	// lblocks contained within physical
	if (mb->dpme_lblock_start >= mb->dpme_pblocks
		|| mb->dpme_lblocks > mb->dpme_pblocks - mb->dpme_lblock_start) {
	    printed = 1;
	    printf("\tlogical blocks (%ld for %ld) not within physical size (%ld)\n",
		    mb->dpme_lblock_start, mb->dpme_lblocks, mb->dpme_pblocks);
	}
	// remember stuff for post processing
	add_range(&list, mb->dpme_pblock_start, mb->dpme_pblocks, 1);
	
	// XXX type is known type?
	// XXX no unknown flags?
	// XXX boot blocks either within or outside of logical
	// XXX checksum matches contents
	// XXX other fields zero if boot_bytes  is zero
	// XXX processor id is known value?
	// XXX no data in reserved3
	if (printed == 0) {
	    printf("\tokay\n");
	}
    }

post_processing:
    // properties of whole map

    // every block on disk in one & only one partition
    coalesce_list(list);
    print_range_list(list);
    // there is a partition for the map
    // map fits within partition that contains it

    // try to detect 512/2048 mixed partition map?

done:
    if (map == NULL) {
	close_media(the_media);
	free(buffer);
	free(name);
    }
}
