/*	$OpenBSD: rf_stripelocks.h,v 1.1 1999/01/11 14:29:51 niklas Exp $	*/
/*	$NetBSD: rf_stripelocks.h,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
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

/* :  
 * Log: rf_stripelocks.h,v 
 * Revision 1.22  1996/06/10 11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.21  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.20  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.19  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.18  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.17  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.16  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.15  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.14  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.13  1996/05/06  22:08:46  wvcii
 * added copyright info and change log
 *
 */

/*****************************************************************************
 *
 * stripelocks.h -- header file for locking stripes 
 *
 * Note that these functions are called from the execution routines of certain
 * DAG Nodes, and so they must be NON-BLOCKING to assure maximum parallelism
 * in the DAG.  Accordingly, when a node wants to acquire a lock, it calls
 * AcquireStripeLock, supplying a pointer to a callback function.  If the lock
 * is free at the time of the call, 0 is returned, indicating that the lock
 * has been acquired.  If the lock is not free, 1 is returned, and a copy of
 * the function pointer and argument are held in the lock table.  When the
 * lock becomes free, the callback function is invoked.
 *
 *****************************************************************************/

#ifndef _RF__RF_STRIPELOCKS_H_
#define _RF__RF_STRIPELOCKS_H_

#include <sys/buf.h>

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_general.h"

struct RF_LockReqDesc_s {
  RF_IoType_t type;            /* read or write */
  RF_int64 start, stop;        /* start and end of range to be locked */
  RF_int64 start2, stop2;      /* start and end of 2nd range to be locked */
  void (*cbFunc)(struct buf *);/* callback function */
  void *cbArg;                 /* argument to callback function */
  RF_LockReqDesc_t *next;      /* next element in chain */
  RF_LockReqDesc_t *templink;  /* for making short-lived lists of request descriptors */
};

#define RF_ASSERT_VALID_LOCKREQ(_lr_) { \
	RF_ASSERT(RF_IO_IS_R_OR_W((_lr_)->type)); \
}

struct RF_StripeLockDesc_s {
  RF_StripeNum_t     stripeID; /* the stripe ID */
  RF_LockReqDesc_t  *granted;  /* unordered list of granted requests */
  RF_LockReqDesc_t  *waitersH; /* FIFO queue of all waiting reqs, both read and write (Head and Tail) */
  RF_LockReqDesc_t  *waitersT;
  int                nWriters; /* number of writers either granted or waiting */
  RF_StripeLockDesc_t  *next;  /* for hash table collision resolution */
};

struct RF_LockTableEntry_s {
  RF_DECLARE_MUTEX(mutex)         /* mutex on this hash chain */
  RF_StripeLockDesc_t  *descList; /* hash chain of lock descriptors */
};

/*
 * Initializes a stripe lock descriptor.  _defSize is the number of sectors
 * that we lock when there is no parity information in the ASM (e.g. RAID0).
 */

#define RF_INIT_LOCK_REQ_DESC(_lrd, _typ, _cbf, _cba, _asm, _defSize)                                           \
  {                                                                                                          \
    (_lrd).type    = _typ;                                                                                   \
    (_lrd).start2  = -1;                                                                                     \
    (_lrd).stop2   = -1;                                                                                     \
    if ((_asm)->parityInfo) {                                                                                \
      (_lrd).start = (_asm)->parityInfo->startSector;                                                        \
      (_lrd).stop  = (_asm)->parityInfo->startSector + (_asm)->parityInfo->numSector-1;                      \
      if ((_asm)->parityInfo->next) {                                                                        \
        (_lrd).start2  = (_asm)->parityInfo->next->startSector;                                              \
        (_lrd).stop2   = (_asm)->parityInfo->next->startSector + (_asm)->parityInfo->next->numSector-1;      \
      }                                                                                                      \
    } else {                                                                                                 \
      (_lrd).start = 0;                                                                                      \
      (_lrd).stop  = (_defSize);                                                                             \
    }													     \
    (_lrd).templink= NULL;                                                                                   \
    (_lrd).cbFunc  = (_cbf);                                                                                 \
    (_lrd).cbArg   = (void *) (_cba);                                                                        \
  }

int rf_ConfigureStripeLockFreeList(RF_ShutdownList_t **listp);
RF_LockTableEntry_t *rf_MakeLockTable(void);
void rf_ShutdownStripeLocks(RF_LockTableEntry_t *lockTable);
int rf_ConfigureStripeLocks(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
int rf_AcquireStripeLock(RF_LockTableEntry_t *lockTable,
	RF_StripeNum_t stripeID, RF_LockReqDesc_t *lockReqDesc);
void rf_ReleaseStripeLock(RF_LockTableEntry_t *lockTable,
	RF_StripeNum_t stripeID, RF_LockReqDesc_t *lockReqDesc);

#endif /* !_RF__RF_STRIPELOCKS_H_ */
