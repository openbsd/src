/*	$OpenBSD: rf_diskqueue.h,v 1.1 1999/01/11 14:29:17 niklas Exp $	*/
/*	$NetBSD: rf_diskqueue.h,v 1.1 1998/11/13 04:20:29 oster Exp $	*/
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

/*****************************************************************************************
 *
 * rf_diskqueue.h -- header file for disk queues
 *
 * see comments in rf_diskqueue.c
 *
 ****************************************************************************************/
/*
 *
 * :  
 *
 * Log: rf_diskqueue.h,v 
 * Revision 1.31  1996/08/07 21:08:49  jimz
 * b_proc -> kb_proc (IRIX complained)
 *
 * Revision 1.30  1996/06/18  20:53:11  jimz
 * fix up disk queueing (remove configure routine,
 * add shutdown list arg to create routines)
 *
 * Revision 1.29  1996/06/13  20:38:19  jimz
 * fix queue type in DiskQueueData
 *
 * Revision 1.28  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.27  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.26  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.25  1996/06/06  17:29:12  jimz
 * track arm position of last I/O dequeued
 *
 * Revision 1.24  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.23  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.22  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.21  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.20  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.19  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.18  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.17  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.16  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.15  1996/05/10  19:39:31  jimz
 * add prev pointer to DiskQueueData
 *
 * Revision 1.14  1996/05/10  16:24:04  jimz
 * mark old defines as deprecated, add RF_ defines
 *
 * Revision 1.13  1995/12/01  15:59:04  root
 * added copyright info
 *
 * Revision 1.12  1995/11/07  16:26:44  wvcii
 * added Peek() function to diskqueuesw
 *
 * Revision 1.11  1995/10/05  02:33:15  jimz
 * made queue lens longs (less instructions to read :-)
 *
 * Revision 1.10  1995/10/04  07:07:07  wvcii
 * queue->numOutstanding now valid for user & sim
 * user tested & verified, sim untested
 *
 * Revision 1.9  1995/09/12  00:21:37  wvcii
 * added support for tracing disk queue time
 *
 * Revision 1.8  95/04/24  13:25:51  holland
 * rewrite to move disk queues, recon, & atomic RMW to kernel
 * 
 * Revision 1.6.10.2  1995/04/03  20:13:56  holland
 * added numOutstanding and maxOutstanding to support moving
 * disk queues into kernel code
 *
 * Revision 1.6.10.1  1995/04/03  20:03:56  holland
 * initial checkin on branch
 *
 * Revision 1.6  1995/03/03  18:34:33  rachad
 * Simulator mechanism added
 *
 * Revision 1.5  1995/03/01  20:25:48  holland
 * kernelization changes
 *
 * Revision 1.4  1995/02/03  22:31:36  holland
 * many changes related to kernelization
 *
 * Revision 1.3  1995/02/01  14:25:19  holland
 * began changes for kernelization:
 *      changed all instances of mutex_t and cond_t to DECLARE macros
 *      converted configuration code to use config structure
 *
 * Revision 1.2  1994/11/29  20:36:02  danner
 * Added symbolic constants for io_type (e.g,IO_TYPE_READ)
 * and support for READ_OP_WRITE
 *
 */


#ifndef _RF__RF_DISKQUEUE_H_
#define _RF__RF_DISKQUEUE_H_

#include "rf_threadstuff.h"
#include "rf_acctrace.h"
#include "rf_alloclist.h"
#include "rf_types.h"
#include "rf_etimer.h"


#ifdef _KERNEL
#if defined(__NetBSD__)
#include "rf_netbsd.h"
#elif defined(__OpenBSD__)
#include "rf_openbsd.h"
#endif
#endif


#define RF_IO_NORMAL_PRIORITY 1
#define RF_IO_LOW_PRIORITY    0

