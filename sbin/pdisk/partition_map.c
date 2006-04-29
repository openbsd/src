//
// partition_map.c - partition map routines
//
// Written by Eryk Vershen
//

/*
 * Copyright 1996,1997,1998 by Apple Computer, Inc.
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

// for malloc(), calloc() & free()
#include <stdlib.h>

// for strncpy() & strcmp()
#include <string.h>
// for O_RDONLY & O_RDWR
#include <fcntl.h>
// for errno
#include <errno.h>

#include "partition_map.h"
#include "pathname.h"
#include "hfs_misc.h"
#include "deblock_media.h"
#include "io.h"
#include "convert.h"
#include "util.h"
#include "errors.h"


//
// Defines
//
#define APPLE_HFS_FLAGS_VALUE	0x4000037f
#define get_align_long(x)	(*(x))
#define put_align_long(y, x)	((*(x)) = (y))
// #define TEST_COMPUTE


//
// Types
//


//
// Global Constants
//
const char * kFreeType	= "Apple_Free";
const char * kMapType	= "Apple_partition_map";
#ifdef __m68k__
const char * kUnixType  = "Apple_UNIX_SVR2";
#else
const char * kUnixType	= "OpenBSD";
#endif
const char * kHFSType	= "Apple_HFS";
const char * kPatchType	= "Apple_Patches";

const char * kFreeName	= "Extra";

enum add_action {
    kReplace = 0,
    kAdd = 1,
    kSplit = 2
};

//
// Global Variables
//
extern int cflag;


//
// Forward declarations
//
int add_data_to_map(struct dpme *, long, partition_map_header *);
int coerce_block0(partition_map_header *map);
int contains_driver(partition_map *entry);
void combine_entry(partition_map *entry);
long compute_device_size(partition_map_header *map, partition_map_header *oldmap);
DPME* create_data(const char *name, const char *dptype, u32 base, u32 length);
void delete_entry(partition_map *entry);
char *get_HFS_name(partition_map *entry, int *kind);
void insert_in_base_order(partition_map *entry);
void insert_in_disk_order(partition_map *entry);
int read_block(partition_map_header *map, unsigned long num, char *buf);
int read_partition_map(partition_map_header *map);
void remove_driver(partition_map *entry);
void remove_from_disk_order(partition_map *entry);
void renumber_disk_addresses(partition_map_header *map);
void sync_device_size(partition_map_header *map);
int write_block(partition_map_header *map, unsigned long num, char *buf);


//
// Routines
//
partition_map_header *
open_partition_map(char *name, int *valid_file, int ask_logical_size)
{
    MEDIA m;
    partition_map_header * map;
    int writable;
    long size;

    m = open_pathname_as_media(name, (rflag)?O_RDONLY:O_RDWR);
    if (m == 0) {
	m = open_pathname_as_media(name, O_RDONLY);
	if (m == 0) {
	    error(errno, "can't open file '%s'", name);
	    *valid_file = 0;
	    return NULL;
	} else {
	    writable = 0;
	}
    } else {
	writable = 1;
    }
    *valid_file = 1;

    map = (partition_map_header *) malloc(sizeof(partition_map_header));
    if (map == NULL) {
	error(errno, "can't allocate memory for open partition map");
	close_media(m);
	return NULL;
    }
    map->name = name;
    map->writable = (rflag)?0:writable;
    map->changed = 0;
    map->written = 0;
    map->disk_order = NULL;
    map->base_order = NULL;

    map->physical_block = media_granularity(m);	/* preflight */
    m = open_deblock_media(PBLOCK_SIZE, m);
    map->m = m;
    map->misc = (Block0 *) malloc(PBLOCK_SIZE);
    if (map->misc == NULL) {
	error(errno, "can't allocate memory for block zero buffer");
	close_media(map->m);
	free(map);
	return NULL;
    } else if (read_media(map->m, (long long) 0, PBLOCK_SIZE, (char *)map->misc) == 0
	    || convert_block0(map->misc, 1)
	    || coerce_block0(map)) {
	// if I can't read block 0 I might as well give up
	error(-1, "Can't read block 0 from '%s'", name);
	close_partition_map(map);
	return NULL;
    }
    map->physical_block = map->misc->sbBlkSize;
    //printf("physical block size is %d\n", map->physical_block);

    if (ask_logical_size && interactive) {
	size = PBLOCK_SIZE;
	printf("A logical block is %ld bytes: ", size);
	flush_to_newline(0);
	get_number_argument("what should be the logical block size? ",
		&size, size);
	size = (size / PBLOCK_SIZE) * PBLOCK_SIZE;
	if (size < PBLOCK_SIZE) {
	    size = PBLOCK_SIZE;
	}
	map->logical_block = size;
    } else {
	map->logical_block = PBLOCK_SIZE;
    }
    if (map->logical_block > MAXIOSIZE) {
	map->logical_block = MAXIOSIZE;
    }
    if (map->logical_block > map->physical_block) {
	map->physical_block = map->logical_block;
    }
    map->blocks_in_map = 0;
    map->maximum_in_map = -1;
    map->media_size = compute_device_size(map, map);
    sync_device_size(map);

    if (read_partition_map(map) < 0) {
	// some sort of failure reading the map
    } else {
	// got it!
	;
	return map;
    }
    close_partition_map(map);
    return NULL;
}


