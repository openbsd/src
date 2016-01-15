/*	$OpenBSD: dump.c,v 1.18 2016/01/15 23:05:00 krw Exp $	*/

//
// dump.c - dumping partition maps
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

#include <sys/param.h>	/* DEV_BSIZE */

#include <err.h>

// for *printf()
#include <stdio.h>

// for malloc() & free()
#include <stdlib.h>

// for strcmp()
#include <string.h>
// for O_RDONLY
#include <fcntl.h>
// for errno
#include <errno.h>

#include "dump.h"
#include "io.h"


//
// Defines
//
#if DPISTRLEN != 32
#error Change in strlen in partition entries! Fix constants
#endif

#define get_align_long(x)	(*(x))


//
// Types
//
typedef struct names {
    const char *abbr;
    const char *full;
} NAMES;

typedef unsigned long OSType;

typedef struct PatchDescriptor {
    OSType		patchSig;
    unsigned short	majorVers;
    unsigned short	minorVers;
    unsigned long	flags;
    unsigned long	patchOffset;
    unsigned long	patchSize;
    unsigned long	patchCRC;
    unsigned long	patchDescriptorLen;
    unsigned char	patchName[33];
    unsigned char	patchVendor[1];
} PatchDescriptor;
typedef PatchDescriptor * PatchDescriptorPtr;

typedef struct PatchList {
    unsigned short numPatchBlocks;	// number of disk blocks to hold the patch list
    unsigned short numPatches;		// number of patches in list
    PatchDescriptor thePatch[1];
} PatchList;
typedef PatchList *PatchListPtr;


//
// Global Constants
//
NAMES plist[] = {
    {"Drvr", "Apple_Driver"},
    {"Drv4", "Apple_Driver43"},
    {"Free", "Apple_Free"},
    {"Patc", "Apple_Patches"},
    {" HFS", "Apple_HFS"},
    {" MFS", "Apple_MFS"},
    {"PDOS", "Apple_PRODOS"},
    {"junk", "Apple_Scratch"},
    {"unix", "Apple_UNIX_SVR2"},
    {" map", "Apple_partition_map"},
    {0,	0},
};

const char * kStringEmpty	= "";
const char * kStringNot		= " not";


//
// Global Variables
//


//
// Forward declarations
//
void adjust_value_and_compute_prefix(double *value, int *prefix);
void dump_block_zero(partition_map_header *map);
void dump_partition_entry(partition_map *entry, int type_length, int name_length, int digits);
int get_max_base_or_length(partition_map_header *map);
int get_max_name_string_length(partition_map_header *map);
int get_max_type_string_length(partition_map_header *map);


//
// Routines
//
int
dump(char *name)
{
    partition_map_header *map;
    int junk;

    map = open_partition_map(name, &junk);
    if (map == NULL) {
	return 0;
    }

    dump_partition_map(map, 1);

    close_partition_map(map);

    return 1;
}


void
dump_block_zero(partition_map_header *map)
{
    Block0 *p;
    DDMap *m;
    int i;
    double value;
    int prefix;
    long t;

    p = map->misc;
    if (p->sbSig != BLOCK0_SIGNATURE) {
	return;
    }

    value = ((double)p->sbBlkCount) * p->sbBlkSize;
    adjust_value_and_compute_prefix(&value, &prefix);
    printf("\nDevice block size=%u, Number of Blocks=%lu (%1.1f%c)\n",
	    p->sbBlkSize, p->sbBlkCount, value, prefix);

    printf("DeviceType=0x%x, DeviceId=0x%x\n",
	    p->sbDevType, p->sbDevId);
    if (p->sbDrvrCount > 0) {
	printf("Drivers-\n");
	m = (DDMap *) p->sbMap;
	for (i = 0; i < p->sbDrvrCount; i++) {
	    printf("%u: %3u @ %lu, ", i+1,
		    m[i].ddSize, get_align_long(&m[i].ddBlock));
	    if (map->logical_block != p->sbBlkSize) {
		t = (m[i].ddSize * p->sbBlkSize) / map->logical_block;
		printf("(%lu@", t);
		t = (get_align_long(&m[i].ddBlock) * p->sbBlkSize)
			/ map->logical_block;
		printf("%lu)  ", t);
	    }
	    printf("type=0x%x\n", m[i].ddType);
	}
    }
    printf("\n");
}