/* the data held by a disk queue entry */
struct RF_DiskQueueData_s {
  RF_SectorNum_t           sectorOffset;   /* sector offset into the disk */
  RF_SectorCount_t         numSector;      /* number of sectors to read/write */
  RF_IoType_t              type;           /* read/write/nop */
  caddr_t                  buf;            /* buffer pointer */
  RF_StripeNum_t           parityStripeID; /* the RAID parity stripe ID this access is for */
  RF_ReconUnitNum_t        which_ru;       /* which RU within this parity stripe */
  int                      priority;       /* the priority of this request */
  int                    (*CompleteFunc)(void *,int);/* function to be called upon completion */
  int                    (*AuxFunc)(void *,...); /* function called upon completion of the first I/O of a Read_Op_Write pair*/
  void                    *argument;       /* argument to be passed to CompleteFunc */
#ifdef SIMULATE
  RF_Owner_t               owner;          /* which task is responsible for this request */
#endif /* SIMULATE */
  void                    *raidPtr;        /* needed for simulation */
  RF_AccTraceEntry_t      *tracerec;       /* perf mon only */
  RF_Etimer_t              qtime;          /* perf mon only - time request is in queue */
  long                     entryTime;
  RF_DiskQueueData_t      *next;
  RF_DiskQueueData_t      *prev;
  caddr_t                  buf2;   /* for read-op-write */
  dev_t                    dev;    /* the device number for in-kernel version */
  RF_DiskQueue_t          *queue;  /* the disk queue to which this req is targeted */
  RF_DiskQueueDataFlags_t  flags;  /* flags controlling operation */
  
#ifdef KERNEL
  struct proc             *b_proc;  /* the b_proc from the original bp passed into the driver for this I/O */
  struct buf              *bp;      /* a bp to use to get this I/O done */
#endif /* KERNEL */
};

#define RF_LOCK_DISK_QUEUE   0x01
#define RF_UNLOCK_DISK_QUEUE 0x02

/* note: "Create" returns type-specific queue header pointer cast to (void *) */
struct RF_DiskQueueSW_s {
  RF_DiskQueueType_t     queueType;
  void                *(*Create)(RF_SectorCount_t, RF_AllocListElem_t *, RF_ShutdownList_t **);    /* creation routine -- one call per queue in system */
  void                 (*Enqueue)(void *,RF_DiskQueueData_t * ,int);   /* enqueue routine */
  RF_DiskQueueData_t  *(*Dequeue)(void *);   /* dequeue routine */
  RF_DiskQueueData_t  *(*Peek)(void *);      /* peek at head of queue */

  /* the rest are optional:  they improve performance, but the driver will deal with it if they don't exist */
  int                  (*Promote)(void *, RF_StripeNum_t, RF_ReconUnitNum_t);   /* promotes priority of tagged accesses */
};

struct RF_DiskQueue_s {
  RF_DiskQueueSW_t    *qPtr;             /* access point to queue functions */
  void                *qHdr;             /* queue header, of whatever type */
  RF_DECLARE_MUTEX(mutex)                /* mutex locking data structures */
  RF_DECLARE_COND(cond)                  /* condition variable for synchronization */
  long                 numOutstanding;   /* number of I/Os currently outstanding on disk */
  long                 maxOutstanding;   /* max # of I/Os that can be outstanding on a disk (in-kernel only) */
  int                  curPriority;      /* the priority of accs all that are currently outstanding */
  long                 queueLength;      /* number of requests in queue */
  RF_DiskQueueData_t  *nextLockingOp;    /* a locking op that has arrived at the head of the queue & is waiting for drainage */
  RF_DiskQueueData_t  *unlockingOp;      /* used at user level to communicate unlocking op b/w user (or dag exec) & disk threads */
  int                  numWaiting;       /* number of threads waiting on this variable.  user-level only */
  RF_DiskQueueFlags_t  flags;            /* terminate, locked */
  RF_Raid_t           *raidPtr;          /* associated array */
  dev_t                dev;              /* device number for kernel version */
  RF_SectorNum_t       last_deq_sector;  /* last sector number dequeued or dispatched */
  int                  row, col;         /* debug only */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  struct raidcinfo *rf_cinfo;      /* disks component info.. */
#endif
};

