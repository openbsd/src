/*	$OpenBSD: partition_map.c,v 1.38 2016/01/18 02:50:01 krw Exp $	*/

/*
 * partition_map.c - partition map routines
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

#include <sys/param.h>		/* DEV_BSIZE */
#include <sys/dkio.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <err.h>
#include <limits.h>
#include <unistd.h>
#include <util.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "partition_map.h"
#include "io.h"
#include "convert.h"
#include "file_media.h"

#define APPLE_HFS_FLAGS_VALUE	0x4000037f
#define get_align_long(x)	(*(x))
#define put_align_long(y, x)	((*(x)) = (y))

const char     *kFreeType = "Apple_Free";
const char     *kMapType = "Apple_partition_map";
const char     *kUnixType = "OpenBSD";
const char     *kHFSType = "Apple_HFS";
const char     *kPatchType = "Apple_Patches";

const char     *kFreeName = "Extra";

enum add_action {
	kReplace = 0,
	kAdd = 1,
	kSplit = 2
};

int		add_data_to_map(struct dpme *, long,
		    struct partition_map_header *);
int		coerce_block0(struct partition_map_header *);
int		contains_driver(struct partition_map *);
void		combine_entry(struct partition_map *);
long		compute_device_size(char *);
struct dpme    *create_data(const char *, const char *, uint32_t, uint32_t);
void		delete_entry(struct partition_map *);
void		insert_in_base_order(struct partition_map *);
void		insert_in_disk_order(struct partition_map *);
int		read_block(struct partition_map_header *, unsigned long,
		    char *);
int		read_partition_map(struct partition_map_header *);
void		remove_driver(struct partition_map *);
void		remove_from_disk_order(struct partition_map *);
void		renumber_disk_addresses(struct partition_map_header *);
void		sync_device_size(struct partition_map_header *);
int		write_block(struct partition_map_header *, unsigned long,
		    char *);

struct partition_map_header *
open_partition_map(char *name, int *valid_file)
{
	struct partition_map_header *map;
	struct file_media *m;
	int writable;

	m = open_file_as_media(name, (rflag) ? O_RDONLY : O_RDWR);
	if (m == 0) {
		m = open_file_as_media(name, O_RDONLY);
		if (m == 0) {
			warn("can't open file '%s'", name);
			*valid_file = 0;
			return NULL;
		} else {
			writable = 0;
		}
	} else {
		writable = 1;
	}
	*valid_file = 1;

	map = malloc(sizeof(struct partition_map_header));
	if (map == NULL) {
		warn("can't allocate memory for open partition map");
		close_file_media(m);
		return NULL;
	}
	map->name = name;
	map->writable = (rflag) ? 0 : writable;
	map->changed = 0;
	map->written = 0;
	map->disk_order = NULL;
	map->base_order = NULL;

	map->physical_block = DEV_BSIZE;	/* preflight */
	map->m = m;
	map->misc = malloc(DEV_BSIZE);
	if (map->misc == NULL) {
		warn("can't allocate memory for block zero buffer");
		close_file_media(map->m);
		free(map);
		return NULL;
	} else if (read_file_media(map->m, (long long) 0, DEV_BSIZE,
		   (char *) map->misc) == 0 ||
		   convert_block0(map->misc, 1) ||
		   coerce_block0(map)) {
		warnx("Can't read block 0 from '%s'", name);
		close_partition_map(map);
		return NULL;
	}
	map->physical_block = map->misc->sbBlkSize;

	map->logical_block = DEV_BSIZE;

	if (map->logical_block > MAXIOSIZE) {
		map->logical_block = MAXIOSIZE;
	}
	if (map->logical_block > map->physical_block) {
		map->physical_block = map->logical_block;
	}
	map->blocks_in_map = 0;
	map->maximum_in_map = -1;
	map->media_size = compute_device_size(map->name);
	sync_device_size(map);

	if (read_partition_map(map) < 0) {
		/* some sort of failure reading the map */
	} else {
		/* got it! */
		return map;
	}
	close_partition_map(map);
	return NULL;
}


void
close_partition_map(struct partition_map_header * map)
{
	struct partition_map *entry, *next;

	if (map == NULL) {
		return;
	}
	free(map->misc);

	for (entry = map->disk_order; entry != NULL; entry = next) {
		next = entry->next_on_disk;
		free(entry->data);
		free(entry);
	}
	close_file_media(map->m);
	free(map);
}


