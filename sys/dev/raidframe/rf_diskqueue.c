/*	$OpenBSD: rf_diskqueue.c,v 1.1 1999/01/11 14:29:17 niklas Exp $	*/
/*	$NetBSD: rf_diskqueue.c,v 1.2 1998/12/03 14:58:24 oster Exp $	*/
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

/****************************************************************************************
 *
 * rf_diskqueue.c -- higher-level disk queue code
 *
 * the routines here are a generic wrapper around the actual queueing
 * routines.  The code here implements thread scheduling, synchronization, 
 * and locking ops (see below) on top of the lower-level queueing code.
 *
 * to support atomic RMW, we implement "locking operations".  When a locking op
 * is dispatched to the lower levels of the driver, the queue is locked, and no further
 * I/Os are dispatched until the queue receives & completes a corresponding "unlocking
 * operation".  This code relies on the higher layers to guarantee that a locking
 * op will always be eventually followed by an unlocking op.  The model is that
 * the higher layers are structured so locking and unlocking ops occur in pairs, i.e.
 * an unlocking op cannot be generated until after a locking op reports completion.
 * There is no good way to check to see that an unlocking op "corresponds" to the
 * op that currently has the queue locked, so we make no such attempt.  Since by
 * definition there can be only one locking op outstanding on a disk, this should
 * not be a problem.
 *
 * In the kernel, we allow multiple I/Os to be concurrently dispatched to the disk
 * driver.  In order to support locking ops in this environment, when we decide to
 * do a locking op, we stop dispatching new I/Os and wait until all dispatched I/Os
 * have completed before dispatching the locking op.
 *
 * Unfortunately, the code is different in the 3 different operating states
 * (user level, kernel, simulator).  In the kernel, I/O is non-blocking, and
 * we have no disk threads to dispatch for us.  Therefore, we have to dispatch
 * new I/Os to the scsi driver at the time of enqueue, and also at the time 
 * of completion.  At user level, I/O is blocking, and so only the disk threads 
 * may dispatch I/Os.  Thus at user level, all we can do at enqueue time is 
 * enqueue and wake up the disk thread to do the dispatch.
 *
 ***************************************************************************************/

/*
 * :  
 *
 * Log: rf_diskqueue.c,v 
 * Revision 1.50  1996/08/07 21:08:38  jimz
 * b_proc -> kb_proc
 *
 * Revision 1.49  1996/07/05  20:36:14  jimz
 * make rf_ConfigureDiskQueueSystem return 0
 *
 * Revision 1.48  1996/06/18  20:53:11  jimz
 * fix up disk queueing (remove configure routine,
 * add shutdown list arg to create routines)
 *
 * Revision 1.47  1996/06/14  14:16:36  jimz
 * fix handling of bogus queue type
 *
 * Revision 1.46  1996/06/13  20:41:44  jimz
 * add scan, cscan, random queueing
 *
 * Revision 1.45  1996/06/11  01:27:50  jimz
 * Fixed bug where diskthread shutdown would crash or hang. This
 * turned out to be two distinct bugs:
 * (1) [crash] The thread shutdown code wasn't properly waiting for
 * all the diskthreads to complete. This caused diskthreads that were
 * exiting+cleaning up to unlock a destroyed mutex.
 * (2) [hang] TerminateDiskQueues wasn't locking, and DiskIODequeue
 * only checked for termination _after_ a wakeup if the queues were
 * empty. This was a race where the termination wakeup could be lost
 * by the dequeueing thread, and the system would hang waiting for the
 * thread to exit, while the thread waited for an I/O or a signal to
 * check the termination flag.
 *
 * Revision 1.44  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.43  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.42  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.41  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.40  1996/06/06  17:28:04  jimz
 * track sector number of last I/O dequeued
 *
 * Revision 1.39  1996/06/06  01:14:13  jimz
 * fix crashing bug when tracerec is NULL (ie, from copyback)
 * initialize req->queue
 *
 * Revision 1.38  1996/06/05  19:38:32  jimz
 * fixed up disk queueing types config
 * added sstf disk queueing
 * fixed exit bug on diskthreads (ref-ing bad mem)
 *
 * Revision 1.37  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.36  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.35  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.34  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.33  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.32  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.31  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.30  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.29  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.28  1996/05/20  16:14:29  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.27  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.26  1996/05/16  19:21:49  wvcii
 * fixed typo in init_dqd
 *
 * Revision 1.25  1996/05/16  16:02:51  jimz
 * switch to RF_FREELIST stuff for DiskQueueData
 *
 * Revision 1.24  1996/05/10  16:24:14  jimz
 * new cvscan function names
 *
 * Revision 1.23  1996/05/01  16:27:54  jimz
 * don't use ccmn bp management
 *
 * Revision 1.22  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.21  1995/12/01  15:59:59  root
 * added copyright info
 *
 * Revision 1.20  1995/11/07  16:27:20  wvcii
 * added Peek() function to diskqueuesw
 * non-locking accesses are never blocked (assume clients enforce proper
 * respect for lock acquisition)
 *
 * Revision 1.19  1995/10/05  18:56:52  jimz
 * fix req handling in IOComplete
 *
 * Revision 1.18  1995/10/04  20:13:50  wvcii
 * added asserts to monitor numOutstanding queueLength
 *
 * Revision 1.17  1995/10/04  07:43:52  wvcii
 * queue->numOutstanding now valid for user & sim
 * added queue->queueLength
 * user tested & verified, sim untested
 *
 * Revision 1.16  1995/09/12  00:21:19  wvcii
 * added support for tracing disk queue time
 *
 */

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_threadid.h"
#include "rf_raid.h"
#include "rf_diskqueue.h"
#include "rf_alloclist.h"
#include "rf_acctrace.h"
#include "rf_etimer.h"
#include "rf_configure.h"
#include "rf_general.h"
#include "rf_freelist.h"
#include "rf_debugprint.h"
#include "rf_shutdown.h"
#include "rf_cvscan.h"
#include "rf_sstf.h"
#include "rf_fifo.h"

