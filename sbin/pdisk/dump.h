//
// dump.h - dumping partition maps
//
// Written by Eryk Vershen
//

/*
 * Copyright 1996,1997 by Apple Computer, Inc.
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

#ifndef __dump__
#define __dump__

#include "partition_map.h"


//
// Defines
//
#define AFLAG_DEFAULT	0
#define PFLAG_DEFAULT	1
#define FFLAG_DEFAULT	0


//
// Types
//


//
// Global Constants
//


//
// Global Variables
//
extern int aflag;
extern int pflag;
extern int fflag;


//
// Forward declarations
//
void display_patches(partition_map *entry);
int dump(char *name);
void dump_block(unsigned char *addr, int len);
void dump_partition_map(partition_map_header *map, int disk_order);
void full_dump_partition_entry(partition_map_header *, int);
void full_dump_block_zero(partition_map_header *map);
void list_all_disks(void);
void show_data_structures(partition_map_header *map);

#endif /* __dump__ */
