/* -------------------------------------------------- 
 |  NAME
 |    getdevices
 |  PURPOSE
 |    build a  drive list from RDB from exec.device's
 |    found on the Dos list.
 |  NOTES
 |    This code relies on peoples hard drive controllers
 |    working properly.  I used similar code in some
 |    cross dos configuration code and it seemed to
 |    lock some amiga controllers up when it polled
 |    each unit.   I don't wish to do all the checks
 |    I did then, so if your controller is one of these
 |    types (you'll find out I guess) sorry.
 |
 |    known working types:
 |   ----------------------
 |    scsi.device.
 |    gvpscsi.device.
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
 |    chopps - Oct 9, 1993: Created.
 +--------------------------------------------------- */

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <devices/hardblocks.h>
#include <string.h>
#include "util.h"
#include "getdevices.h"

struct List *
get_drive_list (void)
{
    struct DosList *dl;
    struct device *dev;
    
    struct List *drive_list = zmalloc (sizeof (*drive_list));
    if (drive_list == NULL) {
	return (NULL);
    }
    NewList (drive_list);

    D (debug_message ("Walking (LDF_READ|LDF_DEVICES) dos list."));
    /* walk the dos list and fetch our device names. */
    dl = LockDosList (LDF_DEVICES|LDF_READ); {
	while (dl = NextDosEntry (dl,LDF_DEVICES)) {
	    char * name;
	    
	    name = get_hard_drive_device_name (dl);
	    if (name) {				  /* if the drive has a device */
						  /* name.  */
		add_name_to_drive_list (drive_list, name);
	    }
	}
    } UnLockDosList (LDF_DEVICES);
    D (debug_message ("done walking (LDF_READ|LDF_DEVICES) dos list."));

    /* now get the units. */
    dev = ptrfrom (struct device, node, drive_list->lh_Head);
    while (dev->node.ln_Succ) {
	ulong i;
	for (i = 0; i < 7; i++) {
	    struct device_data *dd = alloc_device (dev->name, i, 0, sizeof (struct IOStdReq));
	    if (dd) {
		/* we have a unit. */
		do_unit (dev, dd);
		free_device (dd);
	    }
	}	
	dev = ptrfrom (struct device, node, dev->node.ln_Succ);
    }
    return (drive_list);
}

void
free_drive_list (struct List *l)
{
    struct device *d = ptrfrom (struct device, node, l->lh_Head);
    while (d->node.ln_Succ) {
	struct Node *n;
	struct device *next = ptrfrom (struct device, node, d->node.ln_Succ);
	D (verbose_debug_message ("zfree()'ing \"%s\"", d->name));
	while (n = RemHead (&d->units)) {
	    struct unit *u = ptrfrom (struct unit, node, n);
	    free_unit (u);
	}
	zfree (d->name);			  /* free name. */
	zfree (d);				  /* free node. */
	d = next;
    }
}

/* this function returns an error or 0 for success. */
/* -1 for NO MEM. */
/* 1 for already on list */
int
add_name_to_drive_list (struct List *l, char *dev_name)
{
    if (!find_name (l,dev_name)) {		  /* not on list. */
	struct device *d;
	
	verbose_message ("found new device \"%s\"", dev_name);
	d = zmalloc (sizeof (*d));
	if (d) {
	    d->node.ln_Name = d->name = copy_string (dev_name);
	    NewList (&d->units);
	    if (d->name) {
		AddTail (l, &d->node);
		return (0);
	    }
	    zfree (d);
	}
	warn_message ("cannot allocate, unable to process \"%s\"",dev_name);
	return (-1);
    } else {
	return (1);
    }
}


char *
get_hard_drive_device_name (struct DosList *dl)
{
    struct DeviceNode *dn = (struct DeviceNode *)dl;
    if (dn->dn_Type == DLT_DEVICE) {
	struct FileSysStartupMsg *m = BADDR (dn->dn_Startup);
	D (debug_message ("checking dos device \"%b\" for info.", dl->dol_Name));
	if (m && valid_mem (m)) {
	    ULONG *envec = BADDR (m->fssm_Environ);
	    char  *name = BADDR (m->fssm_Device); /* null term bstring. */
	    name++;				  /* inc for bstring adj. */

	    if (valid_mem (envec) && valid_mem (name)) {
		if (envec[DE_TABLESIZE] > DE_UPPERCYL) {
		    ulong dev_size = envec[DE_UPPERCYL] - envec[DE_LOWCYL] + 1;
		    dev_size *= envec[DE_NUMHEADS] * envec[DE_BLKSPERTRACK];
		    dev_size *= envec[DE_SIZEBLOCK] << 2;
		    if (dev_size > (1024*1024*2)) {
			return (name);		  /* if larger than 2M */
		    }
		}
	    } else {
		D (verbose_debug_message ("\"%b\"'s startup message is non-standard.", dl->dol_Name));
	    }
	} else {
	    D (verbose_debug_message ("\"%b\" has no startup message.", dl->dol_Name));	
	}
    }
    return (NULL);
}

ulong checksum (ulong sl, ulong *buf)
{
    ulong ck = 0;
    while (sl--) {
	ck += *buf++;
    }
    return (ck);
}