#ifdef SIMULATE
#include "rf_diskevent.h"
#endif /* SIMULATE */

#if !defined(__NetBSD__) && !defined(__OpenBSD__)
extern struct buf *ubc_bufget();
#endif

static int init_dqd(RF_DiskQueueData_t *);
static void clean_dqd(RF_DiskQueueData_t *);
static void rf_ShutdownDiskQueueSystem(void *);
/* From rf_kintf.c */
int rf_DispatchKernelIO(RF_DiskQueue_t *,RF_DiskQueueData_t *);


#define Dprintf1(s,a)         if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf2(s,a,b)       if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf3(s,a,b,c)     if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),NULL,NULL,NULL,NULL,NULL)
#define Dprintf4(s,a,b,c,d)   if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),NULL,NULL,NULL,NULL)
#define Dprintf5(s,a,b,c,d,e) if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),NULL,NULL,NULL)

#if !defined(KERNEL) && !defined(SIMULATE)

/* queue must be locked before invoking this */
#define SIGNAL_DISK_QUEUE(_q_,_wh_)  \
{                                    \
  if ( (_q_)->numWaiting > 0) {      \
    (_q_)->numWaiting--;             \
    RF_SIGNAL_COND( ((_q_)->cond) );    \
  }                                  \
}

/* queue must be locked before invoking this */
#define WAIT_DISK_QUEUE(_q_,_wh_)                                         \
{                                                                         \
  (_q_)->numWaiting++;                                                    \
  RF_WAIT_COND( ((_q_)->cond), ((_q_)->mutex) );                             \
}

#else /* !defined(KERNEL) && !defined(SIMULATE) */

#define SIGNAL_DISK_QUEUE(_q_,_wh_)
#define WAIT_DISK_QUEUE(_q_,_wh_)

#endif /* !defined(KERNEL) && !defined(SIMULATE) */

/*****************************************************************************************
 *
 * the disk queue switch defines all the functions used in the different queueing
 * disciplines
 *    queue ID, init routine, enqueue routine, dequeue routine
 *
 ****************************************************************************************/