void
dump_partition_map(partition_map_header *map, int disk_order)
{
    partition_map * entry;
    int max_type_length;
    int max_name_length;
    int digits;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    printf("\nPartition map (with %d byte blocks) on '%s'\n",
	map->logical_block, map->name);

    digits = number_of_digits(get_max_base_or_length(map));
    if (digits < 6) {
	digits = 6;
    }
    max_type_length = get_max_type_string_length(map);
    if (max_type_length < 4) {
        max_type_length = 4;
    }
    max_name_length = get_max_name_string_length(map);
    if (max_name_length < 6) {
	max_name_length = 6;
    }
    printf(" #: %*s %-*s %*s   %-*s ( size )\n",
	    max_type_length, "type",
	    max_name_length, "name",
	    digits, "length", digits, "base");

    if (disk_order) {
	for (entry = map->disk_order; entry != NULL;
		entry = entry->next_on_disk) {

	    dump_partition_entry(entry, max_type_length, max_name_length, digits);
	}
    } else {
	for (entry = map->base_order; entry != NULL;
		entry = entry->next_by_base) {

	    dump_partition_entry(entry, max_type_length, max_name_length, digits);
	}
    }
    dump_block_zero(map);
}


void
dump_partition_entry(partition_map *entry, int type_length, int name_length, int digits)
{
    partition_map_header *map;
    int j;
    DPME *p;
    const char *s;
    u32 size;
    double bytes;
    int driver, slice;
    // int kind;
    char *buf;
#if 1
    BZB *bp;
#endif

    map = entry->the_map;
    p = entry->data;
    driver = entry->contains_driver? '*': ' ';
    printf("%2ld: %*.32s", entry->disk_address, type_length, p->dpme_type);

    buf = malloc(name_length+1);
    strncpy(buf, p->dpme_name, name_length);
    buf[name_length] = 0;
    printf("%c%-*.32s ", driver, name_length, buf);
    free(buf);
    /*
    switch (entry->HFS_kind) {
    case kHFS_std:	kind = 'h'; break;
    case kHFS_embed:	kind = 'e'; break;
    case kHFS_plus:	kind = '+'; break;
    default:
    case kHFS_not:	kind = ' '; break;
    }
    printf("%c ", kind);
    */

    if (p->dpme_lblocks + p->dpme_lblock_start != p->dpme_pblocks) {
	printf("%*lu+", digits, p->dpme_lblocks);
	size = p->dpme_lblocks;
    } else if (p->dpme_lblock_start != 0) {
	printf("%*lu ", digits, p->dpme_lblocks);
	size = p->dpme_lblocks;
    } else {
	printf("%*lu ", digits, p->dpme_pblocks);
	size = p->dpme_pblocks;
    }
    if (p->dpme_lblock_start == 0) {
	printf("@ %-*lu", digits, p->dpme_pblock_start);
    } else {
	printf("@~%-*lu", digits, p->dpme_pblock_start + p->dpme_lblock_start);
    }

    bytes = ((double)size) * map->logical_block;
    adjust_value_and_compute_prefix(&bytes, &j);
    if (j != ' ' && j != 'K') {
	printf(" (%#5.1f%c)", bytes, j);
    }

#if 1
    // Old A/UX fields that no one pays attention to anymore.
    bp = (BZB *) (p->dpme_bzb);
    j = -1;
    if (bp->bzb_magic == BZBMAGIC) {
	switch (bp->bzb_type) {
	case FSTEFS:
	    s = "EFS";
	    break;
	case FSTSFS:
	    s = "SFS";
	    j = 1;
	    break;
	case FST:
	default:
	    if ((bp->bzb_flags & BZB_ROOT) != 0) {
		if ((bp->bzb_flags & BZB_USR) != 0) {
		    s = "RUFS";
		} else {
		    s = "RFS";
		}
		j = 0;
	    } else if ((bp->bzb_flags & BZB_USR) != 0) {
		s = "UFS";
		j = 2;
	    } else {
		s = "FS";
	    }
	    break;
	}
	slice = ((bp->bzb_flags >> BZB_SLICE_SHIFT) & BZB_SLICE_MASK);
	if (slice != 0) {
	    printf(" s%1d %4s", slice - 1, s);
	} else if (j >= 0) {
	    printf(" S%1d %4s", j, s);
	} else {
	    printf("    %4s", s);
	}
	if ((bp->bzb_flags & BZB_CRIT) != 0) {
	    printf(" K%1d", bp->bzb_cluster);
	} else if (j < 0) {
	    printf("   ");
	} else {
	    printf(" k%1d", bp->bzb_cluster);
	}
	if (bp->bzb_mount_point[0] != 0) {
	    printf("  %.64s", bp->bzb_mount_point);
	}
    }
#endif
    printf("\n");
}


