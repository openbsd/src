/*	$OpenBSD: rf_diskthreads.h,v 1.1 1999/01/11 14:29:18 niklas Exp $	*/
/*	$NetBSD: rf_diskthreads.h,v 1.1 1998/11/13 04:20:29 oster Exp $	*/
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
 * :  
 * Log: rf_diskthreads.h,v 
 * Revision 1.7  1996/06/10 11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.6  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.5  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.4  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.3  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/18  19:55:58  jimz
 * Initial revision
 *
 */
/*
 * rf_diskthreads.h -- types and prototypes for disk thread system
 */

#ifndef _RF__RF_DISKTHREADS_H_
#define _RF__RF_DISKTHREADS_H_

#include "rf_types.h"

/* this is the information that a disk thread needs to do its job */
struct RF_DiskId_s {
  RF_DiskQueue_t  *queue;
  RF_Raid_t       *raidPtr;
  RF_RaidDisk_t   *disk;
  int              fd;       /* file descriptor */
  RF_RowCol_t      row, col; /* debug only */
#ifdef SIMULATE
  int              state;
#endif /* SIMULATE */
};

int rf_ConfigureDiskThreads(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);

#ifdef SIMULATE
int rf_SetDiskIdle(RF_Raid_t *raidPtr, RF_RowCol_t r, RF_RowCol_t c);
int rf_ScanDiskQueues(RF_Raid_t *raidPtr);
void rf_simulator_complete_io(RF_DiskId_t *id);
void rf_PrintDiskStat(RF_Raid_t *raidPtr);
#else /* SIMULATE */
int rf_ShutdownDiskThreads(RF_Raid_t *raidPtr);
#endif /* SIMULATE */

#endif /* !_RF__RF_DISKTHREADS_H_ */