static RF_DiskQueueSW_t diskqueuesw[] = {
	{"fifo", /* FIFO */
	rf_FifoCreate,
	rf_FifoEnqueue,
	rf_FifoDequeue,
	rf_FifoPeek,
	rf_FifoPromote},

	{"cvscan", /* cvscan */
	rf_CvscanCreate,
	rf_CvscanEnqueue,
	rf_CvscanDequeue,
	rf_CvscanPeek,
	rf_CvscanPromote },

	{"sstf", /* shortest seek time first */
	rf_SstfCreate,
	rf_SstfEnqueue,
	rf_SstfDequeue,
	rf_SstfPeek,
	rf_SstfPromote},

	{"scan", /* SCAN (two-way elevator) */
	rf_ScanCreate,
	rf_SstfEnqueue,
	rf_ScanDequeue,
	rf_ScanPeek,
	rf_SstfPromote},

	{"cscan", /* CSCAN (one-way elevator) */
	rf_CscanCreate,
	rf_SstfEnqueue,
	rf_CscanDequeue,
	rf_CscanPeek,
	rf_SstfPromote},

#if !defined(KERNEL) && RF_INCLUDE_QUEUE_RANDOM > 0
	/* to make a point to Chris :-> */
	{"random", /* random */
	rf_FifoCreate,
	rf_FifoEnqueue,
	rf_RandomDequeue,
	rf_RandomPeek,
	rf_FifoPromote},
#endif /* !KERNEL && RF_INCLUDE_QUEUE_RANDOM > 0 */
};
#define NUM_DISK_QUEUE_TYPES (sizeof(diskqueuesw)/sizeof(RF_DiskQueueSW_t))

static RF_FreeList_t *rf_dqd_freelist;

#define RF_MAX_FREE_DQD 256
#define RF_DQD_INC       16
#define RF_DQD_INITIAL   64

#if defined(__NetBSD__) || defined(__OpenBSD__)
#ifdef _KERNEL
#include <sys/buf.h>
#endif
#endif

static int init_dqd(dqd)
  RF_DiskQueueData_t  *dqd;
{
#ifdef KERNEL
#if defined(__NetBSD__) || defined(__OpenBSD__)
	/* XXX not sure if the following malloc is appropriate... probably not quite... */
	dqd->bp = (struct buf *) malloc( sizeof(struct buf), M_DEVBUF, M_NOWAIT);
	memset(dqd->bp,0,sizeof(struct buf)); /* if you don't do it, nobody else will.. */
	/* XXX */
	/* printf("NEED TO IMPLEMENT THIS BETTER!\n"); */
#else
	dqd->bp = ubc_bufget();
#endif
	if (dqd->bp == NULL) {
		return(ENOMEM);
	}
#endif /* KERNEL */
	return(0);
}

static void clean_dqd(dqd)
  RF_DiskQueueData_t  *dqd;
{
#ifdef KERNEL
#if defined(__NetBSD__) || defined(__OpenBSD__)
	/* printf("NEED TO IMPLEMENT THIS BETTER(2)!\n"); */
	/* XXX ? */
	free( dqd->bp, M_DEVBUF );
#else
    ubc_buffree(dqd->bp);
#endif

#endif /* KERNEL */
}

/* configures a single disk queue */
static int config_disk_queue(
  RF_Raid_t            *raidPtr,
  RF_DiskQueue_t       *diskqueue,
  RF_RowCol_t           r, /* row & col -- debug only.  BZZT not any more... */
  RF_RowCol_t           c,
  RF_DiskQueueSW_t     *p,
  RF_SectorCount_t      sectPerDisk,
  dev_t                 dev,
  int                   maxOutstanding,
  RF_ShutdownList_t   **listp,
  RF_AllocListElem_t   *clList)
{
  int rc;

  diskqueue->row = r;
  diskqueue->col = c;
  diskqueue->qPtr = p;
  diskqueue->qHdr = (p->Create)(sectPerDisk, clList, listp);
  diskqueue->dev  = dev;
  diskqueue->numOutstanding = 0;
  diskqueue->queueLength = 0;
  diskqueue->maxOutstanding = maxOutstanding;
  diskqueue->curPriority    = RF_IO_NORMAL_PRIORITY;
  diskqueue->nextLockingOp  = NULL;
  diskqueue->unlockingOp    = NULL;
  diskqueue->numWaiting=0;
  diskqueue->flags = 0;
  diskqueue->raidPtr = raidPtr;
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  diskqueue->rf_cinfo = &raidPtr->raid_cinfo[r][c];
#endif
  rc = rf_create_managed_mutex(listp, &diskqueue->mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(rc);
  }
  rc = rf_create_managed_cond(listp, &diskqueue->cond);
  if (rc) {
    RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(rc);
  }
  return(0);
}