void
show_data_structures(partition_map_header *map)
{
    Block0 *zp;
    DDMap *m;
    int i;
    int j, slice;
    partition_map * entry;
    DPME *p;
    BZB *bp;
    const char *s;

    if (map == NULL) {
	printf("No partition map exists\n");
	return;
    }
    printf("Header:\n");
    printf("map %d blocks out of %d,  media %lu blocks (%d byte blocks)\n",
	    map->blocks_in_map, map->maximum_in_map,
	    map->media_size, map->logical_block);
    printf("Map is%s writable", (map->writable)?kStringEmpty:kStringNot);
    printf(", but%s changed", (map->changed)?kStringEmpty:kStringNot);
    printf(" and has%s been written\n", (map->written)?kStringEmpty:kStringNot);
    printf("\n");

    if (map->misc == NULL) {
	printf("No block zero\n");
    } else {
	zp = map->misc;

	printf("Block0:\n");
	printf("signature 0x%x", zp->sbSig);
	if (zp->sbSig == BLOCK0_SIGNATURE) {
	    printf("\n");
	} else {
	    printf(" should be 0x%x\n", BLOCK0_SIGNATURE);
	}
	printf("Block size=%u, Number of Blocks=%lu\n",
		zp->sbBlkSize, zp->sbBlkCount);
	printf("DeviceType=0x%x, DeviceId=0x%x, sbData=0x%lx\n",
		zp->sbDevType, zp->sbDevId, zp->sbData);
	if (zp->sbDrvrCount == 0) {
	    printf("No drivers\n");
	} else {
	    printf("%u driver%s-\n", zp->sbDrvrCount,
		    (zp->sbDrvrCount>1)?"s":kStringEmpty);
	    m = (DDMap *) zp->sbMap;
	    for (i = 0; i < zp->sbDrvrCount; i++) {
            printf("%u: @ %lu for %u, type=0x%x\n", i+1,
		   get_align_long(&m[i].ddBlock),
		   m[i].ddSize, m[i].ddType);
	    }
	}
    }
    printf("\n");

/*
u32     dpme_boot_args[32]      ;
u32     dpme_reserved_3[62]     ;
*/
    printf(" #:                 type  length   base    "
	    "flags        (logical)\n");
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	p = entry->data;
	printf("%2ld: %20.32s ",
		entry->disk_address, p->dpme_type);
	printf("%7lu @ %-7lu ", p->dpme_pblocks, p->dpme_pblock_start);
	printf("%c%c%c%c%c%c%c%c%c%c%c%c ",
		(p->dpme_flags & DPME_VALID)?'V':'.',
		(p->dpme_flags & DPME_ALLOCATED)?'A':'.',
		(p->dpme_flags & DPME_IN_USE)?'I':'.',
		(p->dpme_flags & DPME_BOOTABLE)?'B':'.',
		(p->dpme_flags & DPME_READABLE)?'R':'.',
		(p->dpme_flags & DPME_WRITABLE)?'W':'.',
		(p->dpme_flags & DPME_OS_PIC_CODE)?'P':'.',
		(p->dpme_flags & DPME_OS_SPECIFIC_2)?'2':'.',
		(p->dpme_flags & DPME_CHAINABLE)?'C':'.',
		(p->dpme_flags & DPME_DISKDRIVER)?'D':'.',
		(p->dpme_flags & (1<<30))?'M':'.',
		(p->dpme_flags & (1<<31))?'X':'.');
	if (p->dpme_lblock_start != 0 || p->dpme_pblocks != p->dpme_lblocks) {
	    printf("(%lu @ %lu)", p->dpme_lblocks, p->dpme_lblock_start);
	}
	printf("\n");
    }
    printf("\n");
    printf(" #:  booter   bytes      load_address      "
	    "goto_address checksum processor\n");
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	p = entry->data;
	printf("%2ld: ", entry->disk_address);
	printf("%7lu ", p->dpme_boot_block);
	printf("%7lu ", p->dpme_boot_bytes);
	printf("%8lx ", (u32)p->dpme_load_addr);
	printf("%8lx ", (u32)p->dpme_load_addr_2);
	printf("%8lx ", (u32)p->dpme_goto_addr);
	printf("%8lx ", (u32)p->dpme_goto_addr_2);
	printf("%8lx ", p->dpme_checksum);
	printf("%.32s", p->dpme_process_id);
	printf("\n");
    }
    printf("\n");
