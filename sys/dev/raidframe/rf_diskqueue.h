/*	$OpenBSD: rf_diskqueue.h,v 1.5 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_diskqueue.h,v 1.5 2000/02/13 04:53:57 oster Exp $	*/

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

/*****************************************************************************
 *
 * rf_diskqueue.h -- Header file for disk queues.
 *
 * See comments in rf_diskqueue.c
 *
 *****************************************************************************/


#ifndef	_RF__RF_DISKQUEUE_H_
#define	_RF__RF_DISKQUEUE_H_

#include "rf_threadstuff.h"
#include "rf_acctrace.h"
#include "rf_alloclist.h"
#include "rf_types.h"
#include "rf_etimer.h"


#if	defined(__NetBSD__)
#include "rf_netbsd.h"
#elif	defined(__OpenBSD__)
#include "rf_openbsd.h"
#endif


#define	RF_IO_NORMAL_PRIORITY	1
#define	RF_IO_LOW_PRIORITY	0

/* The data held by a disk queue entry. */
struct RF_DiskQueueData_s {
	RF_SectorNum_t	  sectorOffset;	/* Sector offset into the disk. */
	RF_SectorCount_t  numSector;	/* Number of sectors to read/write. */
	RF_IoType_t	  type;		/* Read/write/nop. */
	caddr_t		  buf;		/* Buffer pointer. */
	RF_StripeNum_t	  parityStripeID;
					/*
					 * The RAID parity stripe ID this
					 * access is for.
					 */
	RF_ReconUnitNum_t which_ru;	/* Which RU within this parity stripe */
	int		  priority;	/* The priority of this request. */
	int		(*CompleteFunc) (void *, int);
					/*
					 * Function to be called upon
					 * completion.
					 */
	int		(*AuxFunc) (void *,...);
					/*
					 * Function called upon completion
					 * of the first I/O of a Read_Op_Write
					 * pair.
					 */
	void		 *argument;	/*
					 * Argument to be passed to
					 * CompleteFunc.
					 */
	RF_Raid_t	 *raidPtr;	/* Needed for simulation. */
	RF_AccTraceEntry_t *tracerec;	/* Perf mon only. */
	RF_Etimer_t	  qtime;	/*
					 * Perf mon only - time request is
					 * in queue.
					 */
	long		  entryTime;
	RF_DiskQueueData_t *next;
	RF_DiskQueueData_t *prev;
	caddr_t		  buf2;		/* For read-op-write. */
	dev_t		  dev;		/*
					 * The device number for in-kernel
					 * version.
					 */
	RF_DiskQueue_t	 *queue;	/*
					 * The disk queue to which this req
					 * is targeted.
					 */
	RF_DiskQueueDataFlags_t flags;	/* Flags controlling operation. */

	struct proc	 *b_proc;	/*
					 * The b_proc from the original bp
					 * passed into the driver for this I/O.
					 */
	struct buf	 *bp;		/* A bp to use to get this I/O done. */
};
#define	RF_LOCK_DISK_QUEUE	0x01
#define	RF_UNLOCK_DISK_QUEUE	0x02

/*
 * Note: "Create" returns type-specific queue header pointer cast to (void *).
 */
struct RF_DiskQueueSW_s {
	RF_DiskQueueType_t queueType;
	void		*(*Create) (RF_SectorCount_t, RF_AllocListElem_t *,
			    RF_ShutdownList_t **);
					/*
					 * Creation routine -- one call per
					 * queue in system.
					 */
	void		 (*Enqueue) (void *, RF_DiskQueueData_t *, int);
					/* Enqueue routine. */
	RF_DiskQueueData_t *(*Dequeue) (void *);
					/* Dequeue routine. */
	RF_DiskQueueData_t *(*Peek) (void *);
					/* Peek at head of queue. */

	/*
	 * The rest are optional:  they improve performance, but the driver
	 * will deal with it if they don't exist.
	 */
	int		 (*Promote) (void *, RF_StripeNum_t, RF_ReconUnitNum_t);
					/*
					 * Promotes priority of tagged
					 * accesses.
					 */
};

