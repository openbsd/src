/*	$OpenBSD: rf_raid.h,v 1.1 1999/01/11 14:29:41 niklas Exp $	*/
/*	$NetBSD: rf_raid.h,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
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

/**********************************************
 * rf_raid.h -- main header file for RAID driver
 **********************************************/

/*
 * :  
 * Log: rf_raid.h,v 
 * Revision 1.48  1996/08/20 22:33:54  jimz
 * make hist_diskreq a doubly-indexed array
 *
 * Revision 1.47  1996/07/15  05:40:41  jimz
 * some recon datastructure cleanup
 * better handling of multiple failures
 * added undocumented double-recon test
 *
 * Revision 1.46  1996/07/10  22:28:51  jimz
 * get rid of obsolete row statuses (dead,degraded2)
 *
 * Revision 1.45  1996/06/14  14:56:29  jimz
 * make engine threading stuff ifndef SIMULATE
 *
 * Revision 1.44  1996/06/14  14:16:54  jimz
 * move in engine node queue, atomicity control
 *
 * Revision 1.43  1996/06/12  04:41:26  jimz
 * tweaks to make genplot work with user-level driver
 * (mainly change stat collection)
 *
 * Revision 1.42  1996/06/11  10:57:17  jimz
 * add recon_done_procs, recon_done_proc_mutex
 *
 * Revision 1.41  1996/06/11  01:26:48  jimz
 * added mechanism for user-level to sync diskthread startup,
 * shutdown
 *
 * Revision 1.40  1996/06/10  14:18:58  jimz
 * move user, throughput stats into per-array structure
 *
 * Revision 1.39  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.38  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.37  1996/06/05  19:38:32  jimz
 * fixed up disk queueing types config
 * added sstf disk queueing
 * fixed exit bug on diskthreads (ref-ing bad mem)
 *
 * Revision 1.36  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.35  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.34  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.33  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.32  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.31  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.30  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.29  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.28  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.27  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.26  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.25  1996/05/02  14:57:55  jimz
 * add sectorMask
 *
 * Revision 1.24  1996/04/22  15:53:13  jimz
 * MAX_RAIDS -> NRAIDFRAME
 *
 * Revision 1.23  1995/12/14  18:39:46  jimz
 * convert to rf_types.h types
 *
 * Revision 1.22  1995/12/06  15:02:26  root
 * added copyright info
 *
 * Revision 1.21  1995/10/09  17:39:24  jimz
 * added info for tracking number of outstanding accesses
 * at user-level
 *
 * Revision 1.20  1995/09/30  20:37:46  jimz
 * added acc_totals to Raid for kernel
 *
 * Revision 1.19  1995/09/19  22:57:14  jimz
 * add cache of raidid for kernel
 *
 * Revision 1.18  1995/09/18  16:50:04  jimz
 * added RF_MAX_DISKS (for config ioctls)
 *
 * Revision 1.17  1995/09/07  19:02:31  jimz
 * mods to get raidframe to compile and link
 * in kernel environment
 *
 * Revision 1.16  1995/07/21  19:29:51  robby
 * added some info for the idler to the Raid
 *
 * Revision 1.15  1995/07/16  03:19:14  cfb
 * added cachePtr to *raidPtr
 *
 * Revision 1.14  1995/06/23  13:39:36  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#ifndef _RF__RF_RAID_H_
#define _RF__RF_RAID_H_

#ifdef _KERNEL
#define KERNEL
#endif

#include "rf_archs.h"
#include "rf_types.h"
#include "rf_threadstuff.h"

#ifdef _KERNEL
#if defined(__NetBSD__)
#include "rf_netbsd.h"
#elif defined(__OpenBSD__)
#include "rf_openbsd.h"
#endif
#endif

#ifdef KERNEL
/* XXX Needs to be added.  GO
#include <raidframe.h>
*/
#include <sys/disklabel.h>
#else /* KERNEL */
#include <stdio.h>
#include <assert.h>
#endif /* KERNEL */
#include <sys/types.h>

#include "rf_alloclist.h"
#include "rf_stripelocks.h"
#include "rf_layout.h"
#include "rf_disks.h"
#include "rf_debugMem.h"
#include "rf_diskqueue.h"
#include "rf_reconstruct.h"
#include "rf_acctrace.h"

#if RF_INCLUDE_PARITYLOGGING > 0
#include "rf_paritylog.h"
#endif /* RF_INCLUDE_PARITYLOGGING > 0 */

