/*	$OpenBSD: rf_desc.h,v 1.1 1999/01/11 14:29:15 niklas Exp $	*/
/*	$NetBSD: rf_desc.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
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
 * :  
 * Log: rf_desc.h,v 
 * Revision 1.29  1996/07/22 19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.28  1996/06/07  22:49:22  jimz
 * fix up raidPtr typing
 *
 * Revision 1.27  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.26  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.25  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.24  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.23  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.22  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.21  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.20  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.19  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.18  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.17  1995/12/01  15:58:43  root
 * added copyright info
 *
 * Revision 1.16  1995/11/19  16:31:30  wvcii
 * descriptors now contain an array of dag lists as opposed to a dag header
 *
 * Revision 1.15  1995/11/07  16:24:17  wvcii
 * updated def of _AccessState
 *
 */

#ifndef _RF__RF_DESC_H_
#define _RF__RF_DESC_H_

#include "rf_archs.h"
#include "rf_types.h"
#include "rf_etimer.h"
#include "rf_dag.h"

struct RF_RaidReconDesc_s {
  RF_Raid_t           *raidPtr;      /* raid device descriptor */
  RF_RowCol_t          row;          /* row of failed disk */
  RF_RowCol_t          col;          /* col of failed disk */
  int                  state;        /* how far along the reconstruction operation has gotten */
  RF_RaidDisk_t       *spareDiskPtr; /* describes target disk for recon (not used in dist sparing) */
  int                  numDisksDone; /* the number of surviving disks that have completed their work */
  RF_RowCol_t          srow;         /* row ID of the spare disk (not used in dist sparing) */
  RF_RowCol_t          scol;         /* col ID of the spare disk (not used in dist sparing) */
#ifdef KERNEL
  /*
   * Prevent recon from hogging CPU
   */
  RF_Etimer_t          recon_exec_timer;
  RF_uint64            reconExecTimerRunning;
  RF_uint64            reconExecTicks;
  RF_uint64            maxReconExecTicks;
#endif /* KERNEL */

#if RF_RECON_STATS > 0
  RF_uint64            hsStallCount;       /* head sep stall count */
  RF_uint64            numReconExecDelays;
  RF_uint64            numReconEventWaits;
#endif /* RF_RECON_STATS > 0 */
  RF_RaidReconDesc_t  *next;
};

struct RF_RaidAccessDesc_s {
  RF_Raid_t              *raidPtr;          /* raid device descriptor */
  RF_IoType_t             type;             /* read or write */
  RF_RaidAddr_t           raidAddress;      /* starting address in raid address space */
  RF_SectorCount_t        numBlocks;        /* number of blocks (sectors) to transfer */
  RF_StripeCount_t        numStripes;       /* number of stripes involved in access */
  caddr_t                 bufPtr;           /* pointer to data buffer */

#if !defined(KERNEL) && !defined(SIMULATE)
  caddr_t                 obufPtr;          /* real pointer to data buffer */
#endif /* !KERNEL && !SIMULATE */

  RF_RaidAccessFlags_t    flags;            /* flags controlling operation */
  int                     state;            /* index into states telling how far along the RAID operation has gotten */
  RF_AccessState_t       *states;	        /* array of states to be run */
  int                     status;           /* pass/fail status of the last operation */
  RF_DagList_t           *dagArray;         /* array of dag lists, one list per stripe */
  RF_AccessStripeMapHeader_t  *asmap;       /* the asm for this I/O */
  void                   *bp;               /* buf pointer for this RAID acc.  ignored outside the kernel */
  RF_DagHeader_t        **paramDAG;         /* allows the DAG to be returned to the caller after I/O completion */
  RF_AccessStripeMapHeader_t **paramASM;         /* allows the ASM to be returned to the caller after I/O completion */
  RF_AccTraceEntry_t      tracerec;         /* perf monitoring information for a user access (not for dag stats) */
  void                  (*callbackFunc)(RF_CBParam_t);  /* callback function for this I/O */
  void                   *callbackArg;      /* arg to give to callback func */
  int                    tid;               /* debug only, user-level only: thread id of thr that did this access */

  RF_AllocListElem_t    *cleanupList;       /* memory to be freed at the end of the access*/

  RF_RaidAccessDesc_t         *next;
  RF_RaidAccessDesc_t         *head;

  int numPending;

  RF_DECLARE_MUTEX(mutex)   /* these are used to implement blocking I/O */
  RF_DECLARE_COND(cond)

#ifdef SIMULATE
  RF_Owner_t  owner;                            
  int         async_flag;
#endif /* SIMULATE */

  RF_Etimer_t                 timer;            /* used for timing this access */
};

#endif /* !_RF__RF_DESC_H_ */
