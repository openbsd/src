/*	$OpenBSD: rf_disks.h,v 1.5 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_disks.h,v 1.8 2000/03/27 03:25:17 oster Exp $	*/

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
 * rf_disks.h -- Header file for code related to physical disks.
 */

#ifndef	_RF__RF_DISKS_H_
#define	_RF__RF_DISKS_H_

#include <sys/types.h>

#include "rf_archs.h"
#include "rf_types.h"

#if	defined(__NetBSD__)
#include "rf_netbsd.h"
#elif	defined(__OpenBSD__)
#include "rf_openbsd.h"
#endif

/*
 * A physical disk can be in one of several states:
 * IF YOU ADD A STATE, CHECK TO SEE IF YOU NEED TO MODIFY RF_DEAD_DISK() BELOW.
 */
enum RF_DiskStatus_e {
	rf_ds_optimal,		/* No problems. */
	rf_ds_failed,		/* Reconstruction ongoing. */
	rf_ds_reconstructing,	/*
				 * Reconstruction complete to spare, dead disk
				 * not yet replaced.
				 */
	rf_ds_dist_spared,	/*
				 * Reconstruction complete to distributed
				 * spare space, dead disk not yet replaced.
				 */
	rf_ds_spared,		/*
				 * Reconstruction complete to distributed
				 * spare space, dead disk not yet replaced.
				 */
	rf_ds_spare,		/* An available spare disk. */
	rf_ds_used_spare	/*
				 * A spare which has been used, and hence is
				 * not available.
				 */
};
typedef enum RF_DiskStatus_e RF_DiskStatus_t;

struct RF_RaidDisk_s {
	char devname[56];		/* Name of device file. */
	RF_DiskStatus_t status;		/* Whether it is up or down. */
	RF_RowCol_t spareRow;		/*
					 * If in status "spared", this
					 * identifies the spare disk.
					 */
	RF_RowCol_t spareCol;		/*
					 * If in status "spared", this
					 * identifies the spare disk.
					 */
	RF_SectorCount_t numBlocks;	/*
					 * Number of blocks, obtained via
					 * READ CAPACITY.
					 */
	int blockSize;
	RF_SectorCount_t partitionSize; /*
					 * The *actual* and *full* size of
					 * the partition, from the disklabel.
					 */
	int auto_configured;		/*
					 * 1 if this component was
					 * autoconfigured. 0 otherwise.
					 */
	dev_t dev;
};

/*
 * An RF_DiskOp_t ptr is really a pointer to a UAGT_CCB, but I want
 * to isolate the CAM layer from all other layers, so I typecast to/from
 * RF_DiskOp_t * (i.e. void *) at the interfaces.
 */
typedef void RF_DiskOp_t;

/* If a disk is in any of these states, it is inaccessible. */
#define	RF_DEAD_DISK(_dstat_)						\
	(((_dstat_) == rf_ds_spared) ||					\
	 ((_dstat_) == rf_ds_reconstructing) ||				\
	 ((_dstat_) == rf_ds_failed) ||					\
	 ((_dstat_) == rf_ds_dist_spared))

#ifdef	_KERNEL
#if	defined(__NetBSD__)
#include "rf_netbsd.h"
#elif	defined(__OpenBSD__)
#include "rf_openbsd.h"
#endif

int rf_ConfigureDisks(RF_ShutdownList_t **, RF_Raid_t *, RF_Config_t *);
int rf_ConfigureSpareDisks(RF_ShutdownList_t **, RF_Raid_t *, RF_Config_t *);
int rf_ConfigureDisk(RF_Raid_t *, char *, RF_RaidDisk_t *,
	RF_RowCol_t, RF_RowCol_t);
int rf_AutoConfigureDisks(RF_Raid_t *, RF_Config_t *, RF_AutoConfig_t *);
int rf_CheckLabels(RF_Raid_t *, RF_Config_t *);
int rf_add_hot_spare(RF_Raid_t *, RF_SingleComponent_t *);
int rf_remove_hot_spare(RF_Raid_t *, RF_SingleComponent_t *);
int rf_delete_component(RF_Raid_t *, RF_SingleComponent_t *);
int rf_incorporate_hot_spare(RF_Raid_t *, RF_SingleComponent_t *);
#endif	/* _KERNEL */

#endif	/* !_RF__RF_DISKS_H_ */