/*
xx: cccc RU *dd s...
*/
    printf(" #: type RU *slice mount_point (A/UX only fields)\n");
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	p = entry->data;
	printf("%2ld: ", entry->disk_address);

	bp = (BZB *) (p->dpme_bzb);
	j = -1;
	if (bp->bzb_magic == BZBMAGIC) {
	    switch (bp->bzb_type) {
	    case FSTEFS:
		s = "esch";
		break;
	    case FSTSFS:
		s = "swap";
		j = 1;
		break;
	    case FST:
	    default:
		s = "fsys";
		if ((bp->bzb_flags & BZB_ROOT) != 0) {
		    j = 0;
		} else if ((bp->bzb_flags & BZB_USR) != 0) {
		    j = 2;
		}
		break;
	    }
	    printf("%4s ", s);
	    printf("%c%c ",
		    (bp->bzb_flags & BZB_ROOT)?'R':' ',
		    (bp->bzb_flags & BZB_USR)?'U':' ');
	    slice = ((bp->bzb_flags >> BZB_SLICE_SHIFT) & BZB_SLICE_MASK);
	    if (slice != 0) {
		printf("  %2d", slice);
	    } else if (j >= 0) {
		printf(" *%2d", j);
	    } else {
		printf("    ");
	    }
	    if (bp->bzb_mount_point[0] != 0) {
		printf(" %.64s", bp->bzb_mount_point);
	    }
	}
	printf("\n");
    }
}


void
full_dump_partition_entry(partition_map_header *map, int ix)
{
    partition_map * cur;
    DPME *p;
    int i;
    u32 t;

    cur = find_entry_by_disk_address(ix, map);
    if (cur == NULL) {
	printf("No such partition\n");
	return;
    }

    p = cur->data;
    printf("             signature: 0x%x\n", p->dpme_signature);
    printf("             reserved1: 0x%x\n", p->dpme_reserved_1);
    printf(" number of map entries: %ld\n", p->dpme_map_entries);
    printf("        physical start: %10lu  length: %10lu\n", p->dpme_pblock_start, p->dpme_pblocks);
    printf("         logical start: %10lu  length: %10lu\n", p->dpme_lblock_start, p->dpme_lblocks);

    printf("                 flags: 0x%lx\n", (u32)p->dpme_flags);
    printf("                        ");
    if (p->dpme_flags & DPME_VALID) printf("valid ");
    if (p->dpme_flags & DPME_ALLOCATED) printf("alloc ");
    if (p->dpme_flags & DPME_IN_USE) printf("in-use ");
    if (p->dpme_flags & DPME_BOOTABLE) printf("boot ");
    if (p->dpme_flags & DPME_READABLE) printf("read ");
    if (p->dpme_flags & DPME_WRITABLE) printf("write ");
    if (p->dpme_flags & DPME_OS_PIC_CODE) printf("pic ");
    t = p->dpme_flags >> 7;
    for (i = 7; i <= 31; i++) {
    	if (t & 0x1) {
    	    printf("%d ", i);
    	}
    	t = t >> 1;
    }
    printf("\n");

    printf("                  name: '%.32s'\n", p->dpme_name);
    printf("                  type: '%.32s'\n", p->dpme_type);

    printf("      boot start block: %10lu\n", p->dpme_boot_block);
    printf("boot length (in bytes): %10lu\n", p->dpme_boot_bytes);
    printf("          load address: 0x%08lx  0x%08lx\n",
		(u32)p->dpme_load_addr, (u32)p->dpme_load_addr_2);
    printf("         start address: 0x%08lx  0x%08lx\n",
		(u32)p->dpme_goto_addr, (u32)p->dpme_goto_addr_2);
    printf("              checksum: 0x%08lx\n", p->dpme_checksum);
    printf("             processor: '%.32s'\n", p->dpme_process_id);
    printf("boot args field -");
    dump_block((unsigned char *)p->dpme_boot_args, 32*4);
    printf("dpme_reserved_3 -");
    dump_block((unsigned char *)p->dpme_reserved_3, 62*4);
}