#define RF_DQ_LOCKED  0x02  /* no new accs allowed until queue is explicitly unlocked */

/* macros setting & returning information about queues and requests */
#define RF_QUEUE_LOCKED(_q)                 ((_q)->flags & RF_DQ_LOCKED)
#define RF_QUEUE_EMPTY(_q)                  (((_q)->numOutstanding == 0) && ((_q)->nextLockingOp == NULL) && !RF_QUEUE_LOCKED(_q))
#define RF_QUEUE_FULL(_q)                   ((_q)->numOutstanding == (_q)->maxOutstanding)

#define RF_LOCK_QUEUE(_q)                   (_q)->flags |= RF_DQ_LOCKED
#define RF_UNLOCK_QUEUE(_q)                 (_q)->flags &= ~RF_DQ_LOCKED

#define RF_LOCK_QUEUE_MUTEX(_q_,_wh_)   RF_LOCK_MUTEX((_q_)->mutex)
#define RF_UNLOCK_QUEUE_MUTEX(_q_,_wh_) RF_UNLOCK_MUTEX((_q_)->mutex)

#define RF_LOCKING_REQ(_r)                  ((_r)->flags & RF_LOCK_DISK_QUEUE)
#define RF_UNLOCKING_REQ(_r)                ((_r)->flags & RF_UNLOCK_DISK_QUEUE)

/* whether it is ok to dispatch a regular request */
#define RF_OK_TO_DISPATCH(_q_,_r_) \
  (RF_QUEUE_EMPTY(_q_) || \
    (!RF_QUEUE_FULL(_q_) && ((_r_)->priority >= (_q_)->curPriority)))

int rf_ConfigureDiskQueueSystem(RF_ShutdownList_t **listp);

void rf_TerminateDiskQueues(RF_Raid_t *raidPtr);

int rf_ConfigureDiskQueues(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);

void rf_DiskIOEnqueue(RF_DiskQueue_t *queue, RF_DiskQueueData_t *req, int pri);

#if !defined(KERNEL) && !defined(SIMULATE)
void rf_BroadcastOnQueue(RF_DiskQueue_t *queue);
#endif /* !KERNEL && !SIMULATE */

#ifndef KERNEL
RF_DiskQueueData_t *rf_DiskIODequeue(RF_DiskQueue_t *queue);
#else /* !KERNEL */
void rf_DiskIOComplete(RF_DiskQueue_t *queue, RF_DiskQueueData_t *req, int status);
#endif /* !KERNEL */

int rf_DiskIOPromote(RF_DiskQueue_t *queue, RF_StripeNum_t parityStripeID,
	RF_ReconUnitNum_t which_ru);

RF_DiskQueueData_t *rf_CreateDiskQueueData(RF_IoType_t typ,
	RF_SectorNum_t ssect, RF_SectorCount_t nsect, caddr_t buf,
	RF_StripeNum_t parityStripeID, RF_ReconUnitNum_t which_ru, 
        int (*wakeF)(void *, int),
	void *arg, RF_DiskQueueData_t *next, RF_AccTraceEntry_t *tracerec,
	void *raidPtr, RF_DiskQueueDataFlags_t flags, void *kb_proc);

RF_DiskQueueData_t *rf_CreateDiskQueueDataFull(RF_IoType_t typ,
	RF_SectorNum_t ssect, RF_SectorCount_t nsect, caddr_t buf,
	RF_StripeNum_t parityStripeID, RF_ReconUnitNum_t which_ru, 
        int (*wakeF)(void *, int),
	void *arg, RF_DiskQueueData_t *next, RF_AccTraceEntry_t *tracerec,
	int priority, int (*AuxFunc)(void *,...), caddr_t buf2,
	void *raidPtr, RF_DiskQueueDataFlags_t flags, void *kb_proc);

void rf_FreeDiskQueueData(RF_DiskQueueData_t *p);

#endif /* !_RF__RF_DISKQUEUE_H_ */
