/*	$OpenBSD: rf_disks.h,v 1.1 1999/01/11 14:29:18 niklas Exp $	*/
/*	$NetBSD: rf_disks.h,v 1.1 1998/11/13 04:20:29 oster Exp $	*/
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

/* :  
 * Log: rf_disks.h,v 
 * Revision 1.15  1996/08/20 23:05:13  jimz
 * add nreads, nwrites to RaidDisk
 *
 * Revision 1.14  1996/06/17  03:20:15  jimz
 * increase devname len to 56
 *
 * Revision 1.13  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.12  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.11  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.10  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.9  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.8  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.7  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.6  1996/05/02  22:06:57  jimz
 * add RF_RaidDisk_t
 *
 * Revision 1.5  1995/12/01  15:56:53  root
 * added copyright info
 *
 */

#ifndef _RF__RF_DISKS_H_
#define _RF__RF_DISKS_H_

#include <sys/types.h>

#include "rf_archs.h"
#include "rf_types.h"
#ifdef SIMULATE
#include "rf_geometry.h"
#endif /* SIMULATE  */

/*
 * A physical disk can be in one of several states:
 * IF YOU ADD A STATE, CHECK TO SEE IF YOU NEED TO MODIFY RF_DEAD_DISK() BELOW.
 */
enum RF_DiskStatus_e {
  rf_ds_optimal,        /* no problems */
  rf_ds_failed,         /* reconstruction ongoing */
  rf_ds_reconstructing, /* reconstruction complete to spare, dead disk not yet replaced */
  rf_ds_dist_spared,    /* reconstruction complete to distributed spare space, dead disk not yet replaced */
  rf_ds_spared,         /* reconstruction complete to distributed spare space, dead disk not yet replaced */
  rf_ds_spare,          /* an available spare disk */
  rf_ds_used_spare      /* a spare which has been used, and hence is not available */
};
typedef enum RF_DiskStatus_e RF_DiskStatus_t;

struct RF_RaidDisk_s {
  char              devname[56]; /* name of device file */
  RF_DiskStatus_t   status;      /* whether it is up or down */
  RF_RowCol_t       spareRow;    /* if in status "spared", this identifies the spare disk */
  RF_RowCol_t       spareCol;    /* if in status "spared", this identifies the spare disk */
  RF_SectorCount_t  numBlocks;   /* number of blocks, obtained via READ CAPACITY */
  int               blockSize;
	/* XXX the folling is needed since we seem to need SIMULATE defined
	   in order to get user-land stuff to compile, but we *don't* want
	   this in the structure for the user-land utilities, as the
	   kernel doesn't know about it!! (and it messes up the size of
	   the structure, so there is a communication problem between
	   the kernel and the userland utils :-(  GO */
#if defined(SIMULATE) && !defined(RF_UTILITY)
  RF_DiskState_t    diskState;   /* the name of the disk as used in the disk module */
#endif /* SIMULATE */
#if RF_KEEP_DISKSTATS > 0
  RF_uint64         nreads;
  RF_uint64         nwrites;
#endif /* RF_KEEP_DISKSTATS > 0 */
  dev_t             dev;
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

int rf_ConfigureDisks(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
int rf_ConfigureSpareDisks(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
int rf_ConfigureDisk(RF_Raid_t *raidPtr, char *buf, RF_RaidDisk_t *diskPtr, 
		     RF_DiskOp_t *rdcap_op, RF_DiskOp_t *tur_op, dev_t dev, 
		     RF_RowCol_t row, RF_RowCol_t col);

#ifdef SIMULATE
void rf_default_disk_names(void);
void rf_set_disk_db_name(char *s);
void rf_set_disk_type_name(char *s);
#endif /* SIMULATE */

#endif /* !_RF__RF_DISKS_H_ */