#define RF_MAX_DISKS 128 /* max disks per array */
#if defined(__NetBSD__) || defined(__OpenBSD__)
#define RF_DEV2RAIDID(_dev)  (DISKUNIT(_dev))
#else
#define RF_DEV2RAIDID(_dev)  (minor(_dev)>>6)     /* convert dev_t to raid id */
#endif

/*
 * Each row in the array is a distinct parity group, so
 * each has it's own status, which is one of the following.
 */
typedef enum RF_RowStatus_e {
  rf_rs_optimal,
  rf_rs_degraded,
  rf_rs_reconstructing,
  rf_rs_reconfigured
} RF_RowStatus_t;

struct RF_CumulativeStats_s {
  struct timeval start;     /* the time when the stats were last started*/
  struct timeval stop;      /* the time when the stats were last stopped */
  long sum_io_us;           /* sum of all user response times (us) */
  long num_ios;             /* total number of I/Os serviced */
  long num_sect_moved;      /* total number of sectors read or written */
};

struct RF_ThroughputStats_s {
  RF_DECLARE_MUTEX(mutex)/* a mutex used to lock the configuration stuff */
  struct timeval start;  /* timer started when numOutstandingRequests moves from 0 to 1 */
  struct timeval stop;   /* timer stopped when numOutstandingRequests moves from 1 to 0 */
  RF_uint64 sum_io_us;   /* total time timer is enabled */
  RF_uint64 num_ios;     /* total number of ios processed by RAIDframe */
  long num_out_ios;      /* number of outstanding ios */
};

#ifdef SIMULATE
typedef struct RF_PendingRecon_s RF_PendingRecon_t;
struct RF_PendingRecon_s {
  RF_RowCol_t         row;
  RF_RowCol_t         col;
  RF_PendingRecon_t  *next;
};
#endif /* SIMULATE */

struct RF_Raid_s {
  /* This portion never changes, and can be accessed without locking */
  /* an exception is Disks[][].status, which requires locking when it is changed */
  u_int numRow;             /* number of rows of disks, typically == # of ranks */
  u_int numCol;             /* number of columns of disks, typically == # of disks/rank */
  u_int numSpare;           /* number of spare disks */
  int   maxQueueDepth;      /* max disk queue depth */
  RF_SectorCount_t  totalSectors;   /* total number of sectors in the array */
  RF_SectorCount_t  sectorsPerDisk; /* number of sectors on each disk */
  u_int logBytesPerSector;  /* base-2 log of the number of bytes in a sector */
  u_int bytesPerSector;     /* bytes in a sector */
  RF_int32  sectorMask;     /* mask of bytes-per-sector */

  RF_RaidLayout_t   Layout; /* all information related to layout */
  RF_RaidDisk_t   **Disks;  /* all information related to physical disks */
  RF_DiskQueue_t  **Queues; /* all information related to disk queues */
     /* NOTE:  This is an anchor point via which the queues can be accessed,
      * but the enqueue/dequeue routines in diskqueue.c use a local copy of
      * this pointer for the actual accesses.
      */
  /* The remainder of the structure can change, and therefore requires locking on reads and updates */
  RF_DECLARE_MUTEX(mutex)        /* mutex used to serialize access to the fields below */
  RF_RowStatus_t  *status;       /* the status of each row in the array */
  int              valid;        /* indicates successful configuration */
  RF_LockTableEntry_t *lockTable;   /* stripe-lock table */
  RF_LockTableEntry_t *quiesceLock; /* quiesnce table */
  int                  numFailures; /* total number of failures in the array */

  /*
   * Cleanup stuff
   */
  RF_ShutdownList_t  *shutdownList; /* shutdown activities */
  RF_AllocListElem_t *cleanupList;  /* memory to be freed at shutdown time */

  /*
   * Recon stuff
   */
  RF_HeadSepLimit_t headSepLimit;
  int numFloatingReconBufs;
  int reconInProgress;
#ifdef SIMULATE
  RF_PendingRecon_t *pendingRecon;
#endif /* SIMULATE */
  RF_DECLARE_COND(waitForReconCond)
  RF_RaidReconDesc_t *reconDesc; /* reconstruction descriptor */
  RF_ReconCtrl_t **reconControl; /* reconstruction control structure pointers for each row in the array */

#if !defined(KERNEL) && !defined(SIMULATE)
  /*
   * Disk thread stuff
   */
  int diskthreads_created;
  int diskthreads_running;
  int diskthreads_shutdown;
  RF_DECLARE_MUTEX(diskthread_count_mutex)
  RF_DECLARE_COND(diskthread_count_cond)
#endif /* !KERNEL && !SIMULATE */

