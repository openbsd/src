/*	$OpenBSD: dump.c,v 1.67 2016/01/29 22:51:43 krw Exp $	*/

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

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dpme.h"
#include "partition_map.h"
#include "dump.h"
#include "io.h"

void	adjust_value_and_compute_prefix(double *, int *);
void	dump_block_zero(struct partition_map *);
void	dump_partition_entry(struct entry *, int, int, int);
int	get_max_base_or_length(struct partition_map *);
int	get_max_name_string_length(struct partition_map *);
int	get_max_type_string_length(struct partition_map *);

void
dump_block_zero(struct partition_map *map)
{
	struct ddmap  *m;
	double value;
	int i, prefix;

	value = ((double)map->sbBlkCount) * map->sbBlkSize;
	adjust_value_and_compute_prefix(&value, &prefix);
	printf("\nDevice block size=%u, Number of Blocks=%u (%1.1f%c)\n",
	       map->sbBlkSize, map->sbBlkCount, value, prefix);

	printf("DeviceType=0x%x, DeviceId=0x%x\n", map->sbDevType,
	    map->sbDevId);
	if (map->sbDrvrCount > 0) {
		printf("Drivers-\n");
		m = map->sbDDMap;
		for (i = 0; i < map->sbDrvrCount; i++) {
			printf("%d: %3u @ %u, ", i + 1, m[i].ddSize,
			    m[i].ddBlock);
			printf("type=0x%x\n", m[i].ddType);
		}
	}
	printf("\n");
}


void
dump_partition_map(struct partition_map *map)
{
	struct entry *entry;
	int digits, max_type_length, max_name_length;

	printf("\nPartition map (with %d byte blocks) on '%s'\n",
	       map->physical_block, map->name);

	digits = number_of_digits(get_max_base_or_length(map));
	if (digits < 6)
		digits = 6;
	max_type_length = get_max_type_string_length(map);
	if (max_type_length < 4)
		max_type_length = 4;
	max_name_length = get_max_name_string_length(map);
	if (max_name_length < 6)
		max_name_length = 6;
	printf(" #: %*s %-*s %*s   %-*s ( size )\n", max_type_length, "type",
	    max_name_length, "name", digits, "length", digits, "base");

	LIST_FOREACH(entry, &map->disk_order, disk_entry) {
		dump_partition_entry(entry, max_type_length,
		    max_name_length, digits);
	}
	dump_block_zero(map);
}


void
dump_partition_entry(struct entry *entry, int type_length,
    int name_length, int digits)
{
	struct partition_map *map;
	struct dpme *p;
	double bytes;
	int j, driver;

	map = entry->the_map;
	p = entry->dpme;
	driver = entry->contains_driver ? '*' : ' ';
	printf("%2ld: %*.32s", entry->disk_address, type_length, p->dpme_type);
	printf("%c%-*.32s ", driver, name_length, p->dpme_name);

	printf("%*u @ %-*u", digits, p->dpme_pblocks, digits,
	    p->dpme_pblock_start);

	bytes = ((double) p->dpme_pblocks) * map->physical_block;
	adjust_value_and_compute_prefix(&bytes, &j);
	if (j != ' ' && j != 'K')
		printf(" (%#5.1f%c)", bytes, j);
	printf("\n");
}


void
show_data_structures(struct partition_map *map)
{
	struct entry *entry;
	struct ddmap *m;
	struct dpme *p;
	int i;

	printf("Header:\n");
	printf("map %d blocks out of %d,  media %lu blocks (%d byte blocks)\n",
	    map->blocks_in_map, map->maximum_in_map, map->media_size,
	    map->physical_block);
	printf("Map is%s writable", rflag ? " not" : "");
	printf(" and has%s been changed\n", (map->changed) ? "" : " not");
	printf("\n");

	printf("Block0:\n");
	printf("signature 0x%x", map->sbSig);
	printf("Block size=%u, Number of Blocks=%u\n", map->sbBlkSize,
	    map->sbBlkCount);
	printf("DeviceType=0x%x, DeviceId=0x%x, sbData=0x%x\n", map->sbDevType,
	    map->sbDevId, map->sbData);
	if (map->sbDrvrCount == 0) {
		printf("No drivers\n");
	} else {
		printf("%u driver%s-\n", map->sbDrvrCount,
		    (map->sbDrvrCount > 1) ? "s" : "");
		m = map->sbDDMap;
		for (i = 0; i < map->sbDrvrCount; i++) {
			printf("%u: @ %u for %u, type=0x%x\n", i + 1,
			    m[i].ddBlock, m[i].ddSize, m[i].ddType);
		}
	}
	printf("\n");
	printf(" #:                 type  length   base    "
	       "flags     (      logical      )\n");
	LIST_FOREACH(entry, &map->disk_order, disk_entry) {
		p = entry->dpme;
		printf("%2ld: %20.32s ", entry->disk_address, p->dpme_type);
		printf("%7u @ %-7u ", p->dpme_pblocks, p->dpme_pblock_start);
		printf("%c%c%c%c%c%c%c%c%c ",
		       (p->dpme_flags & DPME_VALID) ? 'V' : '.',
		       (p->dpme_flags & DPME_ALLOCATED) ? 'A' : '.',
		       (p->dpme_flags & DPME_IN_USE) ? 'I' : '.',
		       (p->dpme_flags & DPME_BOOTABLE) ? 'B' : '.',
		       (p->dpme_flags & DPME_READABLE) ? 'R' : '.',
		       (p->dpme_flags & DPME_WRITABLE) ? 'W' : '.',
		       (p->dpme_flags & DPME_OS_PIC_CODE) ? 'P' : '.',
		       (p->dpme_flags & DPME_OS_SPECIFIC_2) ? '2' : '.',
		       (p->dpme_flags & DPME_OS_SPECIFIC_1) ? '1' : '.');
		printf("( %7u @ %-7u )\n", p->dpme_lblocks,
		    p->dpme_lblock_start);
	}
	printf("\n");
	printf(" #:  booter   bytes      load_address      "
	    "goto_address checksum processor\n");
	LIST_FOREACH(entry, &map->disk_order, disk_entry) {
		p = entry->dpme;
		printf("%2ld: ", entry->disk_address);
		printf("%7u ", p->dpme_boot_block);
		printf("%7u ", p->dpme_boot_bytes);
		printf("%8x ", p->dpme_load_addr);
		printf("%8x ", p->dpme_goto_addr);
		printf("%8x ", p->dpme_checksum);
		printf("%.32s", p->dpme_processor_id);
		printf("\n");
	}
	printf("\n");
}