static void rf_ShutdownDiskQueueSystem(ignored)
  void  *ignored;
{
  RF_FREELIST_DESTROY_CLEAN(rf_dqd_freelist,next,(RF_DiskQueueData_t *),clean_dqd);
}

int rf_ConfigureDiskQueueSystem(listp)
  RF_ShutdownList_t  **listp;
{
  int rc;

  RF_FREELIST_CREATE(rf_dqd_freelist, RF_MAX_FREE_DQD,
    RF_DQD_INC, sizeof(RF_DiskQueueData_t));
  if (rf_dqd_freelist == NULL)
    return(ENOMEM);
  rc = rf_ShutdownCreate(listp, rf_ShutdownDiskQueueSystem, NULL);
  if (rc) {
    RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
      __FILE__, __LINE__, rc);
    rf_ShutdownDiskQueueSystem(NULL);
    return(rc);
  }
  RF_FREELIST_PRIME_INIT(rf_dqd_freelist, RF_DQD_INITIAL,next,
    (RF_DiskQueueData_t *),init_dqd);
  return(0);
}

#ifndef KERNEL
/* this is called prior to shutdown to wakeup everyone waiting on a disk queue
 * and tell them to exit
 */
void rf_TerminateDiskQueues(raidPtr)
  RF_Raid_t  *raidPtr;
{
  RF_RowCol_t r, c;

  raidPtr->terminate_disk_queues = 1;
  for (r=0; r<raidPtr->numRow; r++) {
    for (c=0; c<raidPtr->numCol + ((r==0) ? raidPtr->numSpare : 0); c++) {
      RF_LOCK_QUEUE_MUTEX(&raidPtr->Queues[r][c], "TerminateDiskQueues");
      RF_BROADCAST_COND(raidPtr->Queues[r][c].cond);
      RF_UNLOCK_QUEUE_MUTEX(&raidPtr->Queues[r][c], "TerminateDiskQueues");
    }
  }
}
#endif /* !KERNEL */

int rf_ConfigureDiskQueues(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_DiskQueue_t **diskQueues, *spareQueues;
  RF_DiskQueueSW_t *p;
  RF_RowCol_t r, c;
  int rc, i;

  raidPtr->maxQueueDepth = cfgPtr->maxOutstandingDiskReqs;

  for(p=NULL,i=0;i<NUM_DISK_QUEUE_TYPES;i++) {
    if (!strcmp(diskqueuesw[i].queueType, cfgPtr->diskQueueType)) {
      p = &diskqueuesw[i];
      break;
    }
  }
  if (p == NULL) {
    RF_ERRORMSG2("Unknown queue type \"%s\".  Using %s\n",cfgPtr->diskQueueType, diskqueuesw[0].queueType);
    p = &diskqueuesw[0];
  }

  RF_CallocAndAdd(diskQueues, raidPtr->numRow, sizeof(RF_DiskQueue_t *), (RF_DiskQueue_t **), raidPtr->cleanupList);
  if (diskQueues == NULL) {
    return(ENOMEM);
  }
  raidPtr->Queues = diskQueues;
  for (r=0; r<raidPtr->numRow; r++) {
    RF_CallocAndAdd(diskQueues[r], raidPtr->numCol + ((r==0) ? raidPtr->numSpare : 0), sizeof(RF_DiskQueue_t), (RF_DiskQueue_t *), raidPtr->cleanupList);
    if (diskQueues[r] == NULL)
      return(ENOMEM);
    for (c=0; c<raidPtr->numCol; c++) {
      rc = config_disk_queue(raidPtr, &diskQueues[r][c], r, c, p,
        raidPtr->sectorsPerDisk, raidPtr->Disks[r][c].dev,
        cfgPtr->maxOutstandingDiskReqs, listp, raidPtr->cleanupList);
      if (rc)
        return(rc);
    }
  }

  spareQueues = &raidPtr->Queues[0][raidPtr->numCol];
  for (r=0; r<raidPtr->numSpare; r++) {
	  rc = config_disk_queue(raidPtr, &spareQueues[r], 
				 0, raidPtr->numCol+r, p,
				 raidPtr->sectorsPerDisk, 
				 raidPtr->Disks[0][raidPtr->numCol+r].dev,
				 cfgPtr->maxOutstandingDiskReqs, listp, 
				 raidPtr->cleanupList);
    if (rc)
      return(rc);
  }
  return(0);
}