void
dump_block(unsigned char *addr, int len)
{
    int i;
    int j;
    int limit1;
    int limit;
#define LINE_LEN 16
#define UNIT_LEN  4
#define OTHER_LEN  8

    for (i = 0; i < len; i = limit) {
    	limit1 = i + LINE_LEN;
    	if (limit1 > len) {
    	    limit = len;
    	} else {
    	    limit = limit1;
    	}
	printf("\n%03x: ", i);
    	for (j = i; j < limit1; j++) {
	    if (j % UNIT_LEN == 0) {
		printf(" ");
	    }
	    if (j < limit) {
		printf("%02x", addr[j]);
	    } else {
		printf("  ");
	    }
    	}
	printf(" ");
    	for (j = i; j < limit; j++) {
	    if (j % OTHER_LEN == 0) {
		printf(" ");
	    }
    	    if (addr[j] < ' ') {
    	    	printf(".");
    	    } else {
    	    	printf("%c", addr[j]);
    	    }
    	}
    }
    printf("\n");
}

void
full_dump_block_zero(partition_map_header *map)
{
    Block0 *zp;
    DDMap *m;
    int i;

    if (map == NULL) {
	printf("No partition map exists\n");
	return;
    }

    if (map->misc == NULL) {
	printf("No block zero\n");
	return;
    }
    zp = map->misc;

    printf("             signature: 0x%x\n", zp->sbSig);
    printf("       size of a block: %d\n", zp->sbBlkSize);
    printf("      number of blocks: %ld\n", zp->sbBlkCount);
    printf("           device type: 0x%x\n", zp->sbDevType);
    printf("             device id: 0x%x\n", zp->sbDevId);
    printf("                  data: 0x%lx\n", zp->sbData);
    printf("          driver count: %d\n", zp->sbDrvrCount);
    m = (DDMap *) zp->sbMap;
    for (i = 0; &m[i].ddType < &zp->sbMap[247]; i++) {
    	if (m[i].ddBlock == 0 && m[i].ddSize == 0 && m[i].ddType == 0) {
    	    break;
    	}
	printf("      driver %3u block: %ld\n", i+1, m[i].ddBlock);
	printf("        size in blocks: %d\n", m[i].ddSize);
	printf("           driver type: 0x%x\n", m[i].ddType);
    }
    printf("remainder of block -");
    dump_block((unsigned char *)&m[i].ddBlock, (&zp->sbMap[247]-((unsigned short *)&m[i].ddBlock))*2);
}