  /*
   * Array-quiescence stuff
   */
  RF_DECLARE_MUTEX(access_suspend_mutex)
  RF_DECLARE_COND(quiescent_cond)
  RF_IoCount_t accesses_suspended;
  RF_IoCount_t accs_in_flight;
  int access_suspend_release;
  int waiting_for_quiescence;
  RF_CallbackDesc_t *quiesce_wait_list;

  /*
   * Statistics
   */
#if !defined(KERNEL) && !defined(SIMULATE)
  RF_ThroughputStats_t throughputstats;
#endif /* !KERNEL && !SIMULATE */
  RF_CumulativeStats_t userstats;

  /*
   * Engine thread control
   */
  RF_DECLARE_MUTEX(node_queue_mutex)
  RF_DECLARE_COND(node_queue_cond)
  RF_DagNode_t *node_queue;
#ifndef SIMULATE
  RF_Thread_t engine_thread;
  RF_ThreadGroup_t engine_tg;
#endif /* !SIMULATE */
  int shutdown_engine;
  int dags_in_flight; /* debug */

  /*
   * PSS (Parity Stripe Status) stuff
   */
  RF_FreeList_t *pss_freelist;
  long pssTableSize;

  /*
   * Reconstruction stuff
   */
  int procsInBufWait;
  int numFullReconBuffers;
  RF_AccTraceEntry_t *recon_tracerecs;
  unsigned long accumXorTimeUs;
  RF_ReconDoneProc_t *recon_done_procs;
  RF_DECLARE_MUTEX(recon_done_proc_mutex)

#if !defined(KERNEL) && !defined(SIMULATE)
  RF_Thread_t **diskthreads, *sparediskthreads;  /* thread descriptors for disk threads in user-level version */
#endif /* !KERNEL && !SIMULATE */

  /*
   * nAccOutstanding, waitShutdown protected by desc freelist lock
   * (This may seem strange, since that's a central serialization point
   * for a per-array piece of data, but otherwise, it'd be an extra
   * per-array lock, and that'd only be less efficient...)
   */
  RF_DECLARE_COND(outstandingCond)
  int waitShutdown;
  int nAccOutstanding;

  RF_DiskId_t **diskids;
  RF_DiskId_t  *sparediskids;

#ifdef KERNEL
	int           raidid;
#endif /* KERNEL */
	RF_AccTotals_t  acc_totals;
	int           keep_acc_totals;

#ifdef _KERNEL
        struct raidcinfo **raid_cinfo; /* array of component info */
        struct proc *proc; /* XXX shouldn't be needed here.. :-p */
#endif

  int terminate_disk_queues;

  /*
   * XXX
   *
   * config-specific information should be moved
   * somewhere else, or at least hung off this
   * in some generic way
   */

  /* used by rf_compute_workload_shift */
  RF_RowCol_t hist_diskreq[RF_MAXROW][RF_MAXCOL];

  /* used by declustering */
  int noRotate;

#if RF_INCLUDE_PARITYLOGGING > 0
  /* used by parity logging */
  RF_SectorCount_t          regionLogCapacity;
  RF_ParityLogQueue_t       parityLogPool;       /* pool of unused parity logs */
  RF_RegionInfo_t          *regionInfo;          /* array of region state */
  int                       numParityLogs;
  int                       numSectorsPerLog;
  int                       regionParityRange;
  int                       logsInUse;           /* debugging */
  RF_ParityLogDiskQueue_t   parityLogDiskQueue;  /* state of parity logging disk work */
  RF_RegionBufferQueue_t    regionBufferPool;    /* buffers for holding region log */
  RF_RegionBufferQueue_t    parityBufferPool;    /* buffers for holding parity */
  caddr_t                   parityLogBufferHeap; /* pool of unused parity logs */
#ifndef SIMULATE
  RF_Thread_t               pLogDiskThreadHandle;
#endif /* !SIMULATE */

#endif /* RF_INCLUDE_PARITYLOGGING > 0 */
};

#endif /* !_RF__RF_RAID_H_ */