/* Enqueue a disk I/O
 *
 * Unfortunately, we have to do things differently in the different
 * environments (simulator, user-level, kernel).
 * At user level, all I/O is blocking, so we have 1 or more threads/disk
 * and the thread that enqueues is different from the thread that dequeues.
 * In the kernel, I/O is non-blocking and so we'd like to have multiple
 * I/Os outstanding on the physical disks when possible.
 *
 * when any request arrives at a queue, we have two choices:
 *    dispatch it to the lower levels
 *    queue it up
 *
 * kernel rules for when to do what:
 *    locking request:  queue empty => dispatch and lock queue,
 *                      else queue it
 *    unlocking req  :  always dispatch it
 *    normal req     :  queue empty => dispatch it & set priority
 *                      queue not full & priority is ok => dispatch it
 *                      else queue it
 *
 * user-level rules:
 *    always enqueue.  In the special case of an unlocking op, enqueue
 *    in a special way that will cause the unlocking op to be the next
 *    thing dequeued.
 *
 * simulator rules:
 *    Do the same as at user level, with the sleeps and wakeups suppressed.
 */
void rf_DiskIOEnqueue(queue, req, pri)
  RF_DiskQueue_t      *queue;
  RF_DiskQueueData_t  *req;
  int                  pri;
{
  int tid;

  RF_ETIMER_START(req->qtime);
  rf_get_threadid(tid);
  RF_ASSERT(req->type == RF_IO_TYPE_NOP || req->numSector);
  req->priority = pri;

  if (rf_queueDebug && (req->numSector == 0)) {
    printf("Warning: Enqueueing zero-sector access\n");
  }
  
#ifdef KERNEL
  /*
   * kernel
   */
  RF_LOCK_QUEUE_MUTEX( queue, "DiskIOEnqueue" );
  /* locking request */
  if (RF_LOCKING_REQ(req)) {
    if (RF_QUEUE_EMPTY(queue)) {
      Dprintf3("Dispatching pri %d locking op to r %d c %d (queue empty)\n",pri,queue->row, queue->col);
      RF_LOCK_QUEUE(queue);
      rf_DispatchKernelIO(queue, req);
    } else {
      queue->queueLength++;  /* increment count of number of requests waiting in this queue */
      Dprintf3("Enqueueing pri %d locking op to r %d c %d (queue not empty)\n",pri,queue->row, queue->col);
      req->queue = (void *)queue;
      (queue->qPtr->Enqueue)(queue->qHdr, req, pri);
    }
  }
  /* unlocking request */
  else if (RF_UNLOCKING_REQ(req)) {           /* we'll do the actual unlock when this I/O completes */
    Dprintf3("Dispatching pri %d unlocking op to r %d c %d\n",pri,queue->row, queue->col);
    RF_ASSERT(RF_QUEUE_LOCKED(queue));
    rf_DispatchKernelIO(queue, req);
  }
  /* normal request */
  else if (RF_OK_TO_DISPATCH(queue, req)) {
    Dprintf3("Dispatching pri %d regular op to r %d c %d (ok to dispatch)\n",pri,queue->row, queue->col);
    rf_DispatchKernelIO(queue, req);
  } else {
    queue->queueLength++;  /* increment count of number of requests waiting in this queue */
    Dprintf3("Enqueueing pri %d regular op to r %d c %d (not ok to dispatch)\n",pri,queue->row, queue->col);
    req->queue = (void *)queue;
    (queue->qPtr->Enqueue)(queue->qHdr, req, pri);
  }
  RF_UNLOCK_QUEUE_MUTEX( queue, "DiskIOEnqueue" );
  
#else /* KERNEL */
  /*
   * user-level
   */
  RF_LOCK_QUEUE_MUTEX( queue, "DiskIOEnqueue" );
  queue->queueLength++;  /* increment count of number of requests waiting in this queue */
  /* unlocking request */
  if (RF_UNLOCKING_REQ(req)) {
    Dprintf4("[%d] enqueueing pri %d unlocking op & signalling r %d c %d\n", tid, pri, queue->row, queue->col);
    RF_ASSERT(RF_QUEUE_LOCKED(queue) && queue->unlockingOp == NULL);
    queue->unlockingOp = req;
  }
  /* locking and normal requests */
  else {
    req->queue = (void *)queue;
    Dprintf5("[%d] enqueueing pri %d %s op & signalling r %d c %d\n", tid, pri,
	     (RF_LOCKING_REQ(req)) ? "locking" : "regular",queue->row,queue->col);
    (queue->qPtr->Enqueue)(queue->qHdr, req, pri);
  }
  SIGNAL_DISK_QUEUE( queue, "DiskIOEnqueue");
  RF_UNLOCK_QUEUE_MUTEX( queue, "DiskIOEnqueue" );
#endif /* KERNEL */
}
    