void
close_partition_map(partition_map_header *map)
{
    partition_map * entry;
    partition_map * next;

    if (map == NULL) {
	return;
    }

    free(map->misc);

    for (entry = map->disk_order; entry != NULL; entry = next) {
	next = entry->next_on_disk;
	free(entry->data);
	free(entry->HFS_name);
	free(entry);
    }
    close_media(map->m);
    free(map);
}


int
read_partition_map(partition_map_header *map)
{
    DPME *data;
    u32 limit;
    int ix;
    int old_logical;
    double d;

//printf("called read_partition_map\n");
//printf("logical = %d, physical = %d\n", map->logical_block, map->physical_block);
    data = (DPME *) malloc(PBLOCK_SIZE);
    if (data == NULL) {
	error(errno, "can't allocate memory for disk buffers");
	return -1;
    }

    if (read_block(map, 1, (char *)data) == 0) {
	error(-1, "Can't read block 1 from '%s'", map->name);
	free(data);
	return -1;
    } else if (convert_dpme(data, 1)
	    || data->dpme_signature != DPME_SIGNATURE) {
	old_logical = map->logical_block;
	map->logical_block = 512;
	while (map->logical_block <= map->physical_block) {
	    if (read_block(map, 1, (char *)data) == 0) {
		error(-1, "Can't read block 1 from '%s'", map->name);
		free(data);
		return -1;
	    } else if (convert_dpme(data, 1) == 0
		    && data->dpme_signature == DPME_SIGNATURE) {
		d = map->media_size;
		map->media_size =  (d * old_logical) / map->logical_block;
		break;
	    }
	    map->logical_block *= 2;
	}
	if (map->logical_block > map->physical_block) {
	    error(-1, "No valid block 1 on '%s'", map->name);
	    free(data);
	    return -1;
	}
    }
//printf("logical = %d, physical = %d\n", map->logical_block, map->physical_block);

    limit = data->dpme_map_entries;
    ix = 1;
    while (1) {
	if (add_data_to_map(data, ix, map) == 0) {
	    free(data);
	    return -1;
	}

	if (ix >= limit) {
	    break;
	} else {
	    ix++;
	}

	data = (DPME *) malloc(PBLOCK_SIZE);
	if (data == NULL) {
	    error(errno, "can't allocate memory for disk buffers");
	    return -1;
	}

	if (read_block(map, ix, (char *)data) == 0) {
	    error(-1, "Can't read block %u from '%s'", ix, map->name);
	    free(data);
	    return -1;
	} else if (convert_dpme(data, 1)
		|| (data->dpme_signature != DPME_SIGNATURE && dflag == 0)
		|| (data->dpme_map_entries != limit && dflag == 0)) {
	    error(-1, "Bad data in block %u from '%s'", ix, map->name);
	    free(data);
	    return -1;
	}
    }
    return 0;
}


void
write_partition_map(partition_map_header *map)
{
    MEDIA m;
    char *block;
    partition_map * entry;
    int i = 0;
    int result = 0;

    m = map->m;
    if (map->misc != NULL) {
	convert_block0(map->misc, 0);
	result = write_block(map, 0, (char *)map->misc);
	convert_block0(map->misc, 1);
    } else {
	block = (char *) calloc(1, PBLOCK_SIZE);
	if (block != NULL) {
	    result = write_block(map, 0, block);
	    free(block);
	}
    }
    if (result == 0) {
	error(errno, "Unable to write block zero");
    }
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	convert_dpme(entry->data, 0);
	result = write_block(map, entry->disk_address, (char *)entry->data);
	convert_dpme(entry->data, 1);
	i = entry->disk_address;
	if (result == 0) {
	    error(errno, "Unable to write block %d", i);
	}
    }

    if (interactive)
	printf("The partition table has been altered!\n\n");

    os_reload_media(map->m);
}


int
add_data_to_map(struct dpme *data, long ix, partition_map_header *map)
{
    partition_map *entry;

//printf("add data %d to map\n", ix);
    entry = (partition_map *) malloc(sizeof(partition_map));
    if (entry == NULL) {
	error(errno, "can't allocate memory for map entries");
	return 0;
    }
    entry->next_on_disk = NULL;
    entry->prev_on_disk = NULL;
    entry->next_by_base = NULL;
    entry->prev_by_base = NULL;
    entry->disk_address = ix;
    entry->the_map = map;
    entry->data = data;
    entry->contains_driver = contains_driver(entry);
    entry->HFS_name = get_HFS_name(entry, &entry->HFS_kind);

    insert_in_disk_order(entry);
    insert_in_base_order(entry);

    map->blocks_in_map++;
    if (map->maximum_in_map < 0) {
	if (istrncmp(data->dpme_type, kMapType, DPISTRLEN) == 0) {
	    map->maximum_in_map = data->dpme_pblocks;
	}
    }

    return 1;
}


