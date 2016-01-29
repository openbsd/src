/*	$OpenBSD: partition_map.h,v 1.36 2016/01/29 22:51:43 krw Exp $	*/

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

struct ddmap {
    uint32_t	ddBlock;	/* 1st driver's starting sbBlkSize block */
    uint16_t	ddSize;		/* size of 1st driver (512-byte blks) */
    uint16_t	ddType;		/* system type (1 for Mac+) */
};

struct entry;

struct partition_map {
    LIST_HEAD(, entry)	disk_order;
    LIST_HEAD(, entry)	base_order;
    char	       *name;
    int			fd;
    int			changed;
    int			physical_block;
    int			blocks_in_map;
    int			maximum_in_map;
    unsigned long	media_size;	/* in physical blocks */

    /* On-disk block 0 data. */
    uint16_t		sbSig;		/* "ER" */
    uint16_t		sbBlkSize;	/* physical block size of device */
    uint32_t		sbBlkCount;	/* # of physical blocks on device */
    uint16_t		sbDevType;	/* device type */
    uint16_t		sbDevId;	/* device id */
    uint32_t		sbData;		/* not used */
    uint16_t		sbDrvrCount;	/* driver descriptor count */
    struct ddmap	sbDDMap[8];	/* driver descriptor map*/
    uint8_t		sbReserved[430];
};

struct entry {
    LIST_ENTRY(entry)		disk_entry;
    LIST_ENTRY(entry)		base_entry;
    struct partition_map       *the_map;
    struct dpme		       *dpme;
    long			disk_address;
    int				contains_driver;
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

struct partition_map	*init_partition_map(char *);
struct partition_map	*create_partition_map(int, char *, uint64_t, uint32_t);
struct partition_map	*open_partition_map(int, char *, uint64_t, uint32_t);

struct entry	*find_entry_by_disk_address(long, struct partition_map *);
struct entry	*find_entry_by_type(const char *, struct partition_map *);
struct entry	*find_entry_by_base(uint32_t, struct partition_map *);

int	add_partition_to_map(const char *, const char *, uint32_t, uint32_t,
    struct partition_map *);

void	free_partition_map(struct partition_map *);
void	delete_partition_from_map(struct entry *);
void	move_entry_in_map(long, long, struct partition_map *);
void	resize_map(long new_size, struct partition_map *);
void	write_partition_map(struct partition_map *);
void	dpme_init_flags(struct dpme *);
void	sync_device_size(struct partition_map *);

#endif /* __partition_map__ */