#if !defined(KERNEL) && !defined(SIMULATE)
/* user-level only: tell all threads to wake up & recheck the queue */
void rf_BroadcastOnQueue(queue)
  RF_DiskQueue_t *queue;
{
  int i;

  if (queue->maxOutstanding > 1) for (i=0; i<queue->maxOutstanding; i++) {
    SIGNAL_DISK_QUEUE(queue, "BroadcastOnQueue" );
  }
}
#endif /* !KERNEL && !SIMULATE */

#ifndef KERNEL /* not used in kernel */

RF_DiskQueueData_t *rf_DiskIODequeue(queue)
  RF_DiskQueue_t *queue;
{
  RF_DiskQueueData_t *p, *headItem;
  int tid;

  rf_get_threadid(tid);
  RF_LOCK_QUEUE_MUTEX( queue, "DiskIODequeue" );
  for (p=NULL; !p; ) {
    if (queue->unlockingOp) {
      /* unlocking request */
      RF_ASSERT(RF_QUEUE_LOCKED(queue));
      p = queue->unlockingOp;
      queue->unlockingOp = NULL;
      Dprintf4("[%d] dequeueing pri %d unlocking op r %d c %d\n", tid, p->priority, queue->row,queue->col);
    }
    else {
      headItem = (queue->qPtr->Peek)(queue->qHdr);
      if (headItem) {
        if (RF_LOCKING_REQ(headItem)) {
          /* locking request */
          if (!RF_QUEUE_LOCKED(queue)) {
            /* queue isn't locked, so dequeue the request & lock the queue */
            p = (queue->qPtr->Dequeue)( queue->qHdr );
            if (p)
              Dprintf4("[%d] dequeueing pri %d locking op r %d c %d\n", tid, p->priority, queue->row, queue->col);
            else
              Dprintf3("[%d] no dequeue -- raw queue empty r %d c %d\n", tid, queue->row, queue->col);
          }
          else {
            /* queue already locked, no dequeue occurs */
            Dprintf3("[%d] no dequeue -- queue is locked r %d c %d\n", tid, queue->row, queue->col);
            p = NULL;
          }
        }
        else {
          /* normal request, always dequeue and assume caller already has lock (if needed) */
          p = (queue->qPtr->Dequeue)( queue->qHdr );
          if (p)
            Dprintf4("[%d] dequeueing pri %d regular op r %d c %d\n", tid, p->priority, queue->row, queue->col);
          else
            Dprintf3("[%d] no dequeue -- raw queue empty r %d c %d\n", tid, queue->row, queue->col);
        }
      }
      else {
        Dprintf3("[%d] no dequeue -- raw queue empty r %d c %d\n", tid, queue->row, queue->col);
      }
    }

    if (queue->raidPtr->terminate_disk_queues) {
      p = NULL;
      break;
    }
#ifdef SIMULATE
    break;		/* in simulator, return NULL on empty queue instead of blocking */
#else /* SIMULATE */
    if (!p) {
      Dprintf3("[%d] nothing to dequeue: waiting r %d c %d\n", tid, queue->row, queue->col);
      WAIT_DISK_QUEUE( queue, "DiskIODequeue" );
    }
#endif /* SIMULATE */
  }

  if (p) {
    queue->queueLength--;  /* decrement count of number of requests waiting in this queue */
    RF_ASSERT(queue->queueLength >= 0);
    queue->numOutstanding++;
    queue->last_deq_sector = p->sectorOffset;
    /* record the amount of time this request spent in the disk queue */
    RF_ETIMER_STOP(p->qtime);
    RF_ETIMER_EVAL(p->qtime);
    if (p->tracerec)
      p->tracerec->diskqueue_us += RF_ETIMER_VAL_US(p->qtime);
  }

  if (p && RF_LOCKING_REQ(p)) {
    RF_ASSERT(!RF_QUEUE_LOCKED(queue));
    Dprintf3("[%d] locking queue r %d c %d\n",tid,queue->row,queue->col);
    RF_LOCK_QUEUE(queue);
  }
  RF_UNLOCK_QUEUE_MUTEX( queue, "DiskIODequeue" );
  
  return(p);
}

