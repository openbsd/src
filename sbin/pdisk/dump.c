/*	$OpenBSD: dump.c,v 1.48 2016/01/24 01:38:32 krw Exp $	*/

/*
 * dump.c - dumping partition maps
 *
 * Written by Eryk Vershen
 */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dpme.h"
#include "file_media.h"
#include "partition_map.h"
#include "dump.h"
#include "io.h"

#define get_align_long(x)	(*(x))

void	adjust_value_and_compute_prefix(double *, int *);
void	dump_block_zero(struct partition_map_header *);
void	dump_partition_entry(struct partition_map *, int, int, int);
int	get_max_base_or_length(struct partition_map_header *);
int	get_max_name_string_length(struct partition_map_header *);
int	get_max_type_string_length(struct partition_map_header *);

void
dump_block_zero(struct partition_map_header *map)
{
	struct block0 *p;
	struct ddmap  *m;
	double value;
	long t;
	int i, prefix;

	p = map->block0;
	if (p->sbSig != BLOCK0_SIGNATURE) {
		return;
	}
	value = ((double) p->sbBlkCount) * p->sbBlkSize;
	adjust_value_and_compute_prefix(&value, &prefix);
	printf("\nDevice block size=%u, Number of Blocks=%u (%1.1f%c)\n",
	       p->sbBlkSize, p->sbBlkCount, value, prefix);

	printf("DeviceType=0x%x, DeviceId=0x%x\n",
	       p->sbDevType, p->sbDevId);
	if (p->sbDrvrCount > 0) {
		printf("Drivers-\n");
		m = (struct ddmap *) p->sbMap;
		for (i = 0; i < p->sbDrvrCount; i++) {
			printf("%d: %3u @ %u, ", i + 1,
			       m[i].ddSize, get_align_long(&m[i].ddBlock));
			if (map->logical_block != p->sbBlkSize) {
				t = (m[i].ddSize * p->sbBlkSize) /
				    map->logical_block;
				printf("(%lu@", t);
				t = (get_align_long(&m[i].ddBlock) *
				    p->sbBlkSize) / map->logical_block;
				printf("%lu)  ", t);
			}
			printf("type=0x%x\n", m[i].ddType);
		}
	}
	printf("\n");
}


void
dump_partition_map(struct partition_map_header *map)
{
	struct partition_map *entry;
	int digits, max_type_length, max_name_length;

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

	for (entry = map->disk_order; entry != NULL;
	     entry = entry->next_on_disk) {
		dump_partition_entry(entry, max_type_length,
		    max_name_length, digits);
	}
	dump_block_zero(map);
}


void
dump_partition_entry(struct partition_map *entry, int type_length,
    int name_length, int digits)
{
	struct partition_map_header *map;
	struct dpme    *p;
	char           *buf;
	double bytes;
	int j, driver;
	uint32_t size;

	map = entry->the_map;
	p = entry->dpme;
	driver = entry->contains_driver ? '*' : ' ';
	printf("%2ld: %*.32s", entry->disk_address, type_length, p->dpme_type);

	buf = malloc(name_length + 1);
	strncpy(buf, p->dpme_name, name_length);
	buf[name_length] = 0;
	printf("%c%-*.32s ", driver, name_length, buf);
	free(buf);

	if (p->dpme_lblocks + p->dpme_lblock_start != p->dpme_pblocks) {
		printf("%*u+", digits, p->dpme_lblocks);
		size = p->dpme_lblocks;
	} else if (p->dpme_lblock_start != 0) {
		printf("%*u ", digits, p->dpme_lblocks);
		size = p->dpme_lblocks;
	} else {
		printf("%*u ", digits, p->dpme_pblocks);
		size = p->dpme_pblocks;
	}
	if (p->dpme_lblock_start == 0) {
		printf("@ %-*u", digits, p->dpme_pblock_start);
	} else {
		printf("@~%-*u", digits, p->dpme_pblock_start +
		    p->dpme_lblock_start);
	}

	bytes = ((double) size) * map->logical_block;
	adjust_value_and_compute_prefix(&bytes, &j);
	if (j != ' ' && j != 'K') {
		printf(" (%#5.1f%c)", bytes, j);
	}
	printf("\n");
}


