/*	$OpenBSD: rf_disks.h,v 1.2 1999/02/16 00:02:40 niklas Exp $	*/
/*	$NetBSD: rf_disks.h,v 1.3 1999/02/05 00:06:09 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * rf_disks.h -- header file for code related to physical disks
 */

#ifndef _RF__RF_DISKS_H_
#define _RF__RF_DISKS_H_

#include <sys/types.h>

#include "rf_archs.h"
#include "rf_types.h"

/*
 * A physical disk can be in one of several states:
 * IF YOU ADD A STATE, CHECK TO SEE IF YOU NEED TO MODIFY RF_DEAD_DISK() BELOW.
 */
enum RF_DiskStatus_e {
	rf_ds_optimal,		/* no problems */
	rf_ds_failed,		/* reconstruction ongoing */
	rf_ds_reconstructing,	/* reconstruction complete to spare, dead disk
				 * not yet replaced */
	rf_ds_dist_spared,	/* reconstruction complete to distributed
				 * spare space, dead disk not yet replaced */
	rf_ds_spared,		/* reconstruction complete to distributed
				 * spare space, dead disk not yet replaced */
	rf_ds_spare,		/* an available spare disk */
	rf_ds_used_spare	/* a spare which has been used, and hence is
				 * not available */
};
typedef enum RF_DiskStatus_e RF_DiskStatus_t;

struct RF_RaidDisk_s {
	char    devname[56];	/* name of device file */
	RF_DiskStatus_t status;	/* whether it is up or down */
	RF_RowCol_t spareRow;	/* if in status "spared", this identifies the
				 * spare disk */
	RF_RowCol_t spareCol;	/* if in status "spared", this identifies the
				 * spare disk */
	RF_SectorCount_t numBlocks;	/* number of blocks, obtained via READ
					 * CAPACITY */
	int     blockSize;
	/* XXX the folling is needed since we seem to need SIMULATE defined in
	 * order to get user-land stuff to compile, but we *don't* want this
	 * in the structure for the user-land utilities, as the kernel doesn't
	 * know about it!! (and it messes up the size of the structure, so
	 * there is a communication problem between the kernel and the
	 * userland utils :-(  GO */
#if RF_KEEP_DISKSTATS > 0
	RF_uint64 nreads;
	RF_uint64 nwrites;
#endif				/* RF_KEEP_DISKSTATS > 0 */
	dev_t   dev;
};
/*
 * An RF_DiskOp_t ptr is really a pointer to a UAGT_CCB, but I want
 * to isolate the cam layer from all other layers, so I typecast to/from
 * RF_DiskOp_t * (i.e. void *) at the interfaces.
 */
typedef void RF_DiskOp_t;

/* if a disk is in any of these states, it is inaccessible */
#define RF_DEAD_DISK(_dstat_) (((_dstat_) == rf_ds_spared) || \
	((_dstat_) == rf_ds_reconstructing) || ((_dstat_) == rf_ds_failed) || \
	((_dstat_) == rf_ds_dist_spared))

int 
rf_ConfigureDisks(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
int 
rf_ConfigureSpareDisks(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
int 
rf_ConfigureDisk(RF_Raid_t * raidPtr, char *buf, RF_RaidDisk_t * diskPtr,
    RF_DiskOp_t * rdcap_op, RF_DiskOp_t * tur_op, dev_t dev,
    RF_RowCol_t row, RF_RowCol_t col);

#endif				/* !_RF__RF_DISKS_H_ */
