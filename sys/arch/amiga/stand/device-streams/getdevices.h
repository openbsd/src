/* -------------------------------------------------- 
 |  NAME
 |    getdevices
 |  PURPOSE
 |    header for getdevices.c
 |  NOTES
 | 
 |  COPYRIGHT
 |    Copyright (C) 1993  Christian E. Hopps
 |
 |    This program is free software; you can redistribute it and/or modify
 |    it under the terms of the GNU General Public License as published by
 |    the Free Software Foundation; either version 2 of the License, or
 |    (at your option) any later version.
 |
 |    This program is distributed in the hope that it will be useful,
 |    but WITHOUT ANY WARRANTY; without even the implied warranty of
 |    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 |    GNU General Public License for more details.
 |
 |    You should have received a copy of the GNU General Public License
 |    along with this program; if not, write to the Free Software
 |    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 |    
 |  HISTORY
 |   chopps - Oct 9, 1993: Created.
 +--------------------------------------------------- */

#if ! defined (_GETDEVICES_H)
#define _GETDEVICES_H

#include "util.h"
#include "devices.h"
#include <devices/hardblocks.h>

struct device {
    struct Node node;
    struct List units;
    char *name;				  /* name of exec device. */
    
};

/* structure that holds all info on this paticular unit for an exec device. */
struct unit {
    struct Node node;
    struct List parts;
    struct RigidDiskBlock *rdb;
    char *name;					  /* just a pointer to the */
						  /* lists data. */
    ulong rdb_at;				  /* what block the rdb is at. */
    ulong unit;					  /* unit number of drive. */
    ulong flags;				  /* unit number of drive. */
    ulong total_blocks;				  /* total blocks on drive. */
    ulong cylinders;				  /* number of cylinders. */
    ulong heads;				  /* number of heads. */
    ulong blocks_per_track;			  /* number of blocks per head */
						  /* per cylinder. */
    ulong bytes_per_block;			  /* number of bytes per block. */
};

struct partition {
    struct Node  node;
    struct unit *unit;				  /* back pointer. */
    struct PartitionBlock pb;			  /* partition block. */
    char  *name;				  /* name of the partition. */
    ulong  start_block;				  /* block that partition */
						  /* starts on. */
    ulong  end_block;				  /* block that partition ends */
						  /* on. */
    ulong  total_blocks;			  /* total number of blocks for */
						  /* this partition (e-s+1) */
    ulong  block_size;				  /* size of blocks for this partition. */
};

struct List * get_drive_list (void);
void free_drive_list (struct List *l);
int add_name_to_drive_list (struct List *l, char *dev_name);
char * get_hard_drive_device_name (struct DosList *dl);

#endif /* _GETDEVICES_H */