int
read_partition_map(struct partition_map_header * map)
{
	struct dpme    *data;
	double d;
	int ix, old_logical;
	uint32_t limit;

	data = malloc(DEV_BSIZE);
	if (data == NULL) {
		warn("can't allocate memory for disk buffers");
		return -1;
	}
	if (read_block(map, 1, (char *) data) == 0) {
		warnx("Can't read block 1 from '%s'", map->name);
		free(data);
		return -1;
	} else if (convert_dpme(data, 1) ||
		   data->dpme_signature != DPME_SIGNATURE) {
		old_logical = map->logical_block;
		map->logical_block = 512;
		while (map->logical_block <= map->physical_block) {
			if (read_block(map, 1, (char *) data) == 0) {
				warnx("Can't read block 1 from '%s'",
				    map->name);
				free(data);
				return -1;
			} else if (convert_dpme(data, 1) == 0
				&& data->dpme_signature == DPME_SIGNATURE) {
				d = map->media_size;
				map->media_size = (d * old_logical) /
				    map->logical_block;
				break;
			}
			map->logical_block *= 2;
		}
		if (map->logical_block > map->physical_block) {
			warnx("No valid block 1 on '%s'", map->name);
			free(data);
			return -1;
		}
	}
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

		data = malloc(DEV_BSIZE);
		if (data == NULL) {
			warn("can't allocate memory for disk buffers");
			return -1;
		}
		if (read_block(map, ix, (char *) data) == 0) {
			warnx("Can't read block %u from '%s'", ix, map->name);
			free(data);
			return -1;
		} else if (convert_dpme(data, 1) ||
			   (data->dpme_signature != DPME_SIGNATURE &&
			   dflag == 0) ||
			   (data->dpme_map_entries != limit && dflag == 0)) {
			warnx("Bad data in block %u from '%s'", ix, map->name);
			free(data);
			return -1;
		}
	}
	return 0;
}


void
write_partition_map(struct partition_map_header * map)
{
	struct partition_map *entry;
	struct file_media *m;
	char *block;
	int i = 0, result = 0;

	m = map->m;
	if (map->misc != NULL) {
		convert_block0(map->misc, 0);
		result = write_block(map, 0, (char *) map->misc);
		convert_block0(map->misc, 1);
	} else {
		block = calloc(1, DEV_BSIZE);
		if (block != NULL) {
			result = write_block(map, 0, block);
			free(block);
		}
	}
	if (result == 0) {
		warn("Unable to write block zero");
	}
	for (entry = map->disk_order; entry != NULL;
	    entry = entry->next_on_disk) {
		convert_dpme(entry->data, 0);
		result = write_block(map, entry->disk_address,
		    (char *)entry->data);
		convert_dpme(entry->data, 1);
		i = entry->disk_address;
		if (result == 0) {
			warn("Unable to write block %d", i);
		}
	}

	os_reload_file_media(map->m);
}


int
add_data_to_map(struct dpme * data, long ix, struct partition_map_header * map)
{
	struct partition_map *entry;

	entry = malloc(sizeof(struct partition_map));
	if (entry == NULL) {
		warn("can't allocate memory for map entries");
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

	insert_in_disk_order(entry);
	insert_in_base_order(entry);

	map->blocks_in_map++;
	if (map->maximum_in_map < 0) {
		if (strncasecmp(data->dpme_type, kMapType, DPISTRLEN) == 0) {
			map->maximum_in_map = data->dpme_pblocks;
		}
	}
	return 1;
}


struct partition_map_header *
init_partition_map(char *name, struct partition_map_header * oldmap)
{
	struct partition_map_header *map;

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
			     1, (map->media_size <= 128 ? 2 : 63), map);
	return map;
}