void
do_unit (struct device *dev, struct device_data *dd)
{
    struct unit *u = zmalloc (sizeof (*u));
    if (u) {
	int i;
	u->name = dev->name;
	NewList (&u->parts);
	u->unit = dd->unit;
	u->flags = dd->flags;
	u->rdb_at = (ulong)-1L;
	u->rdb = zmalloc (512);
	if (NULL == u->rdb) {
	    free_unit (u);
	    return;
	}
	/* scans the first 200 blocks for the RDB root. */
	for (i = 0; i<200; i++) {
	    if (512 != device_read (dd, 512*i, 512, u->rdb)) {
		verbose_message ("warn: unable to read \"%s\" unit: %ld flags 0x%lx",
			     dd->name, dd->unit, dd->flags);
		free_unit (u);
		return;
	    }
	    if (u->rdb->rdb_ID == IDNAME_RIGIDDISK) {
		if (!checksum (u->rdb->rdb_SummedLongs, (ulong *)u->rdb)) {
		    u->rdb_at = i;
		    u->cylinders = u->rdb->rdb_Cylinders;
		    u->heads = u->rdb->rdb_Heads;
		    u->blocks_per_track = u->rdb->rdb_Sectors;
		    u->bytes_per_block = u->rdb->rdb_BlockBytes;
		    u->total_blocks = u->cylinders*u->heads*u->blocks_per_track;
		    verbose_message ("found drive %.8s %.16s %.4s [capacity:%ldM]"
				     "\n at unit %ld on device \"%s\"" ,
				     u->rdb->rdb_DiskVendor,
				     u->rdb->rdb_DiskProduct,
				     u->rdb->rdb_DiskRevision,
				     (u->total_blocks*u->bytes_per_block)/(1024*1024),
				     u->unit, u->name);
		    if (u->rdb->rdb_PartitionList != (ulong)~0) {
			get_partitions (dd, u);
		    }
		    AddTail (&dev->units, &u->node);
		    break;
		} else {
		    warn_message ("found RDB at %ld on unit %ld of \"%s\", failed checksum", i, u->unit, u->name);
		    break;
		}
	    }
	}
	if (u->rdb_at == (ulong)-1L) {
	    verbose_message ("\"%s\" at unit: %ld has no RDB.", u->name, u->unit);
	    free_unit (u);
	    return;
	} 
    }
}

void
free_unit (struct unit *u)
{
    if (u) {
	struct Node *n;
	while (n = RemHead (&u->parts)) {
	    struct partition *p = ptrfrom (struct partition, node, n);
	    free_partition (p);
	}
	zfree (u->rdb);
	zfree (u);
    }
}


void
get_partitions (struct device_data *dd, struct unit *u)
{
    ulong bpb = u->bytes_per_block;
    struct PartitionBlock *pb = zmalloc (bpb);
    if (pb) {
	ulong partblock = u->rdb->rdb_PartitionList;
	while (partblock != (ulong)~0) {
	    ulong nextpartblock = (ulong)~0;
	    
	    if (bpb != device_read (dd, bpb*partblock, bpb, pb)) {
		verbose_message ("warn: unable to read block: %ld from \"%s\" unit: %ld flags 0x%lx",
				 partblock, dd->name, dd->unit, dd->flags);
		break;
	    }
	    if (pb->pb_ID == IDNAME_PARTITION) {
		if (!checksum (pb->pb_SummedLongs, (ulong *)pb)) {
		    if (pb->pb_Environment[DE_TABLESIZE] > DE_UPPERCYL) {
			struct partition *p = zmalloc (sizeof (struct partition));
			if (p) {
			    ulong *e;
			    CopyMem (pb, &p->pb, sizeof (struct PartitionBlock));
			    e = p->pb.pb_Environment;
			    p->name = p->pb.pb_DriveName;
			    p->name[p->name[0]+1] = 0;
			    p->name++;			  /* adjust for size */
							  /* byte. */
			    p->node.ln_Name = p->name;
			    p->unit = u;
			    p->start_block = e[DE_LOWCYL]*e[DE_NUMHEADS]*e[DE_BLKSPERTRACK];
			    p->end_block = (e[DE_UPPERCYL]+1)*e[DE_NUMHEADS]*e[DE_BLKSPERTRACK] - 1;
			    p->total_blocks = p->end_block - p->start_block + 1;
			    p->block_size = e[DE_SIZEBLOCK] << 2;

			    /* the size stuff is convoluted to avoid overflow. */
			    verbose_message ("| partition: \"%s\" sb: %ld eb: %ld totb: %ld",
					     p->name, p->start_block,
					     p->end_block, p->total_blocks);
			    verbose_message ("|            Block Size: %ld Capacity: %ld.%ldM",
					     p->block_size,
					     (p->block_size*p->total_blocks)/(1024*1024),
					     (10*((p->block_size*p->total_blocks)/(1024) % 1024))/1024);
			    
			    nextpartblock = pb->pb_Next;
			    p->unit = u;
			    AddTail (&u->parts, &p->node);
			} else {
			    warn_message ("failed to allocate memory for partition");
			}
		    } else {
			warn_message ("found PART at %ld on unit %ld of\"%s\",\n      tablesize to small",
				      partblock, u->unit, u->name);
			break;
		    }

		} else {
		    warn_message ("found PART at %ld on unit %ld of \"%s\", failed checksum", 
				  partblock, u->unit, u->name);
		    break;
		}
	    }
	    partblock = nextpartblock;
	}
	zfree (pb);
    }
}

void
free_partition (struct partition *p)
{
    zfree (p);
}

    