struct RF_DiskQueue_s {
	RF_DiskQueueSW_t  *qPtr;	/* Access point to queue functions. */
	void		  *qHdr;	/* Queue header, of whatever type. */
	RF_DECLARE_MUTEX(mutex);	/* Mutex locking data structures. */
	RF_DECLARE_COND(cond);		/*
					 * Condition variable for
					 * synchronization.
					 */
	long		   numOutstanding;
					/*
					 * Number of I/Os currently
					 * outstanding on disk.
					 */
	long		   maxOutstanding;
					/*
					 * Max number of I/Os that can be
					 * outstanding on a disk.
					 * (in-kernel only)
					 */
	int		   curPriority;	/*
					 * The priority of accs all that are
					 * currently outstanding.
					 */
	long		   queueLength;	/* Number of requests in queue. */
	RF_DiskQueueData_t *nextLockingOp;
					/*
					 * A locking op that has arrived at
					 * the head of the queue & is waiting
					 * for drainage.
					 */
	RF_DiskQueueData_t *unlockingOp;/*
					 * Used at user level to communicate
					 * unlocking op b/w user (or dag exec)
					 * & disk threads.
					 */
	int		   numWaiting;	/*
					 * Number of threads waiting on
					 * this variable.
					 * (user-level only)
					 */
	RF_DiskQueueFlags_t flags;	/* Terminate, locked. */
	RF_Raid_t	  *raidPtr;	/* Associated array. */
	dev_t		   dev;		/* Device number for kernel version. */
	RF_SectorNum_t	   last_deq_sector;
					/*
					 * Last sector number dequeued or
					 * dispatched.
					 */
	int		   row, col;	/* Debug only. */
	struct raidcinfo  *rf_cinfo;	/* Disks component info... */
};

/* No new accs allowed until queue is explicitly unlocked. */
#define	RF_DQ_LOCKED	0x02

/* Macros setting & returning information about queues and requests. */
#define	RF_QUEUE_LOCKED(_q)		((_q)->flags & RF_DQ_LOCKED)
#define	RF_QUEUE_EMPTY(_q)		(((_q)->numOutstanding == 0) &&	\
					 ((_q)->nextLockingOp == NULL) && \
					 !RF_QUEUE_LOCKED(_q))
#define	RF_QUEUE_FULL(_q)		((_q)->numOutstanding ==	\
					 (_q)->maxOutstanding)

#define	RF_LOCK_QUEUE(_q)		(_q)->flags |= RF_DQ_LOCKED
#define	RF_UNLOCK_QUEUE(_q)		(_q)->flags &= ~RF_DQ_LOCKED

#define	RF_LOCK_QUEUE_MUTEX(_q_,_wh_)	RF_LOCK_MUTEX((_q_)->mutex)
#define	RF_UNLOCK_QUEUE_MUTEX(_q_,_wh_)	RF_UNLOCK_MUTEX((_q_)->mutex)

#define	RF_LOCKING_REQ(_r)		((_r)->flags & RF_LOCK_DISK_QUEUE)
#define	RF_UNLOCKING_REQ(_r)		((_r)->flags & RF_UNLOCK_DISK_QUEUE)

/* Whether it is ok to dispatch a regular request. */
#define	RF_OK_TO_DISPATCH(_q_,_r_)					\
	(RF_QUEUE_EMPTY(_q_) ||						\
	( !RF_QUEUE_FULL(_q_) && ((_r_)->priority >= (_q_)->curPriority)))

int  rf_ConfigureDiskQueueSystem(RF_ShutdownList_t **);

void rf_TerminateDiskQueues(RF_Raid_t *);

int  rf_ConfigureDiskQueues(RF_ShutdownList_t **, RF_Raid_t *, RF_Config_t *);

void rf_DiskIOEnqueue(RF_DiskQueue_t *, RF_DiskQueueData_t *, int);

void rf_DiskIOComplete(RF_DiskQueue_t *, RF_DiskQueueData_t *, int);

int  rf_DiskIOPromote(RF_DiskQueue_t *, RF_StripeNum_t, RF_ReconUnitNum_t);

RF_DiskQueueData_t *rf_CreateDiskQueueData(RF_IoType_t, RF_SectorNum_t,
	RF_SectorCount_t, caddr_t, RF_StripeNum_t, RF_ReconUnitNum_t,
	int (*) (void *, int), void *, RF_DiskQueueData_t *,
	RF_AccTraceEntry_t *, void *, RF_DiskQueueDataFlags_t, void *);

RF_DiskQueueData_t *rf_CreateDiskQueueDataFull(RF_IoType_t, RF_SectorNum_t,
	RF_SectorCount_t, caddr_t, RF_StripeNum_t, RF_ReconUnitNum_t,
	int (*) (void *, int), void *, RF_DiskQueueData_t *,
	RF_AccTraceEntry_t *, int, int (*) (void *,...), caddr_t, void *,
	RF_DiskQueueDataFlags_t, void *);

void rf_FreeDiskQueueData(RF_DiskQueueData_t *);

int  rf_ConfigureDiskQueue(RF_Raid_t *, RF_DiskQueue_t *, RF_RowCol_t,
	RF_RowCol_t, RF_DiskQueueSW_t *, RF_SectorCount_t, dev_t, int,
	RF_ShutdownList_t **, RF_AllocListElem_t *);

#endif	/* ! _RF__RF_DISKQUEUE_H_ */