partition_map_header *
init_partition_map(char *name, partition_map_header* oldmap)
{
    partition_map_header *map;

    if (oldmap != NULL) {
	printf("map already exists\n");
	if (get_okay("do you want to reinit? [n/y]: ", 0) != 1) {
	    return oldmap;
	}
    }

    map = create_partition_map(name, oldmap);
    if (map == NULL) {
	return oldmap;
    }
    close_partition_map(oldmap);

    add_partition_to_map("Apple", kMapType,
	    1, (map->media_size <= 128? 2: 63), map);
    return map;
}


partition_map_header *
create_partition_map(char *name, partition_map_header *oldmap)
{
    MEDIA m;
    partition_map_header * map;
    DPME *data;
    unsigned long default_number;
    unsigned long number;
    long size;
    unsigned long multiple;

    m = open_pathname_as_media(name, (rflag)?O_RDONLY:O_RDWR);
    if (m == 0) {
	error(errno, "can't open file '%s' for %sing", name,
		(rflag)?"read":"writ");
	return NULL;
    }

    map = (partition_map_header *) malloc(sizeof(partition_map_header));
    if (map == NULL) {
	error(errno, "can't allocate memory for open partition map");
	close_media(m);
	return NULL;
    }
    map->name = name;
    map->writable = (rflag)?0:1;
    map->changed = 1;
    map->disk_order = NULL;
    map->base_order = NULL;

    if (oldmap != NULL) {
	size = oldmap->physical_block;
    } else {
	size = media_granularity(m);
    }
    m = open_deblock_media(PBLOCK_SIZE, m);
    map->m = m;
    if (interactive) {
	printf("A physical block is %ld bytes: ", size);
	flush_to_newline(0);
	get_number_argument("what should be the physical block size? ",
		&size, size);
	size = (size / PBLOCK_SIZE) * PBLOCK_SIZE;
	if (size < PBLOCK_SIZE) {
	    size = PBLOCK_SIZE;
	}
    }
    if (map->physical_block > MAXIOSIZE) {
	map->physical_block = MAXIOSIZE;
    }
    map->physical_block = size;
    // printf("block size is %d\n", map->physical_block);

    if (oldmap != NULL) {
	size = oldmap->logical_block;
    } else {
	size = PBLOCK_SIZE;
    }
    if (interactive) {
	printf("A logical block is %ld bytes: ", size);
	flush_to_newline(0);
	get_number_argument("what should be the logical block size? ",
		&size, size);
	size = (size / PBLOCK_SIZE) * PBLOCK_SIZE;
	if (size < PBLOCK_SIZE) {
	    size = PBLOCK_SIZE;
	}
    }
#if 0
    if (size > map->physical_block) {
	size = map->physical_block;
    }
#endif
    map->logical_block = size;

    map->blocks_in_map = 0;
    map->maximum_in_map = -1;

    number = compute_device_size(map, oldmap);
    if (interactive) {
	printf("size of 'device' is %lu blocks (%d byte blocks): ",
		number, map->logical_block);
	default_number = number;
	flush_to_newline(0);
	do {
	    if (get_number_argument("what should be the size? ",
		    (long *)&number, default_number) == 0) {
		printf("Not a number\n");
		flush_to_newline(1);
		number = 0;
	    } else {
		multiple = get_multiplier(map->logical_block);
		if (multiple == 0) {
		    printf("Bad multiplier\n");
		    number = 0;
		} else if (multiple != 1) {
		    if (0xFFFFFFFF/multiple < number) {
			printf("Number too large\n");
			number = 0;
		    } else {
			number *= multiple;
		    }
		}
	    }
	    default_number = kDefault;
	} while (number == 0);

	if (number < 4) {
	    number = 4;
	}
	printf("new size of 'device' is %lu blocks (%d byte blocks)\n",
		number, map->logical_block);
    }
    map->media_size = number;

    map->misc = (Block0 *) calloc(1, PBLOCK_SIZE);
    if (map->misc == NULL) {
	error(errno, "can't allocate memory for block zero buffer");
    } else {
	// got it!
	coerce_block0(map);
	sync_device_size(map);
	
	data = (DPME *) calloc(1, PBLOCK_SIZE);
	if (data == NULL) {
	    error(errno, "can't allocate memory for disk buffers");
	} else {
	    // set data into entry
	    data->dpme_signature = DPME_SIGNATURE;
	    data->dpme_map_entries = 1;
	    data->dpme_pblock_start = 1;
	    data->dpme_pblocks = map->media_size - 1;
	    strncpy(data->dpme_name, kFreeName, DPISTRLEN);
	    strncpy(data->dpme_type, kFreeType, DPISTRLEN);
	    data->dpme_lblock_start = 0;
	    data->dpme_lblocks = data->dpme_pblocks;
	    dpme_writable_set(data, 1);
	    dpme_readable_set(data, 1);
	    dpme_bootable_set(data, 0);
	    dpme_in_use_set(data, 0);
	    dpme_allocated_set(data, 0);
	    dpme_valid_set(data, 1);

	    if (add_data_to_map(data, 1, map) == 0) {
		free(data);
	    } else {
		return map;
	    }
	}
    }
    close_partition_map(map);
    return NULL;
}