void
full_dump_partition_entry(struct partition_map *map, int ix)
{
	struct entry *cur;
	struct dpme *p;
	int i;
	uint32_t t;

	cur = find_entry_by_disk_address(ix, map);
	if (cur == NULL) {
		printf("No such partition\n");
		return;
	}
	p = cur->dpme;
	printf("             signature: 0x%x\n", p->dpme_signature);
	printf(" number of map entries: %u\n", p->dpme_map_entries);
	printf("        physical start: %10u  length: %10u\n",
	    p->dpme_pblock_start, p->dpme_pblocks);
	printf("         logical start: %10u  length: %10u\n",
	    p->dpme_lblock_start, p->dpme_lblocks);

	printf("                 flags: 0x%x\n", (uint32_t)p->dpme_flags);
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
		if (t & 0x1)
			printf("%d ", i);
		t = t >> 1;
	}
	printf("\n");

	printf("                  name: '%.32s'\n", p->dpme_name);
	printf("                  type: '%.32s'\n", p->dpme_type);
	printf("      boot start block: %10u\n", p->dpme_boot_block);
	printf("boot length (in bytes): %10u\n", p->dpme_boot_bytes);
	printf("          load address: 0x%08x\n", p->dpme_load_addr);
	printf("         start address: 0x%08x\n", p->dpme_goto_addr);
	printf("              checksum: 0x%08x\n", p->dpme_checksum);
	printf("             processor: '%.32s'\n", p->dpme_processor_id);
	printf("dpme_reserved_1 -");
	dump_block(p->dpme_reserved_1, sizeof(p->dpme_reserved_1));
	printf("dpme_reserved_2 -");
	dump_block(p->dpme_reserved_2, sizeof(p->dpme_reserved_2));
	printf("dpme_reserved_3 -");
	dump_block(p->dpme_reserved_3, sizeof(p->dpme_reserved_3));
	printf("dpme_reserved_4 -");
	dump_block(p->dpme_reserved_4, sizeof(p->dpme_reserved_4));
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
		if (limit1 > len)
			limit = len;
		else
			limit = limit1;
		printf("\n%03x: ", i);
		for (j = i; j < limit1; j++) {
			if (j % UNIT_LEN == 0)
				printf(" ");
			if (j < limit)
				printf("%02x", addr[j]);
			else
				printf("  ");
		}
		printf(" ");
		for (j = i; j < limit; j++) {
			if (j % OTHER_LEN == 0)
				printf(" ");
			if (addr[j] < ' ')
				printf(".");
			else
				printf("%c", addr[j]);
		}
	}
	printf("\n");
}

void
full_dump_block_zero(struct partition_map *map)
{
	struct ddmap *m;
	int i;

	m = map->sbDDMap;

	printf("             signature: 0x%x\n", map->sbSig);
	printf("       size of a block: %u\n", map->sbBlkSize);
	printf("      number of blocks: %u\n", map->sbBlkCount);
	printf("           device type: 0x%x\n", map->sbDevType);
	printf("             device id: 0x%x\n", map->sbDevId);
	printf("                  data: 0x%x\n", map->sbData);
	printf("          driver count: %u\n", map->sbDrvrCount);
	for (i = 0; i < 8; i++) {
		if (m[i].ddBlock == 0 && m[i].ddSize == 0 && m[i].ddType == 0)
			break;
		printf("      driver %3u block: %u\n", i + 1, m[i].ddBlock);
		printf("        size in blocks: %u\n", m[i].ddSize);
		printf("           driver type: 0x%x\n", m[i].ddType);
	}
	printf("remainder of block -");
	dump_block(map->sbReserved, sizeof(map->sbReserved));
}

int
get_max_type_string_length(struct partition_map *map)
{
	struct entry *entry;
	int max, length;

	max = 0;

	LIST_FOREACH(entry, &map->disk_order, disk_entry) {
		length = strnlen(entry->dpme->dpme_type, DPISTRLEN);
		if (length > max)
			max = length;
	}

	return max;
}

int
get_max_name_string_length(struct partition_map *map)
{
	struct entry *entry;
	int max, length;

	max = 0;

	LIST_FOREACH(entry, &map->disk_order, disk_entry) {
		length = strnlen(entry->dpme->dpme_name, DPISTRLEN);
		if (length > max)
			max = length;
	}

	return max;
}

int
get_max_base_or_length(struct partition_map *map)
{
	struct entry *entry;
	int max;

	max = 0;

	LIST_FOREACH(entry, &map->disk_order, disk_entry) {
		if (entry->dpme->dpme_pblock_start > max)
			max = entry->dpme->dpme_pblock_start;
		if (entry->dpme->dpme_pblocks > max)
			max = entry->dpme->dpme_pblocks;
		if (entry->dpme->dpme_lblock_start > max)
			max = entry->dpme->dpme_lblock_start;
		if (entry->dpme->dpme_lblocks > max)
			max = entry->dpme->dpme_lblocks;
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