struct partition_map_header *
create_partition_map(char *name, struct partition_map_header * oldmap)
{
	struct partition_map_header *map;
	struct file_media *m;
	struct dpme *data;
	unsigned long number;
	long size;

	m = open_file_as_media(name, (rflag) ? O_RDONLY : O_RDWR);
	if (m == 0) {
		warn("can't open file '%s' for %sing", name, (rflag) ?
		    "read" : "writ");
		return NULL;
	}
	map = malloc(sizeof(struct partition_map_header));
	if (map == NULL) {
		warn("can't allocate memory for open partition map");
		close_file_media(m);
		return NULL;
	}
	map->name = name;
	map->writable = (rflag) ? 0 : 1;
	map->changed = 1;
	map->disk_order = NULL;
	map->base_order = NULL;

	if (oldmap != NULL) {
		size = oldmap->physical_block;
	} else {
		size = DEV_BSIZE;
	}
	map->m = m;
	if (map->physical_block > MAXIOSIZE) {
		map->physical_block = MAXIOSIZE;
	}
	map->physical_block = size;

	if (oldmap != NULL) {
		size = oldmap->logical_block;
	} else {
		size = DEV_BSIZE;
	}

	map->logical_block = size;

	map->blocks_in_map = 0;
	map->maximum_in_map = -1;

	number = compute_device_size(map->name);
	map->media_size = number;

	map->misc = calloc(1, DEV_BSIZE);
	if (map->misc == NULL) {
		warn("can't allocate memory for block zero buffer");
	} else {
		coerce_block0(map);
		sync_device_size(map);

		data = calloc(1, DEV_BSIZE);
		if (data == NULL) {
			warn("can't allocate memory for disk buffers");
		} else {
			data->dpme_signature = DPME_SIGNATURE;
			data->dpme_map_entries = 1;
			data->dpme_pblock_start = 1;
			data->dpme_pblocks = map->media_size - 1;
			strncpy(data->dpme_name, kFreeName, DPISTRLEN);
			strncpy(data->dpme_type, kFreeType, DPISTRLEN);
			data->dpme_lblock_start = 0;
			data->dpme_lblocks = data->dpme_pblocks;
			data->dpme_flags = DPME_WRITABLE | DPME_READABLE |
			    DPME_VALID;

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
coerce_block0(struct partition_map_header * map)
{
	struct block0 *p;

	p = map->misc;
	if (p == NULL) {
		return 1;
	}
	if (p->sbSig != BLOCK0_SIGNATURE) {
		p->sbSig = BLOCK0_SIGNATURE;
		if (map->physical_block == 1) {
			p->sbBlkSize = DEV_BSIZE;
		} else {
			p->sbBlkSize = map->physical_block;
		}
		p->sbBlkCount = 0;
		p->sbDevType = 0;
		p->sbDevId = 0;
		p->sbData = 0;
		p->sbDrvrCount = 0;
	}
	return 0;
}


int
add_partition_to_map(const char *name, const char *dptype, uint32_t base, uint32_t length,
		     struct partition_map_header * map)
{
	struct partition_map *cur;
	struct dpme *data;
	enum add_action act;
	int limit;
	uint32_t adjusted_base = 0;
	uint32_t adjusted_length = 0;
	uint32_t new_base = 0;
	uint32_t new_length = 0;

	/* find a block that starts includes base and length */
	cur = map->base_order;
	while (cur != NULL) {
		if (cur->data->dpme_pblock_start <= base &&
		    (base + length) <=
		    (cur->data->dpme_pblock_start + cur->data->dpme_pblocks)) {
			break;
		} else {
			/*
			 * check if request is past end of existing
			 * partitions, but on disk
			 */
			if ((cur->next_by_base == NULL) &&
			    (base + length <= map->media_size)) {
				/* Expand final free partition */
				if ((strncasecmp(cur->data->dpme_type,
				    kFreeType, DPISTRLEN) == 0) &&
				    base >= cur->data->dpme_pblock_start) {
					cur->data->dpme_pblocks =
						map->media_size -
						cur->data->dpme_pblock_start;
					break;
				}
				/* create an extra free partition */
				if (base >= cur->data->dpme_pblock_start +
				    cur->data->dpme_pblocks) {
					if (map->maximum_in_map < 0) {
						limit = map->media_size;
					} else {
						limit = map->maximum_in_map;
					}
					if (map->blocks_in_map + 1 > limit) {
						printf("the map is not big "
						    "enough\n");
						return 0;
					}
					data = create_data(kFreeName, kFreeType,
					    cur->data->dpme_pblock_start +
					    cur->data->dpme_pblocks,
					    map->media_size -
					    (cur->data->dpme_pblock_start +
					    cur->data->dpme_pblocks));
					if (data != NULL) {
						if (add_data_to_map(data,
						    cur->disk_address, map) ==
						    0) {
							free(data);
						}
					}
				}
			}
			cur = cur->next_by_base;
		}
	}
	/* if it is not Extra then punt */
	if (cur == NULL ||
	    strncasecmp(cur->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
		printf("requested base and length is not "
		       "within an existing free partition\n");
		return 0;
	}
	/* figure out what to do and sizes */
	data = cur->data;
	if (data->dpme_pblock_start == base) {
		/* replace or add */
		if (data->dpme_pblocks == length) {
			act = kReplace;
		} else {
			act = kAdd;
			adjusted_base = base + length;
			adjusted_length = data->dpme_pblocks - length;
		}
	} else {
		/* split or add */
		if (data->dpme_pblock_start + data->dpme_pblocks == base +
		    length) {
			act = kAdd;
			adjusted_base = data->dpme_pblock_start;
			adjusted_length = base - adjusted_base;
		} else {
			act = kSplit;
			new_base = data->dpme_pblock_start;
			new_length = base - new_base;
			adjusted_base = base + length;
			adjusted_length = data->dpme_pblocks - (length +
			    new_length);
		}
	}
	/* if the map will overflow then punt */
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
		/* adjust this block's size */
		cur->data->dpme_pblock_start = adjusted_base;
		cur->data->dpme_pblocks = adjusted_length;
		cur->data->dpme_lblocks = adjusted_length;
		/* insert new with block address equal to this one */
		if (add_data_to_map(data, cur->disk_address, map) == 0) {
			free(data);
		} else if (act == kSplit) {
			data = create_data(kFreeName, kFreeType, new_base,
			    new_length);
			if (data != NULL) {
				/*
				 * insert new with block address equal to
				 * this one
				 */
				if (add_data_to_map(data, cur->disk_address,
				    map) == 0) {
					free(data);
				}
			}
		}
	}
	renumber_disk_addresses(map);
	map->changed = 1;
	return 1;
}


struct dpme    *
create_data(const char *name, const char *dptype, uint32_t base, uint32_t length)
{
	struct dpme *data;

	data = calloc(1, DEV_BSIZE);
	if (data == NULL) {
		warn("can't allocate memory for disk buffers");
	} else {
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
dpme_init_flags(struct dpme * data)
{
	if (strncasecmp(data->dpme_type, kHFSType, DPISTRLEN) == 0) {
		/* XXX this is gross, fix it! */
		data->dpme_flags = APPLE_HFS_FLAGS_VALUE;
	} else {
		data->dpme_flags = DPME_WRITABLE | DPME_READABLE |
		    DPME_ALLOCATED | DPME_VALID;
	}
}

void
renumber_disk_addresses(struct partition_map_header * map)
{
	struct partition_map *cur;
	long ix;

	/* reset disk addresses */
	cur = map->disk_order;
	ix = 1;
	while (cur != NULL) {
		cur->disk_address = ix++;
		cur->data->dpme_map_entries = map->blocks_in_map;
		cur = cur->next_on_disk;
	}
}

long
compute_device_size(char *name)
{
	struct disklabel dl;
	struct stat st;
	u_int64_t sz;
	int fd;

	fd = opendev(name, O_RDONLY, OPENDEV_PART, NULL);
	if (fd == -1)
		warn("can't open %s", name);

	if (fstat(fd, &st) == -1)
		err(1, "can't fstat %s", name);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file", name);
	if (ioctl(fd, DIOCGPDINFO, &dl) == -1)
		err(1, "can't get disklabel for %s", name);

	close(fd);

	sz = DL_GETDSIZE(&dl);
	if (sz > LONG_MAX)
		sz = LONG_MAX;

	return ((long) sz);
}


void
sync_device_size(struct partition_map_header * map)
{
	struct block0  *p;
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
delete_partition_from_map(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct dpme *data;

	if (strncasecmp(entry->data->dpme_type, kMapType, DPISTRLEN) == 0) {
		printf("Can't delete entry for the map itself\n");
		return;
	}
	if (entry->contains_driver) {
		printf("This program can't install drivers\n");
		if (get_okay("are you sure you want to delete this driver? "
		    "[n/y]: ", 0) != 1) {
			return;
		}
	}
	/* if past end of disk, delete it completely */
	if (entry->next_by_base == NULL &&
	    entry->data->dpme_pblock_start >= entry->the_map->media_size) {
		if (entry->contains_driver) {
			remove_driver(entry);	/* update block0 if necessary */
		}
		delete_entry(entry);
		return;
	}
	/* If at end of disk, incorporate extra disk space to partition */
	if (entry->next_by_base == NULL) {
		entry->data->dpme_pblocks = entry->the_map->media_size -
		    entry->data->dpme_pblock_start;
	}
	data = create_data(kFreeName, kFreeType,
		 entry->data->dpme_pblock_start, entry->data->dpme_pblocks);
	if (data == NULL) {
		return;
	}
	if (entry->contains_driver) {
		remove_driver(entry);	/* update block0 if necessary */
	}
	free(entry->data);
	entry->data = data;
	combine_entry(entry);
	map = entry->the_map;
	renumber_disk_addresses(map);
	map->changed = 1;
}


int
contains_driver(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct block0  *p;
	struct ddmap   *m;
	int i, f;
	uint32_t start;

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
		m = (struct ddmap *) p->sbMap;
		for (i = 0; i < p->sbDrvrCount; i++) {
			start = get_align_long(&m[i].ddBlock);
			if (entry->data->dpme_pblock_start <= f * start &&
			    f * (start + m[i].ddSize) <=
			    (entry->data->dpme_pblock_start +
			    entry->data->dpme_pblocks)) {
				return 1;
			}
		}
	}
	return 0;
}


void
combine_entry(struct partition_map * entry)
{
	struct partition_map *p;
	uint32_t end;

	if (entry == NULL
	|| strncasecmp(entry->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
		return;
	}
	if (entry->next_by_base != NULL) {
		p = entry->next_by_base;
		if (strncasecmp(p->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
			/* next is not free */
		} else if (entry->data->dpme_pblock_start +
			   entry->data->dpme_pblocks !=
			   p->data->dpme_pblock_start) {
			/* next is not contiguous (XXX this is bad) */
			printf("next entry is not contiguous\n");
			/* start is already minimum */
			/* new end is maximum of two ends */
			end = p->data->dpme_pblock_start +
			    p->data->dpme_pblocks;
			if (end > entry->data->dpme_pblock_start +
			    entry->data->dpme_pblocks) {
				entry->data->dpme_pblocks = end -
				    entry->data->dpme_pblock_start;
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
		if (strncasecmp(p->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
			/* previous is not free */
		} else if (p->data->dpme_pblock_start + p->data->dpme_pblocks
			   != entry->data->dpme_pblock_start) {
			/* previous is not contiguous (XXX this is bad) */
			printf("previous entry is not contiguous\n");
			/* new end is maximum of two ends */
			end = p->data->dpme_pblock_start +
			    p->data->dpme_pblocks;
			if (end < entry->data->dpme_pblock_start +
			    entry->data->dpme_pblocks) {
				end = entry->data->dpme_pblock_start +
				    entry->data->dpme_pblocks;
			}
			entry->data->dpme_pblocks = end -
			    p->data->dpme_pblock_start;
			/* new start is previous entry's start */
			entry->data->dpme_pblock_start =
			    p->data->dpme_pblock_start;
			entry->data->dpme_lblocks = entry->data->dpme_pblocks;
			delete_entry(p);
		} else {
			entry->data->dpme_pblock_start =
			    p->data->dpme_pblock_start;
			entry->data->dpme_pblocks += p->data->dpme_pblocks;
			entry->data->dpme_lblocks = entry->data->dpme_pblocks;
			delete_entry(p);
		}
	}
	entry->contains_driver = contains_driver(entry);
}


void
delete_entry(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *p;

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
	free(entry);
}


struct partition_map *
find_entry_by_disk_address(long ix, struct partition_map_header * map)
{
	struct partition_map *cur;

	cur = map->disk_order;
	while (cur != NULL) {
		if (cur->disk_address == ix) {
			break;
		}
		cur = cur->next_on_disk;
	}
	return cur;
}


struct partition_map *
find_entry_by_type(const char *type_name, struct partition_map_header * map)
{
	struct partition_map *cur;

	cur = map->base_order;
	while (cur != NULL) {
		if (strncasecmp(cur->data->dpme_type, type_name, DPISTRLEN) ==
		    0) {
			break;
		}
		cur = cur->next_by_base;
	}
	return cur;
}

struct partition_map *
find_entry_by_base(uint32_t base, struct partition_map_header * map)
{
	struct partition_map *cur;

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
move_entry_in_map(long old_index, long ix, struct partition_map_header * map)
{
	struct partition_map *cur;

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
remove_from_disk_order(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *p;

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
insert_in_disk_order(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *cur;

	/* find position in disk list & insert */
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
		for (cur = map->disk_order; cur != NULL;
		    cur = cur->next_on_disk) {
			if (cur->disk_address <= entry->disk_address &&
			    (cur->next_on_disk == NULL ||
			    entry->disk_address <=
			    cur->next_on_disk->disk_address)) {
				entry->next_on_disk = cur->next_on_disk;
				cur->next_on_disk = entry;
				entry->prev_on_disk = cur;
				if (entry->next_on_disk != NULL) {
					entry->next_on_disk->prev_on_disk =
					    entry;
				}
				break;
			}
		}
	}
}


void
insert_in_base_order(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *cur;

	/* find position in base list & insert */
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
		for (cur = map->base_order; cur != NULL;
		    cur = cur->next_by_base) {
			if (cur->data->dpme_pblock_start <=
			    entry->data->dpme_pblock_start &&
			    (cur->next_by_base == NULL ||
			    entry->data->dpme_pblock_start <=
			    cur->next_by_base->data->dpme_pblock_start)) {
				entry->next_by_base = cur->next_by_base;
				cur->next_by_base = entry;
				entry->prev_by_base = cur;
				if (entry->next_by_base != NULL) {
					entry->next_by_base->prev_by_base =
					    entry;
				}
				break;
			}
		}
	}
}


void
resize_map(long new_size, struct partition_map_header * map)
{
	struct partition_map *entry;
	struct partition_map *next;
	int incr;

	entry = find_entry_by_type(kMapType, map);

	if (entry == NULL) {
		printf("Couldn't find entry for map!\n");
		return;
	}
	next = entry->next_by_base;

	if (new_size == entry->data->dpme_pblocks) {
		return;
	}
	/* make it smaller */
	if (new_size < entry->data->dpme_pblocks) {
		if (next == NULL ||
		    strncasecmp(next->data->dpme_type, kFreeType, DPISTRLEN) !=
		    0) {
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
	/* make it larger */
	if (next == NULL ||
	    strncasecmp(next->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
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
remove_driver(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct block0  *p;
	struct ddmap   *m;
	int i, j, f;
	uint32_t start;

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
		m = (struct ddmap *) p->sbMap;
		for (i = 0; i < p->sbDrvrCount; i++) {
			start = get_align_long(&m[i].ddBlock);

			/*
			 * zap the driver if it is wholly contained in the
			 * partition
			 */
			if (entry->data->dpme_pblock_start <= f * start &&
			    f * (start + m[i].ddSize) <=
			    (entry->data->dpme_pblock_start
				+ entry->data->dpme_pblocks)) {
				/* delete this driver */
				/*
				 * by copying down later ones and zapping the
				 * last
				 */
				for (j = i + 1; j < p->sbDrvrCount; j++, i++) {
					put_align_long(get_align_long(
					    &m[j].ddBlock), &m[i].ddBlock);
					m[i].ddSize = m[j].ddSize;
					m[i].ddType = m[j].ddType;
				}
				put_align_long(0, &m[i].ddBlock);
				m[i].ddSize = 0;
				m[i].ddType = 0;
				p->sbDrvrCount -= 1;
				return;	/* XXX if we continue we will delete
					 * other drivers? */
			}
		}
	}
}

int
read_block(struct partition_map_header * map, unsigned long num, char *buf)
{
	return read_file_media(map->m, ((long long) num) * map->logical_block,
			       DEV_BSIZE, (void *) buf);
}


int
write_block(struct partition_map_header * map, unsigned long num, char *buf)
{
	return write_file_media(map->m, ((long long) num) * map->logical_block,
				DEV_BSIZE, (void *) buf);
}