#else /* !KERNEL */

/* get the next set of I/Os started, kernel version only */
void rf_DiskIOComplete(queue, req, status)
  RF_DiskQueue_t      *queue;
  RF_DiskQueueData_t  *req;
  int                  status;
{
  int done=0;

  RF_LOCK_QUEUE_MUTEX( queue, "DiskIOComplete" );

  /* unlock the queue:
     (1) after an unlocking req completes
     (2) after a locking req fails
  */
  if (RF_UNLOCKING_REQ(req) || (RF_LOCKING_REQ(req) && status)) {
    Dprintf2("DiskIOComplete: unlocking queue at r %d c %d\n", queue->row, queue->col);
    RF_ASSERT(RF_QUEUE_LOCKED(queue) && (queue->unlockingOp == NULL));
    RF_UNLOCK_QUEUE(queue);
  }

  queue->numOutstanding--;
  RF_ASSERT(queue->numOutstanding >= 0);

  /* dispatch requests to the disk until we find one that we can't. */
  /* no reason to continue once we've filled up the queue */
  /* no reason to even start if the queue is locked */
  
  while (!done && !RF_QUEUE_FULL(queue) && !RF_QUEUE_LOCKED(queue)) {
    if (queue->nextLockingOp) {
      req = queue->nextLockingOp; queue->nextLockingOp = NULL;
      Dprintf3("DiskIOComplete: a pri %d locking req was pending at r %d c %d\n",req->priority,queue->row, queue->col);
    } else {
      req = (queue->qPtr->Dequeue)( queue->qHdr );
      if (req != NULL) {
	      Dprintf3("DiskIOComplete: extracting pri %d req from queue at r %d c %d\n",req->priority,queue->row, queue->col);
      } else {
	      Dprintf1("DiskIOComplete: no more requests to extract.\n","");
      }
    }
    if (req) {
	queue->queueLength--;  /* decrement count of number of requests waiting in this queue */
	RF_ASSERT(queue->queueLength >= 0);
    }
    if (!req) done=1;
    else if (RF_LOCKING_REQ(req)) {
      if (RF_QUEUE_EMPTY(queue)) {                   					/* dispatch it */
	Dprintf3("DiskIOComplete: dispatching pri %d locking req to r %d c %d (queue empty)\n",req->priority,queue->row, queue->col);
	RF_LOCK_QUEUE(queue);
	rf_DispatchKernelIO(queue, req);
	done = 1;
      } else {                         		           /* put it aside to wait for the queue to drain */
	Dprintf3("DiskIOComplete: postponing pri %d locking req to r %d c %d\n",req->priority,queue->row, queue->col);
	RF_ASSERT(queue->nextLockingOp == NULL);
	queue->nextLockingOp = req;
	done = 1;
      }
    } else if (RF_UNLOCKING_REQ(req)) {      	/* should not happen: unlocking ops should not get queued */
      RF_ASSERT(RF_QUEUE_LOCKED(queue)); 			               /* support it anyway for the future */
      Dprintf3("DiskIOComplete: dispatching pri %d unl req to r %d c %d (SHOULD NOT SEE THIS)\n",req->priority,queue->row, queue->col);
      rf_DispatchKernelIO(queue, req);
      done = 1;
    } else if (RF_OK_TO_DISPATCH(queue, req)) {
      Dprintf3("DiskIOComplete: dispatching pri %d regular req to r %d c %d (ok to dispatch)\n",req->priority,queue->row, queue->col);
      rf_DispatchKernelIO(queue, req);
    } else {                                   		  /* we can't dispatch it, so just re-enqueue it.  */
      /* potential trouble here if disk queues batch reqs */
      Dprintf3("DiskIOComplete: re-enqueueing pri %d regular req to r %d c %d\n",req->priority,queue->row, queue->col);
      queue->queueLength++; 
      (queue->qPtr->Enqueue)(queue->qHdr, req, req->priority);
      done = 1;
    }
  }
  
  RF_UNLOCK_QUEUE_MUTEX( queue, "DiskIOComplete" );
}
#endif /* !KERNEL */