int
coerce_block0(partition_map_header *map)
{
    Block0 *p;

    p = map->misc;
    if (p == NULL) {
	return 1;
    }
    if (p->sbSig != BLOCK0_SIGNATURE) {
	p->sbSig = BLOCK0_SIGNATURE;
	if (map->physical_block == 1) {
	    p->sbBlkSize = PBLOCK_SIZE;
	} else {
	    p->sbBlkSize = map->physical_block;
	}
	p->sbBlkCount = 0;
	p->sbDevType = 0;
	p->sbDevId = 0;
	p->sbData = 0;
	p->sbDrvrCount = 0;
    }
    return 0;	// we do this simply to make it easier to call this function
}


int
add_partition_to_map(const char *name, const char *dptype, u32 base, u32 length,
	partition_map_header *map)
{
    partition_map * cur;
    DPME *data;
    enum add_action act;
    int limit;
    u32 adjusted_base = 0;
    u32 adjusted_length = 0;
    u32 new_base = 0;
    u32 new_length = 0;

	// find a block that starts includes base and length
    cur = map->base_order;
    while (cur != NULL) {
	if (cur->data->dpme_pblock_start <= base
		&& (base + length) <=
		    (cur->data->dpme_pblock_start + cur->data->dpme_pblocks)) {
	    break;
	} else {
	  // check if request is past end of existing partitions, but on disk
	  if ((cur->next_by_base == NULL) &&
	      (base + length <= map->media_size)) {
	    // Expand final free partition
	    if ((istrncmp(cur->data->dpme_type, kFreeType, DPISTRLEN) == 0) &&
		base >= cur->data->dpme_pblock_start) {
	      cur->data->dpme_pblocks =
		map->media_size - cur->data->dpme_pblock_start;
	      break;
	    }
	    // create an extra free partition
	    if (base >= cur->data->dpme_pblock_start + cur->data->dpme_pblocks) {
	      if (map->maximum_in_map < 0) {
		limit = map->media_size;
	      } else {
		limit = map->maximum_in_map;
	      }
	      if (map->blocks_in_map + 1 > limit) {
		printf("the map is not big enough\n");
		return 0;
	      }
	      data = create_data(kFreeName, kFreeType,
		  cur->data->dpme_pblock_start + cur->data->dpme_pblocks,
		  map->media_size - (cur->data->dpme_pblock_start + cur->data->dpme_pblocks));
	      if (data != NULL) {
		if (add_data_to_map(data, cur->disk_address, map) == 0) {
		  free(data);
		}
	      }
	    }
	  }
	  cur = cur->next_by_base;
	}
    }
	// if it is not Extra then punt
    if (cur == NULL
	    || istrncmp(cur->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	printf("requested base and length is not "
		"within an existing free partition\n");
	return 0;
    }
	// figure out what to do and sizes
    data = cur->data;
    if (data->dpme_pblock_start == base) {
	// replace or add
	if (data->dpme_pblocks == length) {
	    act = kReplace;
	} else {
	    act = kAdd;
	    adjusted_base = base + length;
	    adjusted_length = data->dpme_pblocks - length;
	}
    } else {
	// split or add
	if (data->dpme_pblock_start + data->dpme_pblocks == base + length) {
	    act = kAdd;
	    adjusted_base = data->dpme_pblock_start;
	    adjusted_length = base - adjusted_base;
	} else {
	    act = kSplit;
	    new_base = data->dpme_pblock_start;
	    new_length = base - new_base;
	    adjusted_base = base + length;
	    adjusted_length = data->dpme_pblocks - (length + new_length);
	}
    }
	// if the map will overflow then punt
    if (map->maximum_in_map < 0) {
	limit = map->media_size;
    } else {
	limit = map->maximum_in_map;
    }
    if (map->blocks_in_map + act > limit) {
	printf("the map is not big enough\n");
	return 0;
    }

    data = create_data(name, dptype, base, length);
    if (data == NULL) {
	return 0;
    }
    if (act == kReplace) {
	free(cur->data);
	cur->data = data;
    } else {
	    // adjust this block's size
	cur->data->dpme_pblock_start = adjusted_base;
	cur->data->dpme_pblocks = adjusted_length;
	cur->data->dpme_lblocks = adjusted_length;
	    // insert new with block address equal to this one
	if (add_data_to_map(data, cur->disk_address, map) == 0) {
	    free(data);
	} else if (act == kSplit) {
	    data = create_data(kFreeName, kFreeType, new_base, new_length);
	    if (data != NULL) {
		    // insert new with block address equal to this one
		if (add_data_to_map(data, cur->disk_address, map) == 0) {
		    free(data);
		}
	    }
	}
    }
	// renumber disk addresses
    renumber_disk_addresses(map);
	// mark changed
    map->changed = 1;
    return 1;
}


DPME *
create_data(const char *name, const char *dptype, u32 base, u32 length)
{
    DPME *data;

    data = (DPME *) calloc(1, PBLOCK_SIZE);
    if (data == NULL) {
	error(errno, "can't allocate memory for disk buffers");
    } else {
	// set data into entry
	data->dpme_signature = DPME_SIGNATURE;
	data->dpme_map_entries = 1;
	data->dpme_pblock_start = base;
	data->dpme_pblocks = length;
	strncpy(data->dpme_name, name, DPISTRLEN);
	strncpy(data->dpme_type, dptype, DPISTRLEN);
	data->dpme_lblock_start = 0;
	data->dpme_lblocks = data->dpme_pblocks;
	dpme_init_flags(data);
    }
    return data;
}

void
dpme_init_flags(DPME *data)
{
    if (istrncmp(data->dpme_type, kHFSType, DPISTRLEN) == 0) { /* XXX this is gross, fix it! */
	data->dpme_flags = APPLE_HFS_FLAGS_VALUE;
    }
    else {
	dpme_writable_set(data, 1);
	dpme_readable_set(data, 1);
	dpme_bootable_set(data, 0);
	dpme_in_use_set(data, 0);
	dpme_allocated_set(data, 1);
	dpme_valid_set(data, 1);
    }
}

/* These bits are appropriate for Apple_UNIX_SVR2 partitions
 * used by OpenBSD.  They may be ok for A/UX, but have not been
 * tested.
 */
void
bzb_init_slice(BZB *bp, int slice)
{
    memset(bp,0,sizeof(BZB));
    if ((slice >= 'A') && (slice <= 'Z')) {
	slice += 'a' - 'A';
    }
    if ((slice != 0) && ((slice < 'a') || (slice > 'z'))) {
	error(-1,"Bad bzb slice");
	slice = 0;
    }
    switch (slice) {
    case 0:
    case 'c':
	return;
    case 'a':
	bp->bzb_type = FST;
	strlcpy(bp->bzb_mount_point, "/", sizeof(bp->bzb_mount_point));
	bp->bzb_inode = 1;
	bzb_root_set(bp,1);
	bzb_usr_set(bp,1);
	break;
    case 'b':
	bp->bzb_type = FSTSFS;
	strlcpy(bp->bzb_mount_point, "(swap)", sizeof(bp->bzb_mount_point));
	break;
    case 'g':
	strlcpy(bp->bzb_mount_point, "/usr", sizeof(bp->bzb_mount_point));
	/* Fall through */
    default:
	bp->bzb_type = FST;
	bp->bzb_inode = 1;
	bzb_usr_set(bp,1);
	break;
    }
    bzb_slice_set(bp,0);  // XXX OpenBSD disksubr.c ignores slice
    //	bzb_slice_set(bp,slice-'a'+1);
    bp->bzb_magic = BZBMAGIC;
}

void
renumber_disk_addresses(partition_map_header *map)
{
    partition_map * cur;
    long ix;

	// reset disk addresses
    cur = map->disk_order;
    ix = 1;
    while (cur != NULL) {
	cur->disk_address = ix++;
	cur->data->dpme_map_entries = map->blocks_in_map;
	cur = cur->next_on_disk;
    }
}


long
compute_device_size(partition_map_header *map, partition_map_header *oldmap)
{
#ifdef TEST_COMPUTE
    unsigned long length;
    struct hd_geometry geometry;
    struct stat info;
    loff_t pos;
#endif
    char* data;
    unsigned long l, r, x = 0;
    long long size;
    int valid = 0;
#ifdef TEST_COMPUTE
    int fd;

    fd = map->fd->fd;
    printf("\n");
    if (fstat(fd, &info) < 0) {
	printf("stat of device failed\n");
    } else {
	printf("stat: mode = 0%o, type=%s\n", info.st_mode,
		(S_ISREG(info.st_mode)? "Regular":
		(S_ISBLK(info.st_mode)?"Block":"Other")));
	printf("size = %d, blocks = %d\n",
		info.st_size, info.st_size/map->logical_block);
    }

    if (ioctl(fd, BLKGETSIZE, &length) < 0) {
	printf("get device size failed\n");
    } else {
	printf("BLKGETSIZE:size in blocks = %u\n", length);
    }

    if (ioctl(fd, HDIO_GETGEO, &geometry) < 0) {
	printf("get device geometry failed\n");
    } else {
	printf("HDIO_GETGEO: heads=%d, sectors=%d, cylinders=%d, start=%d,  total=%d\n",
		geometry.heads, geometry.sectors,
		geometry.cylinders, geometry.start,
		geometry.heads*geometry.sectors*geometry.cylinders);
    }

    if ((pos = llseek(fd, (loff_t)0, SEEK_END)) < 0) {
	printf("llseek to end of device failed\n");
    } else if ((pos = llseek(fd, (loff_t)0, SEEK_CUR)) < 0) {
	printf("llseek to end of device failed on second try\n");
    } else {
	printf("llseek: pos = %d, blocks=%d\n", pos, pos/map->logical_block);
    }
#endif

    if (cflag == 0 && oldmap != NULL && oldmap->misc->sbBlkCount != 0) {
	return (oldmap->misc->sbBlkCount
		* (oldmap->physical_block / map->logical_block));
    }

    size = media_total_size(map->m);
    if (size != 0) {
    	return (long)(size / map->logical_block);
    }

    // else case

    data = (char *) malloc(PBLOCK_SIZE);
    if (data == NULL) {
	error(errno, "can't allocate memory for try buffer");
	x = 0;
    } else {
	// double till off end
	l = 0;
	r = 1024;
	while (read_block(map, r, data) != 0) {
	    l = r;
	    if (r <= 1024) {
		r = r * 1024;
	    } else {
		r = r * 2;
	    }
	    if (r >= 0x80000000) {
		r = 0xFFFFFFFE;
		break;
	    }
	}
	// binary search for end
	while (l <= r) {
	    x = (r - l) / 2 + l;
	    if ((valid = read_block(map, x, data)) != 0) {
		l = x + 1;
	    } else {
		if (x > 0) {
		    r = x - 1;
		} else {
		    break;
		}
	    }
	}
	if (valid != 0) {
	    x = x + 1;
	}
	// printf("size in blocks = %d\n", x);
	free(data);
    }

    return x;
}


void
sync_device_size(partition_map_header *map)
{
    Block0 *p;
    unsigned long size;
    double d;

    p = map->misc;
    if (p == NULL) {
	return;
    }
    d = map->media_size;
    size = (d * map->logical_block) / p->sbBlkSize;
    if (p->sbBlkCount != size) {
	p->sbBlkCount = size;
    }
}


void
delete_partition_from_map(partition_map *entry)
{
    partition_map_header *map;
    DPME *data;

    if (istrncmp(entry->data->dpme_type, kMapType, DPISTRLEN) == 0) {
	printf("Can't delete entry for the map itself\n");
	return;
    }
    if (entry->contains_driver) {
	printf("This program can't install drivers\n");
	if (get_okay("are you sure you want to delete this driver? [n/y]: ", 0) != 1) {
	    return;
	}
    }
    // if past end of disk, delete it completely
    if (entry->next_by_base == NULL &&
	entry->data->dpme_pblock_start >= entry->the_map->media_size) {
      if (entry->contains_driver) {
	remove_driver(entry);	// update block0 if necessary
      }
      delete_entry(entry);
      return;
    }
    // If at end of disk, incorporate extra disk space to partition
    if (entry->next_by_base == NULL) {
      entry->data->dpme_pblocks =
	 entry->the_map->media_size - entry->data->dpme_pblock_start;
    }
    data = create_data(kFreeName, kFreeType,
	    entry->data->dpme_pblock_start, entry->data->dpme_pblocks);
    if (data == NULL) {
	return;
    }
    if (entry->contains_driver) {
    	remove_driver(entry);	// update block0 if necessary
    }
    free(entry->data);
    free(entry->HFS_name);
    entry->HFS_kind = kHFS_not;
    entry->HFS_name = 0;
    entry->data = data;
    combine_entry(entry);
    map = entry->the_map;
    renumber_disk_addresses(map);
    map->changed = 1;
}


int
contains_driver(partition_map *entry)
{
    partition_map_header *map;
    Block0 *p;
    DDMap *m;
    int i;
    int f;
    u32 start;

    map = entry->the_map;
    p = map->misc;
    if (p == NULL) {
	return 0;
    }
    if (p->sbSig != BLOCK0_SIGNATURE) {
	return 0;
    }
    if (map->logical_block > p->sbBlkSize) {
	return 0;
    } else {
	f = p->sbBlkSize / map->logical_block;
    }
    if (p->sbDrvrCount > 0) {
	m = (DDMap *) p->sbMap;
	for (i = 0; i < p->sbDrvrCount; i++) {
	    start = get_align_long(&m[i].ddBlock);
	    if (entry->data->dpme_pblock_start <= f*start
		    && f*(start + m[i].ddSize)
			<= (entry->data->dpme_pblock_start
			+ entry->data->dpme_pblocks)) {
		return 1;
	    }
	}
    }
    return 0;
}


void
combine_entry(partition_map *entry)
{
    partition_map *p;
    u32 end;

    if (entry == NULL
	    || istrncmp(entry->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	return;
    }
    if (entry->next_by_base != NULL) {
	p = entry->next_by_base;
	if (istrncmp(p->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	    // next is not free
	} else if (entry->data->dpme_pblock_start + entry->data->dpme_pblocks
		!= p->data->dpme_pblock_start) {
	    // next is not contiguous (XXX this is bad)
	    printf("next entry is not contiguous\n");
	    // start is already minimum
	    // new end is maximum of two ends
	    end = p->data->dpme_pblock_start + p->data->dpme_pblocks;
	    if (end > entry->data->dpme_pblock_start + entry->data->dpme_pblocks) {
	    	entry->data->dpme_pblocks = end - entry->data->dpme_pblock_start;
	    }
	    entry->data->dpme_lblocks = entry->data->dpme_pblocks;
	    delete_entry(p);
	} else {
	    entry->data->dpme_pblocks += p->data->dpme_pblocks;
	    entry->data->dpme_lblocks = entry->data->dpme_pblocks;
	    delete_entry(p);
	}
    }
    if (entry->prev_by_base != NULL) {
	p = entry->prev_by_base;
	if (istrncmp(p->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	    // previous is not free
	} else if (p->data->dpme_pblock_start + p->data->dpme_pblocks
		!= entry->data->dpme_pblock_start) {
	    // previous is not contiguous (XXX this is bad)
	    printf("previous entry is not contiguous\n");
	    // new end is maximum of two ends
	    end = p->data->dpme_pblock_start + p->data->dpme_pblocks;
	    if (end < entry->data->dpme_pblock_start + entry->data->dpme_pblocks) {
		end = entry->data->dpme_pblock_start + entry->data->dpme_pblocks;
	    }
	    entry->data->dpme_pblocks = end - p->data->dpme_pblock_start;
	    // new start is previous entry's start
	    entry->data->dpme_pblock_start = p->data->dpme_pblock_start;
	    entry->data->dpme_lblocks = entry->data->dpme_pblocks;
	    delete_entry(p);
	} else {
	    entry->data->dpme_pblock_start = p->data->dpme_pblock_start;
	    entry->data->dpme_pblocks += p->data->dpme_pblocks;
	    entry->data->dpme_lblocks = entry->data->dpme_pblocks;
	    delete_entry(p);
	}
    }
    entry->contains_driver = contains_driver(entry);
}


void
delete_entry(partition_map *entry)
{
    partition_map_header *map;
    partition_map *p;

    map = entry->the_map;
    map->blocks_in_map--;

    remove_from_disk_order(entry);

    p = entry->next_by_base;
    if (map->base_order == entry) {
	map->base_order = p;
    }
    if (p != NULL) {
	p->prev_by_base = entry->prev_by_base;
    }
    if (entry->prev_by_base != NULL) {
	entry->prev_by_base->next_by_base = p;
    }

    free(entry->data);
    free(entry->HFS_name);
    free(entry);
}


partition_map *
find_entry_by_disk_address(long ix, partition_map_header *map)
{
    partition_map * cur;

    cur = map->disk_order;
    while (cur != NULL) {
	if (cur->disk_address == ix) {
	    break;
	}
	cur = cur->next_on_disk;
    }
    return cur;
}


partition_map *
find_entry_by_type(const char *type_name, partition_map_header *map)
{
    partition_map * cur;

    cur = map->base_order;
    while (cur != NULL) {
	if (istrncmp(cur->data->dpme_type, type_name, DPISTRLEN) == 0) {
	    break;
	}
	cur = cur->next_by_base;
    }
    return cur;
}

partition_map *
find_entry_by_base(u32 base, partition_map_header *map)
{
    partition_map * cur;

    cur = map->base_order;
    while (cur != NULL) {
	if (cur->data->dpme_pblock_start == base) {
	    break;
	}
	cur = cur->next_by_base;
    }
    return cur;
}


void
move_entry_in_map(long old_index, long ix, partition_map_header *map)
{
    partition_map * cur;

    cur = find_entry_by_disk_address(old_index, map);
    if (cur == NULL) {
	printf("No such partition\n");
    } else {
	remove_from_disk_order(cur);
	cur->disk_address = ix;
	insert_in_disk_order(cur);
	renumber_disk_addresses(map);
	map->changed = 1;
    }
}


void
remove_from_disk_order(partition_map *entry)
{
    partition_map_header *map;
    partition_map *p;

    map = entry->the_map;
    p = entry->next_on_disk;
    if (map->disk_order == entry) {
	map->disk_order = p;
    }
    if (p != NULL) {
	p->prev_on_disk = entry->prev_on_disk;
    }
    if (entry->prev_on_disk != NULL) {
	entry->prev_on_disk->next_on_disk = p;
    }
    entry->next_on_disk = NULL;
    entry->prev_on_disk = NULL;
}


void
insert_in_disk_order(partition_map *entry)
{
    partition_map_header *map;
    partition_map * cur;

    // find position in disk list & insert
    map = entry->the_map;
    cur = map->disk_order;
    if (cur == NULL || entry->disk_address <= cur->disk_address) {
	map->disk_order = entry;
	entry->next_on_disk = cur;
	if (cur != NULL) {
	    cur->prev_on_disk = entry;
	}
	entry->prev_on_disk = NULL;
    } else {
	for (cur = map->disk_order; cur != NULL; cur = cur->next_on_disk) {
	    if (cur->disk_address <= entry->disk_address
		    && (cur->next_on_disk == NULL
		    || entry->disk_address <= cur->next_on_disk->disk_address)) {
		entry->next_on_disk = cur->next_on_disk;
		cur->next_on_disk = entry;
		entry->prev_on_disk = cur;
		if (entry->next_on_disk != NULL) {
		    entry->next_on_disk->prev_on_disk = entry;
		}
		break;
	    }
	}
    }
}


void
insert_in_base_order(partition_map *entry)
{
    partition_map_header *map;
    partition_map * cur;

    // find position in base list & insert
    map = entry->the_map;
    cur = map->base_order;
    if (cur == NULL
	    || entry->data->dpme_pblock_start <= cur->data->dpme_pblock_start) {
	map->base_order = entry;
	entry->next_by_base = cur;
	if (cur != NULL) {
	    cur->prev_by_base = entry;
	}
	entry->prev_by_base = NULL;
    } else {
	for (cur = map->base_order; cur != NULL; cur = cur->next_by_base) {
	    if (cur->data->dpme_pblock_start <= entry->data->dpme_pblock_start
		    && (cur->next_by_base == NULL
		    || entry->data->dpme_pblock_start
			<= cur->next_by_base->data->dpme_pblock_start)) {
		entry->next_by_base = cur->next_by_base;
		cur->next_by_base = entry;
		entry->prev_by_base = cur;
		if (entry->next_by_base != NULL) {
		    entry->next_by_base->prev_by_base = entry;
		}
		break;
	    }
	}
    }
}


void
resize_map(long new_size, partition_map_header *map)
{
    partition_map * entry;
    partition_map * next;
    int incr;

    // find map entry
    entry = find_entry_by_type(kMapType, map);

    if (entry == NULL) {
	printf("Couldn't find entry for map!\n");
	return;
    }
    next = entry->next_by_base;

	// same size
    if (new_size == entry->data->dpme_pblocks) {
	// do nothing
	return;
    }

	// make it smaller
    if (new_size < entry->data->dpme_pblocks) {
	if (next == NULL
		|| istrncmp(next->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	    incr = 1;
	} else {
	    incr = 0;
	}
	if (new_size < map->blocks_in_map + incr) {
	    printf("New size would be too small\n");
	    return;
	}
	goto doit;
    }

	// make it larger
    if (next == NULL
	    || istrncmp(next->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	printf("No free space to expand into\n");
	return;
    }
    if (entry->data->dpme_pblock_start + entry->data->dpme_pblocks
	    != next->data->dpme_pblock_start) {
	printf("No contiguous free space to expand into\n");
	return;
    }
    if (new_size > entry->data->dpme_pblocks + next->data->dpme_pblocks) {
	printf("No enough free space\n");
	return;
    }
doit:
    entry->data->dpme_type[0] = 0;
    delete_partition_from_map(entry);
    add_partition_to_map("Apple", kMapType, 1, new_size, map);
    map->maximum_in_map = new_size;
}


void
remove_driver(partition_map *entry)
{
    partition_map_header *map;
    Block0 *p;
    DDMap *m;
    int i;
    int j;
    int f;
    u32 start;

    map = entry->the_map;
    p = map->misc;
    if (p == NULL) {
	return;
    }
    if (p->sbSig != BLOCK0_SIGNATURE) {
	return;
    }
    if (map->logical_block > p->sbBlkSize) {
	/* this is not supposed to happen, but let's just ignore it. */
	return;
    } else {
	/*
	 * compute the factor to convert the block numbers in block0
	 * into partition map block numbers.
	 */
	f = p->sbBlkSize / map->logical_block;
    }
    if (p->sbDrvrCount > 0) {
	m = (DDMap *) p->sbMap;
	for (i = 0; i < p->sbDrvrCount; i++) {
	    start = get_align_long(&m[i].ddBlock);

	    /* zap the driver if it is wholly contained in the partition */
	    if (entry->data->dpme_pblock_start <= f*start
		    && f*(start + m[i].ddSize)
			<= (entry->data->dpme_pblock_start
			+ entry->data->dpme_pblocks)) {
		// delete this driver
		// by copying down later ones and zapping the last
		for (j = i+1; j < p->sbDrvrCount; j++, i++) {
		   put_align_long(get_align_long(&m[j].ddBlock), &m[i].ddBlock);
		   m[i].ddSize = m[j].ddSize;
		   m[i].ddType = m[j].ddType;
		}
	        put_align_long(0, &m[i].ddBlock);
		m[i].ddSize = 0;
		m[i].ddType = 0;
		p->sbDrvrCount -= 1;
		return; /* XXX if we continue we will delete other drivers? */
	    }
	}
    }
}

int
read_block(partition_map_header *map, unsigned long num, char *buf)
{
//printf("read block %d\n", num);
    return read_media(map->m, ((long long) num) * map->logical_block,
    		PBLOCK_SIZE, (void *)buf);
}


int
write_block(partition_map_header *map, unsigned long num, char *buf)
{
    return write_media(map->m, ((long long) num) * map->logical_block,
    		PBLOCK_SIZE, (void *)buf);
}
