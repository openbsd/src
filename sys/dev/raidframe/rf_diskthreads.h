/*	$OpenBSD: rf_diskthreads.h,v 1.2 1999/02/16 00:02:40 niklas Exp $	*/
/*	$NetBSD: rf_diskthreads.h,v 1.3 1999/02/05 00:06:10 oster Exp $	*/
/*
 * rf_diskthreads.h
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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
 * rf_diskthreads.h -- types and prototypes for disk thread system
 */

#ifndef _RF__RF_DISKTHREADS_H_
#define _RF__RF_DISKTHREADS_H_

#include "rf_types.h"

/* this is the information that a disk thread needs to do its job */
struct RF_DiskId_s {
	RF_DiskQueue_t *queue;
	RF_Raid_t *raidPtr;
	RF_RaidDisk_t *disk;
	int     fd;		/* file descriptor */
	RF_RowCol_t row, col;	/* debug only */
};

int 
rf_ConfigureDiskThreads(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);

int     rf_ShutdownDiskThreads(RF_Raid_t * raidPtr);

#endif				/* !_RF__RF_DISKTHREADS_H_ */