/* promotes accesses tagged with the given parityStripeID from low priority
 * to normal priority.  This promotion is optional, meaning that a queue
 * need not implement it.  If there is no promotion routine associated with
 * a queue, this routine does nothing and returns -1.
 */
int rf_DiskIOPromote(queue, parityStripeID, which_ru)
  RF_DiskQueue_t     *queue;
  RF_StripeNum_t      parityStripeID;
  RF_ReconUnitNum_t   which_ru;
{
  int retval;
  
  if (!queue->qPtr->Promote)
    return(-1);
  RF_LOCK_QUEUE_MUTEX( queue, "DiskIOPromote" );
  retval = (queue->qPtr->Promote)( queue->qHdr, parityStripeID, which_ru );
  RF_UNLOCK_QUEUE_MUTEX( queue, "DiskIOPromote" );
  return(retval);
}

RF_DiskQueueData_t *rf_CreateDiskQueueData(
  RF_IoType_t                typ,
  RF_SectorNum_t             ssect,
  RF_SectorCount_t           nsect,
  caddr_t                    buf,
  RF_StripeNum_t             parityStripeID,
  RF_ReconUnitNum_t          which_ru,
  int                      (*wakeF)(void *,int),
  void                      *arg,
  RF_DiskQueueData_t        *next,
  RF_AccTraceEntry_t        *tracerec,
  void                      *raidPtr,
  RF_DiskQueueDataFlags_t    flags,
  void                      *kb_proc)
{
  RF_DiskQueueData_t *p;

  RF_FREELIST_GET_INIT(rf_dqd_freelist,p,next,(RF_DiskQueueData_t *),init_dqd);

  p->sectorOffset  = ssect + rf_protectedSectors;
  p->numSector     = nsect;
  p->type          = typ;
  p->buf           = buf;
  p->parityStripeID= parityStripeID;
  p->which_ru      = which_ru;
  p->CompleteFunc  = wakeF;
  p->argument      = arg;
  p->next          = next;
  p->tracerec      = tracerec;
  p->priority      = RF_IO_NORMAL_PRIORITY;
  p->AuxFunc       = NULL;
  p->buf2          = NULL;
#ifdef SIMULATE
  p->owner         = rf_GetCurrentOwner();
#endif /* SIMULATE */
  p->raidPtr       = raidPtr;
  p->flags         = flags;
#ifdef KERNEL
  p->b_proc        = kb_proc;
#endif /* KERNEL */
  return(p);
}

RF_DiskQueueData_t *rf_CreateDiskQueueDataFull(
  RF_IoType_t                typ,
  RF_SectorNum_t             ssect,
  RF_SectorCount_t           nsect,
  caddr_t                    buf,
  RF_StripeNum_t             parityStripeID,
  RF_ReconUnitNum_t          which_ru,
  int                      (*wakeF)(void *,int),
  void                      *arg,
  RF_DiskQueueData_t        *next,
  RF_AccTraceEntry_t        *tracerec,
  int                        priority,
  int                      (*AuxFunc)(void *,...),
  caddr_t                    buf2,
  void                      *raidPtr,
  RF_DiskQueueDataFlags_t    flags,
  void                      *kb_proc)
{
  RF_DiskQueueData_t *p;

  RF_FREELIST_GET_INIT(rf_dqd_freelist,p,next,(RF_DiskQueueData_t *),init_dqd);

  p->sectorOffset  = ssect + rf_protectedSectors;
  p->numSector     = nsect;
  p->type          = typ;
  p->buf           = buf;
  p->parityStripeID= parityStripeID;
  p->which_ru      = which_ru;
  p->CompleteFunc  = wakeF;
  p->argument      = arg;
  p->next          = next;
  p->tracerec      = tracerec;
  p->priority      = priority;
  p->AuxFunc       = AuxFunc;
  p->buf2          = buf2;
#ifdef SIMULATE
  p->owner         = rf_GetCurrentOwner();
#endif /* SIMULATE */
  p->raidPtr       = raidPtr;
  p->flags         = flags;
#ifdef KERNEL
  p->b_proc        = kb_proc;
#endif /* KERNEL */
  return(p);
}

void rf_FreeDiskQueueData(p)
  RF_DiskQueueData_t  *p;
{
	RF_FREELIST_FREE_CLEAN(rf_dqd_freelist,p,next,clean_dqd);
}