void
display_patches(partition_map *entry)
{
    long long offset;
    MEDIA m;
    static unsigned char *patch_block;
    PatchListPtr p;
    PatchDescriptorPtr q;
    unsigned char *next;
    unsigned char *s;
    int i;

    offset = entry->data->dpme_pblock_start;
    m = entry->the_map->m;
    offset = ((long long) entry->data->dpme_pblock_start) * entry->the_map->logical_block;
    if (patch_block == NULL) {
	patch_block = malloc(DEV_BSIZE);
	if (patch_block == NULL) {
	    warn("can't allocate memory for patch block buffer");
	    return;
	}
    }
    if (read_media(m, (long long)offset, DEV_BSIZE, (char *)patch_block) == 0) {
	warn("Can't read patch block");
	return;
    }
    p = (PatchListPtr) patch_block;
    if (p->numPatchBlocks != 1) {
	i = p->numPatchBlocks;
	free(patch_block);
	patch_block = reallocarray(NULL, i, DEV_BSIZE);
	if (patch_block == NULL) {
	    warn("can't allocate memory for patch blocks buffer");
	    return;
	}
	s = patch_block + DEV_BSIZE*i;
	while (i > 0) {
	    s -= DEV_BSIZE;
	    i -= 1;
	    if (read_media(m, offset+i, DEV_BSIZE, (char *)s) == 0) {
		warn("Can't read patch block %d", i);
		return;
	    }
	}
	p = (PatchListPtr) patch_block;
    }
    printf("Patch list (%d entries)\n", p->numPatches);
    q = p->thePatch;
    for (i = 0; i < p->numPatches; i++) {
	printf("%2d signature: '%.4s'\n", i+1, (char *)&q->patchSig);
	printf("     version: %d.%d\n", q->majorVers, q->minorVers);
	printf("       flags: 0x%lx\n", q->flags);
	printf("      offset: %ld\n", q->patchOffset);
	printf("        size: %ld\n", q->patchSize);
	printf("         CRC: 0x%lx\n", q->patchCRC);
	printf("        name: '%.*s'\n", q->patchName[0], &q->patchName[1]);
	printf("      vendor: '%.*s'\n", q->patchVendor[0], &q->patchVendor[1]);
	next = ((unsigned char *)q) + q->patchDescriptorLen;
	s = &q->patchVendor[q->patchVendor[0]+1];
	if (next > s) {
	    printf("remainder of entry -");
	    dump_block(s, next-s);
	}
	q = (PatchDescriptorPtr)next;
    }
}

int
get_max_type_string_length(partition_map_header *map)
{
    partition_map * entry;
    int max;
    int length;

    if (map == NULL) {
	return 0;
    }

    max = 0;

    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	length = strnlen(entry->data->dpme_type, DPISTRLEN);
	if (length > max) {
	    max = length;
	}
    }

    return max;
}

int
get_max_name_string_length(partition_map_header *map)
{
    partition_map * entry;
    int max;
    int length;

    if (map == NULL) {
	return 0;
    }

    max = 0;

    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	length = strnlen(entry->data->dpme_name, DPISTRLEN);
	if (length > max) {
	    max = length;
	}
    }

    return max;
}

int
get_max_base_or_length(partition_map_header *map)
{
    partition_map * entry;
    int max;

    if (map == NULL) {
	return 0;
    }

    max = 0;

    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	if (entry->data->dpme_pblock_start > max) {
	    max = entry->data->dpme_pblock_start;
	}
	if (entry->data->dpme_pblocks > max) {
	    max = entry->data->dpme_pblocks;
	}
	if (entry->data->dpme_lblock_start > max) {
	    max = entry->data->dpme_lblock_start;
	}
	if (entry->data->dpme_lblocks > max) {
	    max = entry->data->dpme_lblocks;
	}
    }

    return max;
}

void
adjust_value_and_compute_prefix(double *value, int *prefix)
{
    double bytes;
    int multiplier;

    bytes = *value;
    if (bytes < 1024.0) {
	multiplier = ' ';
    } else {
	bytes = bytes / 1024.0;
	if (bytes < 1024.0) {
	    multiplier = 'K';
	} else {
	    bytes = bytes / 1024.0;
	    if (bytes < 1024.0) {
		multiplier = 'M';
	    } else {
		bytes = bytes / 1024.0;
		if (bytes < 1024.0) {
		    multiplier = 'G';
		} else {
		    bytes = bytes / 1024.0;
		    multiplier = 'T';
		}
	    }
	}
    }
    *value = bytes;
    *prefix = multiplier;
}
