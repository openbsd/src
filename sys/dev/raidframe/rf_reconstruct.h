/*	$OpenBSD: rf_reconstruct.h,v 1.1 1999/01/11 14:29:47 niklas Exp $	*/
/*	$NetBSD: rf_reconstruct.h,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
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

/*********************************************************
 * rf_reconstruct.h -- header file for reconstruction code
 *********************************************************/

/* :  
 * Log: rf_reconstruct.h,v 
 * Revision 1.25  1996/08/01 15:57:24  jimz
 * minor cleanup
 *
 * Revision 1.24  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.23  1996/07/15  05:40:41  jimz
 * some recon datastructure cleanup
 * better handling of multiple failures
 * added undocumented double-recon test
 *
 * Revision 1.22  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.21  1996/07/11  19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.20  1996/06/11  10:57:30  jimz
 * add rf_RegisterReconDoneProc
 *
 * Revision 1.19  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.18  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.17  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.16  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.15  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.14  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.13  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.12  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.11  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.10  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.9  1995/12/06  15:04:55  root
 * added copyright info
 *
 */

#ifndef _RF__RF_RECONSTRUCT_H_
#define _RF__RF_RECONSTRUCT_H_

#include "rf_types.h"
#include <sys/time.h>
#include "rf_reconmap.h"
#include "rf_psstatus.h"

/* reconstruction configuration information */
struct RF_ReconConfig_s {
  unsigned           numFloatingReconBufs; /* number of floating recon bufs to use */
  RF_HeadSepLimit_t  headSepLimit;         /* how far apart the heads are allow to become, in parity stripes */ 
};

/* a reconstruction buffer */
struct RF_ReconBuffer_s {
  RF_Raid_t        *raidPtr;          /* void * to avoid recursive includes */
  caddr_t           buffer;           /* points to the data */
  RF_StripeNum_t    parityStripeID;   /* the parity stripe that this data relates to */
  int               which_ru;         /* which reconstruction unit within the PSS */
  RF_SectorNum_t failedDiskSectorOffset;/* the offset into the failed disk */
  RF_RowCol_t       row, col;         /* which disk this buffer belongs to or is targeted at */
  RF_StripeCount_t  count;            /* counts the # of SUs installed so far */
  int               priority;         /* used to force hi priority recon */
  RF_RbufType_t     type;             /* FORCED or FLOATING */
  char             *arrived;          /* [x] = 1/0 if SU from disk x has/hasn't arrived */
  RF_ReconBuffer_t *next;             /* used for buffer management */
  void             *arg;              /* generic field for general use */
  RF_RowCol_t       spRow, spCol;     /* spare disk to which this buf should be written */
                                      /* if dist sparing off, always identifies the replacement disk */
  RF_SectorNum_t    spOffset;         /* offset into the spare disk */
                                      /* if dist sparing off, identical to failedDiskSectorOffset */
  RF_ReconParityStripeStatus_t *pssPtr; /* debug- pss associated with issue-pending write */
};

/* a reconstruction event descriptor.  The event types currently are:
 *    RF_REVENT_READDONE    -- a read operation has completed
 *    RF_REVENT_WRITEDONE   -- a write operation has completed
 *    RF_REVENT_BUFREADY    -- the buffer manager has produced a full buffer
 *    RF_REVENT_BLOCKCLEAR  -- a reconstruction blockage has been cleared
 *    RF_REVENT_BUFCLEAR    -- the buffer manager has released a process blocked on submission
 *    RF_REVENT_SKIP        -- we need to skip the current RU and go on to the next one, typ. b/c we found recon forced
 *    RF_REVENT_FORCEDREADONE- a forced-reconstructoin read operation has completed
 */
typedef enum RF_Revent_e {
	RF_REVENT_READDONE,
	RF_REVENT_WRITEDONE,
	RF_REVENT_BUFREADY,
	RF_REVENT_BLOCKCLEAR,
	RF_REVENT_BUFCLEAR,
	RF_REVENT_HEADSEPCLEAR,
	RF_REVENT_SKIP,
	RF_REVENT_FORCEDREADDONE
} RF_Revent_t;