void
show_data_structures(struct partition_map_header *map)
{
	struct partition_map *entry;
	struct block0 *zp;
	struct ddmap *m;
	struct dpme *p;
	int i;

	printf("Header:\n");
	printf("map %d blocks out of %d,  media %lu blocks (%d byte blocks)\n",
	       map->blocks_in_map, map->maximum_in_map,
	       map->media_size, map->logical_block);
	printf("Map is%s writable", rflag ? " not" : "");
	printf(" and has%s been changed\n", (map->changed) ? "" : " not");
	printf("\n");

	zp = map->block0;

	printf("Block0:\n");
	printf("signature 0x%x", zp->sbSig);
	if (zp->sbSig == BLOCK0_SIGNATURE) {
		printf("\n");
	} else {
		printf(" should be 0x%x\n", BLOCK0_SIGNATURE);
	}
	printf("Block size=%u, Number of Blocks=%u\n", zp->sbBlkSize,
	    zp->sbBlkCount);
	printf("DeviceType=0x%x, DeviceId=0x%x, sbData=0x%x\n", zp->sbDevType,
	    zp->sbDevId, zp->sbData);
	if (zp->sbDrvrCount == 0) {
		printf("No drivers\n");
	} else {
		printf("%u driver%s-\n", zp->sbDrvrCount,
		       (zp->sbDrvrCount > 1) ? "s" : "");
		m = (struct ddmap *) zp->sbMap;
		for (i = 0; i < zp->sbDrvrCount; i++) {
			printf("%u: @ %u for %u, type=0x%x\n", i + 1,
			    get_align_long(&m[i].ddBlock), m[i].ddSize,
			    m[i].ddType);
		}
	}
	printf("\n");
	printf(" #:                 type  length   base    "
	       "flags        (logical)\n");
	for (entry = map->disk_order; entry != NULL;
	    entry = entry->next_on_disk) {
		p = entry->dpme;
		printf("%2ld: %20.32s ",
		       entry->disk_address, p->dpme_type);
		printf("%7u @ %-7u ", p->dpme_pblocks, p->dpme_pblock_start);
		printf("%c%c%c%c%c%c%c%c%c%c%c%c ",
		       (p->dpme_flags & DPME_VALID) ? 'V' : '.',
		       (p->dpme_flags & DPME_ALLOCATED) ? 'A' : '.',
		       (p->dpme_flags & DPME_IN_USE) ? 'I' : '.',
		       (p->dpme_flags & DPME_BOOTABLE) ? 'B' : '.',
		       (p->dpme_flags & DPME_READABLE) ? 'R' : '.',
		       (p->dpme_flags & DPME_WRITABLE) ? 'W' : '.',
		       (p->dpme_flags & DPME_OS_PIC_CODE) ? 'P' : '.',
		       (p->dpme_flags & DPME_OS_SPECIFIC_2) ? '2' : '.',
		       (p->dpme_flags & DPME_CHAINABLE) ? 'C' : '.',
		       (p->dpme_flags & DPME_DISKDRIVER) ? 'D' : '.',
		       (p->dpme_flags & (1 << 30)) ? 'M' : '.',
		       (p->dpme_flags & (1 << 31)) ? 'X' : '.');
		if (p->dpme_lblock_start != 0 || p->dpme_pblocks !=
		    p->dpme_lblocks) {
			printf("(%u @ %u)", p->dpme_lblocks,
			    p->dpme_lblock_start);
		}
		printf("\n");
	}
	printf("\n");
	printf(" #:  booter   bytes      load_address      "
	       "goto_address checksum processor\n");
	for (entry = map->disk_order; entry != NULL;
	    entry = entry->next_on_disk) {
		p = entry->dpme;
		printf("%2ld: ", entry->disk_address);
		printf("%7u ", p->dpme_boot_block);
		printf("%7u ", p->dpme_boot_bytes);
		printf("%8x ", (uint32_t) p->dpme_load_addr);
		printf("%8x ", (uint32_t) p->dpme_load_addr_2);
		printf("%8x ", (uint32_t) p->dpme_goto_addr);
		printf("%8x ", (uint32_t) p->dpme_goto_addr_2);
		printf("%8x ", p->dpme_checksum);
		printf("%.32s", p->dpme_process_id);
		printf("\n");
	}
	printf("\n");
}


