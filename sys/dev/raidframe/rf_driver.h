/*	$OpenBSD: rf_driver.h,v 1.1 1999/01/11 14:29:19 niklas Exp $	*/
/*	$NetBSD: rf_driver.h,v 1.1 1998/11/13 04:20:29 oster Exp $	*/
/*
 * rf_driver.h
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
 * Log: rf_driver.h,v 
 * Revision 1.11  1996/07/11 19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.10  1996/06/10  14:18:58  jimz
 * move user, throughput stats into per-array structure
 *
 * Revision 1.9  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.8  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.7  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.6  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.5  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.4  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.3  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/18  19:56:10  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_DRIVER_H_
#define _RF__RF_DRIVER_H_

#include "rf_threadstuff.h"
#include "rf_types.h"

RF_DECLARE_EXTERN_MUTEX(rf_printf_mutex)

int rf_BootRaidframe(void);
int rf_UnbootRaidframe(void);
int rf_Shutdown(RF_Raid_t *raidPtr);
int rf_Configure(RF_Raid_t *raidPtr, RF_Config_t *cfgPtr);
RF_RaidAccessDesc_t *rf_AllocRaidAccDesc(RF_Raid_t *raidPtr, RF_IoType_t type,
	RF_RaidAddr_t raidAddress, RF_SectorCount_t numBlocks, caddr_t bufPtr,
	void *bp, RF_DagHeader_t **paramDAG, RF_AccessStripeMapHeader_t **paramASM,
	RF_RaidAccessFlags_t flags, void (*cbF)(struct buf *), void *cbA,
	RF_AccessState_t *states);
void rf_FreeRaidAccDesc(RF_RaidAccessDesc_t *desc);
int rf_DoAccess(RF_Raid_t *raidPtr, RF_IoType_t type, int async_flag,
	RF_RaidAddr_t raidAddress, RF_SectorCount_t numBlocks, caddr_t bufPtr,
	void *bp_in, RF_DagHeader_t **paramDAG,
	RF_AccessStripeMapHeader_t **paramASM, RF_RaidAccessFlags_t flags,
	RF_RaidAccessDesc_t **paramDesc, void (*cbF)(struct buf *), void *cbA);
int rf_SetReconfiguredMode(RF_Raid_t *raidPtr, RF_RowCol_t row,
	RF_RowCol_t col);
int rf_FailDisk(RF_Raid_t *raidPtr, RF_RowCol_t frow, RF_RowCol_t fcol,
	int initRecon);
#ifdef SIMULATE
void rf_ScheduleContinueReconstructFailedDisk(RF_RaidReconDesc_t *reconDesc);
#endif /* SIMULATE */
void rf_SignalQuiescenceLock(RF_Raid_t *raidPtr, RF_RaidReconDesc_t *reconDesc);
int  rf_SuspendNewRequestsAndWait(RF_Raid_t *raidPtr);
void rf_ResumeNewRequests(RF_Raid_t *raidPtr);
void rf_StartThroughputStats(RF_Raid_t *raidPtr);
void rf_StartUserStats(RF_Raid_t *raidPtr);
void rf_StopUserStats(RF_Raid_t *raidPtr);
void rf_UpdateUserStats(RF_Raid_t *raidPtr, int rt, int numsect);
void rf_PrintUserStats(RF_Raid_t *raidPtr);

#endif /* !_RF__RF_DRIVER_H_ */
