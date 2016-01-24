/*	$OpenBSD: partition_map.h,v 1.29 2016/01/24 01:38:32 krw Exp $	*/

/*
 * partition_map.h - partition map routines
 *
 * Written by Eryk Vershen
 */

/*
 * Copyright 1996,1998 by Apple Computer, Inc.
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

#ifndef __partition_map__
#define __partition_map__

struct partition_map_header {
    char		       *name;
    struct partition_map       *disk_order;
    struct partition_map       *base_order;
    struct block0	       *block0;
    int				fd;
    int				changed;
    int				physical_block;	/* must be == sbBlockSize */
    int				logical_block;	/* must be <= physical_block */
    int				blocks_in_map;
    int				maximum_in_map;
    unsigned long		media_size;	/* in logical_blocks */
};

struct partition_map {
    struct partition_map	       *next_on_disk;
    struct partition_map	       *prev_on_disk;
    struct partition_map	       *next_by_base;
    struct partition_map	       *prev_by_base;
    struct partition_map_header	       *the_map;
    struct dpme			       *dpme;
    long				disk_address;
    int					contains_driver;
};

extern const char *kFreeType;
extern const char *kMapType;
extern const char *kUnixType;
extern const char *kHFSType;
extern const char *kFreeName;
extern const char *kPatchType;

extern int dflag;
extern int lflag;
extern int rflag;

struct partition_map_header	*init_partition_map(char *);
struct partition_map_header	*create_partition_map(int, char *, uint64_t,
    uint32_t);
struct partition_map_header	*open_partition_map(int, char *, uint64_t,
    uint32_t);

struct partition_map		*find_entry_by_disk_address(long,
    struct partition_map_header *);
struct partition_map		*find_entry_by_type(const char *,
    struct partition_map_header *);
struct partition_map		*find_entry_by_base(uint32_t,
    struct partition_map_header *);

int	add_partition_to_map(const char *, const char *, uint32_t, uint32_t,
    struct partition_map_header *);
void	free_partition_map(struct partition_map_header *);
void	delete_partition_from_map(struct partition_map *);
void	move_entry_in_map(long, long, struct partition_map_header *);
void	resize_map(long new_size, struct partition_map_header *);
void	write_partition_map(struct partition_map_header *);
void	dpme_init_flags(struct dpme *);
void	sync_device_size(struct partition_map_header *);

#endif /* __partition_map__ */