struct RF_ReconEvent_s {
  RF_Revent_t       type;  /* what kind of event has occurred */
  RF_RowCol_t       col;   /* row ID is implicit in the queue in which the event is placed */
  void             *arg;   /* a generic argument */
  RF_ReconEvent_t  *next;
};

/*
 * Reconstruction control information maintained per-disk
 * (for surviving disks)
 */
struct RF_PerDiskReconCtrl_s {
  RF_ReconCtrl_t     *reconCtrl;
  RF_RowCol_t         row, col;              /* to make this structure self-identifying */
  RF_StripeNum_t      curPSID;               /* the next parity stripe ID to check on this disk */
  RF_HeadSepLimit_t   headSepCounter;        /* counter used to control maximum head separation */
  RF_SectorNum_t      diskOffset;            /* the offset into the indicated disk of the current PU */
  RF_ReconUnitNum_t   ru_count;              /* this counts off the recon units within each parity unit */
  RF_ReconBuffer_t   *rbuf;                  /* the recon buffer assigned to this disk */
};

/* main reconstruction control structure */
struct RF_ReconCtrl_s {
  RF_RaidReconDesc_t    *reconDesc;
  RF_RowCol_t            fcol;          /* which column has failed */
  RF_PerDiskReconCtrl_t *perDiskInfo;   /* information maintained per-disk */
  RF_ReconMap_t         *reconMap;      /* map of what has/has not been reconstructed */
  RF_RowCol_t            spareRow;      /* which of the spare disks we're using */
  RF_RowCol_t            spareCol;
  RF_StripeNum_t         lastPSID;      /* the ID of the last parity stripe we want reconstructed */
  int                    percentComplete; /* percentage completion of reconstruction */

  /* reconstruction event queue */
  RF_ReconEvent_t  *eventQueue;    /* queue of pending reconstruction events */
  RF_DECLARE_MUTEX(eq_mutex)       /* mutex for locking event queue */
  RF_DECLARE_COND(eq_cond)         /* condition variable for signalling recon events */
  int               eq_count;      /* debug only */

  /* reconstruction buffer management */
  RF_DECLARE_MUTEX(rb_mutex)             /* mutex for messing around with recon buffers */
  RF_ReconBuffer_t      *floatingRbufs;  /* available floating reconstruction buffers */
  RF_ReconBuffer_t      *committedRbufs; /* recon buffers that have been committed to some waiting disk */
  RF_ReconBuffer_t      *fullBufferList; /* full buffers waiting to be written out */
  RF_ReconBuffer_t      *priorityList;   /* full buffers that have been elevated to higher priority */
  RF_CallbackDesc_t     *bufferWaitList; /* disks that are currently blocked waiting for buffers */

  /* parity stripe status table */
  RF_PSStatusHeader_t  *pssTable;  /* stores the reconstruction status of active parity stripes */

  /* maximum-head separation control */
  RF_HeadSepLimit_t  minHeadSepCounter;  /* the minimum hs counter over all disks */
  RF_CallbackDesc_t *headSepCBList;  /* list of callbacks to be done as minPSID advances */

  /* performance monitoring */
  struct timeval    starttime;      /* recon start time */

  void (*continueFunc)(void *);     /* function to call when io returns*/
  void *continueArg;                     /* argument for Func */
};

/* the default priority for reconstruction accesses */
#define RF_IO_RECON_PRIORITY RF_IO_LOW_PRIORITY

int rf_ConfigureReconstruction(RF_ShutdownList_t **listp);

int rf_ReconstructFailedDisk(RF_Raid_t *raidPtr, RF_RowCol_t row,
	RF_RowCol_t col);

int rf_ReconstructFailedDiskBasic(RF_Raid_t *raidPtr, RF_RowCol_t row,
	RF_RowCol_t col);

int rf_ContinueReconstructFailedDisk(RF_RaidReconDesc_t *reconDesc);

int rf_ForceOrBlockRecon(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
	void (*cbFunc)(RF_Raid_t *,void *), void *cbArg);

int rf_UnblockRecon(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap);

int rf_RegisterReconDoneProc(RF_Raid_t *raidPtr, void (*proc)(RF_Raid_t *, void *), void *arg,
	RF_ReconDoneProc_t **handlep);

#endif /* !_RF__RF_RECONSTRUCT_H_ */