void
full_dump_partition_entry(struct partition_map_header *map, int ix)
{
	struct partition_map *cur;
	struct dpme    *p;
	int i;
	uint32_t t;

	cur = find_entry_by_disk_address(ix, map);
	if (cur == NULL) {
		printf("No such partition\n");
		return;
	}
	p = cur->dpme;
	printf("             signature: 0x%x\n", p->dpme_signature);
	printf("             reserved1: 0x%x\n", p->dpme_reserved_1);
	printf(" number of map entries: %u\n", p->dpme_map_entries);
	printf("        physical start: %10u  length: %10u\n",
	    p->dpme_pblock_start, p->dpme_pblocks);
	printf("         logical start: %10u  length: %10u\n",
	    p->dpme_lblock_start, p->dpme_lblocks);

	printf("                 flags: 0x%x\n", (uint32_t) p->dpme_flags);
	printf("                        ");
	if (p->dpme_flags & DPME_VALID)
		printf("valid ");
	if (p->dpme_flags & DPME_ALLOCATED)
		printf("alloc ");
	if (p->dpme_flags & DPME_IN_USE)
		printf("in-use ");
	if (p->dpme_flags & DPME_BOOTABLE)
		printf("boot ");
	if (p->dpme_flags & DPME_READABLE)
		printf("read ");
	if (p->dpme_flags & DPME_WRITABLE)
		printf("write ");
	if (p->dpme_flags & DPME_OS_PIC_CODE)
		printf("pic ");
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

	printf("      boot start block: %10u\n", p->dpme_boot_block);
	printf("boot length (in bytes): %10u\n", p->dpme_boot_bytes);
	printf("          load address: 0x%08x  0x%08x\n",
	       (uint32_t) p->dpme_load_addr, (uint32_t) p->dpme_load_addr_2);
	printf("         start address: 0x%08x  0x%08x\n",
	       (uint32_t) p->dpme_goto_addr, (uint32_t) p->dpme_goto_addr_2);
	printf("              checksum: 0x%08x\n", p->dpme_checksum);
	printf("             processor: '%.32s'\n", p->dpme_process_id);
	printf("boot args field -");
	dump_block((unsigned char *) p->dpme_boot_args, 32 * 4);
	printf("dpme_reserved_3 -");
	dump_block((unsigned char *) p->dpme_reserved_3, 62 * 4);
}


void
dump_block(unsigned char *addr, int len)
{
	int i, j, limit1, limit;

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
full_dump_block_zero(struct partition_map_header *map)
{
	struct block0 *zp;
	struct ddmap *m;
	int i;

	if (map->block0 == NULL) {
		printf("No block zero\n");
		return;
	}
	zp = map->block0;

	printf("             signature: 0x%x\n", zp->sbSig);
	printf("       size of a block: %u\n", zp->sbBlkSize);
	printf("      number of blocks: %u\n", zp->sbBlkCount);
	printf("           device type: 0x%x\n", zp->sbDevType);
	printf("             device id: 0x%x\n", zp->sbDevId);
	printf("                  data: 0x%x\n", zp->sbData);
	printf("          driver count: %u\n", zp->sbDrvrCount);
	m = (struct ddmap *) zp->sbMap;
	for (i = 0; &m[i].ddType < &zp->sbMap[247]; i++) {
		if (m[i].ddBlock == 0 && m[i].ddSize == 0 && m[i].ddType == 0) {
			break;
		}
		printf("      driver %3u block: %u\n", i + 1, m[i].ddBlock);
		printf("        size in blocks: %u\n", m[i].ddSize);
		printf("           driver type: 0x%x\n", m[i].ddType);
	}
	printf("remainder of block -");
	dump_block((unsigned char *) &m[i].ddBlock, (&zp->sbMap[247] -
	    ((unsigned short *) &m[i].ddBlock)) * 2);
}

int
get_max_type_string_length(struct partition_map_header *map)
{
	struct partition_map *entry;
	int max, length;

	max = 0;

	for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
		length = strnlen(entry->dpme->dpme_type, DPISTRLEN);
		if (length > max) {
			max = length;
		}
	}

	return max;
}

int
get_max_name_string_length(struct partition_map_header *map)
{
	struct partition_map *entry;
	int max, length;

	max = 0;

	for (entry = map->disk_order; entry != NULL; entry =
	    entry->next_on_disk) {
		length = strnlen(entry->dpme->dpme_name, DPISTRLEN);
		if (length > max) {
			max = length;
		}
	}

	return max;
}

int
get_max_base_or_length(struct partition_map_header *map)
{
	struct partition_map *entry;
	int max;

	max = 0;

	for (entry = map->disk_order; entry != NULL;
	    entry = entry->next_on_disk) {
		if (entry->dpme->dpme_pblock_start > max) {
			max = entry->dpme->dpme_pblock_start;
		}
		if (entry->dpme->dpme_pblocks > max) {
			max = entry->dpme->dpme_pblocks;
		}
		if (entry->dpme->dpme_lblock_start > max) {
			max = entry->dpme->dpme_lblock_start;
		}
		if (entry->dpme->dpme_lblocks > max) {
			max = entry->dpme->dpme_lblocks;
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
