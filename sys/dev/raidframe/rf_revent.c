/*	$OpenBSD: rf_revent.c,v 1.1 1999/01/11 14:29:48 niklas Exp $	*/
/*	$NetBSD: rf_revent.c,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author:
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
 * revent.c -- reconstruction event handling code
 */
/*
 * :  
 * Log: rf_revent.c,v 
 * Revision 1.22  1996/08/11 00:41:11  jimz
 * extern hz only for kernel
 *
 * Revision 1.21  1996/07/15  05:40:41  jimz
 * some recon datastructure cleanup
 * better handling of multiple failures
 * added undocumented double-recon test
 *
 * Revision 1.20  1996/06/17  03:18:04  jimz
 * include shutdown.h for macroized ShutdownCreate
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
 * Revision 1.15  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.14  1996/05/20  16:13:40  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 * use RF_FREELIST for revents
 *
 * Revision 1.13  1996/05/18  20:09:47  jimz
 * bit of cleanup to compile cleanly in kernel, once again
 *
 * Revision 1.12  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 */

#ifdef _KERNEL
#define KERNEL
#endif

#include <sys/errno.h>

#include "rf_raid.h"
#include "rf_revent.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_freelist.h"
#include "rf_desc.h"
#include "rf_shutdown.h"

static RF_FreeList_t *rf_revent_freelist;
#define RF_MAX_FREE_REVENT 128
#define RF_REVENT_INC        8
#define RF_REVENT_INITIAL    8


#ifdef KERNEL

#include <sys/proc.h>

extern int hz;

#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#define DO_WAIT(_rc)       mpsleep(&(_rc)->eventQueue, PZERO, "raidframe eventq", 0, \
			      (void *) simple_lock_addr((_rc)->eq_mutex), MS_LOCK_SIMPLE)
#else
#define DO_WAIT(_rc)   tsleep(&(_rc)->eventQueue, PRIBIO | PCATCH, "raidframe eventq", 0)
#endif

#define DO_SIGNAL(_rc)     wakeup(&(_rc)->eventQueue)

#else      /* KERNEL */

#define DO_WAIT(_rc)       RF_WAIT_COND((_rc)->eq_cond, (_rc)->eq_mutex)
#define DO_SIGNAL(_rc)     RF_SIGNAL_COND((_rc)->eq_cond)

#endif     /* KERNEL */

static void rf_ShutdownReconEvent(void *);

static RF_ReconEvent_t *GetReconEventDesc(RF_RowCol_t row, RF_RowCol_t col,
	void *arg, RF_Revent_t type);
RF_ReconEvent_t *rf_GetNextReconEvent(RF_RaidReconDesc_t   *,
				      RF_RowCol_t, void (*continueFunc)(void *),
				      void *);     

static void rf_ShutdownReconEvent(ignored)
  void  *ignored;
{
	RF_FREELIST_DESTROY(rf_revent_freelist,next,(RF_ReconEvent_t *));
}

int rf_ConfigureReconEvent(listp)
  RF_ShutdownList_t  **listp;
{
  int rc;

  RF_FREELIST_CREATE(rf_revent_freelist, RF_MAX_FREE_REVENT,
    RF_REVENT_INC, sizeof(RF_ReconEvent_t));
  if (rf_revent_freelist == NULL)
    return(ENOMEM);
  rc = rf_ShutdownCreate(listp, rf_ShutdownReconEvent, NULL);
  if (rc) {
    RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    rf_ShutdownReconEvent(NULL);
    return(rc);
  }
  RF_FREELIST_PRIME(rf_revent_freelist, RF_REVENT_INITIAL,next,
    (RF_ReconEvent_t *));
  return(0);
}

/* returns the next reconstruction event, blocking the calling thread until
 * one becomes available
 */

/* will now return null if it is blocked or will return an event if it is not */

RF_ReconEvent_t *rf_GetNextReconEvent(reconDesc, row, continueFunc, continueArg)
  RF_RaidReconDesc_t   *reconDesc;
  RF_RowCol_t           row;
  void                (*continueFunc)(void *);
  void                 *continueArg;     
{
  RF_Raid_t *raidPtr = reconDesc->raidPtr;
  RF_ReconCtrl_t *rctrl = raidPtr->reconControl[row];
  RF_ReconEvent_t *event;
  
  RF_ASSERT( row >= 0 && row <= raidPtr->numRow );
  RF_LOCK_MUTEX(rctrl->eq_mutex);
  RF_ASSERT( (rctrl->eventQueue==NULL) == (rctrl->eq_count == 0));  /* q null and count==0 must be equivalent conditions */


  rctrl->continueFunc=continueFunc;
  rctrl->continueArg=continueArg;

#ifdef SIMULATE
  if (!rctrl->eventQueue) {
    RF_UNLOCK_MUTEX(rctrl->eq_mutex);
    return (NULL);
  }
#else /* SIMULATE */

#ifdef KERNEL

/* mpsleep timeout value: secs = timo_val/hz.  'ticks' here is defined as cycle-counter ticks, not softclock ticks */
#define MAX_RECON_EXEC_TICKS 15000000  /* 150 Mhz => this many ticks in 100 ms */
#define RECON_DELAY_MS 25
#define RECON_TIMO     ((RECON_DELAY_MS * hz) / 1000)

  /* we are not pre-emptible in the kernel, but we don't want to run forever.  If we run w/o blocking
   * for more than MAX_RECON_EXEC_TICKS ticks of the cycle counter, delay for RECON_DELAY before continuing.
   * this may murder us with context switches, so we may need to increase both the MAX...TICKS and the RECON_DELAY_MS.
   */
  if (reconDesc->reconExecTimerRunning) {
    int status;
    
    RF_ETIMER_STOP(reconDesc->recon_exec_timer);
    RF_ETIMER_EVAL(reconDesc->recon_exec_timer);
    reconDesc->reconExecTicks += RF_ETIMER_VAL_TICKS(reconDesc->recon_exec_timer);
    if (reconDesc->reconExecTicks > reconDesc->maxReconExecTicks)
      reconDesc->maxReconExecTicks = reconDesc->reconExecTicks;
    if (reconDesc->reconExecTicks >= MAX_RECON_EXEC_TICKS) {
      /* we've been running too long.  delay for RECON_DELAY_MS */
#if RF_RECON_STATS > 0
      reconDesc->numReconExecDelays++;
#endif /* RF_RECON_STATS > 0 */
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
      status = mpsleep(&reconDesc->reconExecTicks, PZERO, "recon delay", RECON_TIMO, (void *) simple_lock_addr(rctrl->eq_mutex), MS_LOCK_SIMPLE);
#else
      status = tsleep(&reconDesc->reconExecTicks, PRIBIO | PCATCH, "recon delay", RECON_TIMO );
#endif
      RF_ASSERT(status == EWOULDBLOCK);
      reconDesc->reconExecTicks = 0;
    }
  }

#endif /* KERNEL */

  while (!rctrl->eventQueue) {
#if RF_RECON_STATS > 0
    reconDesc->numReconEventWaits++;
#endif /* RF_RECON_STATS > 0 */
    DO_WAIT(rctrl);
#ifdef KERNEL
    reconDesc->reconExecTicks = 0; /* we've just waited */
#endif /* KERNEL */
  }

#endif /* SIMULATE */

#ifdef KERNEL
  reconDesc->reconExecTimerRunning = 1;
  RF_ETIMER_START(reconDesc->recon_exec_timer);
#endif /* KERNEL */

  event = rctrl->eventQueue;
  rctrl->eventQueue = event->next;
  event->next = NULL;
  rctrl->eq_count--;
  RF_ASSERT( (rctrl->eventQueue==NULL) == (rctrl->eq_count == 0));  /* q null and count==0 must be equivalent conditions */
  RF_UNLOCK_MUTEX(rctrl->eq_mutex);
  return(event);
}

/* enqueues a reconstruction event on the indicated queue */
void rf_CauseReconEvent(raidPtr, row, col, arg, type)
  RF_Raid_t    *raidPtr;
  RF_RowCol_t   row;
  RF_RowCol_t   col;
  void         *arg;
  RF_Revent_t   type;
{
  RF_ReconCtrl_t *rctrl = raidPtr->reconControl[row];
  RF_ReconEvent_t *event = GetReconEventDesc(row, col, arg, type);

  if (type == RF_REVENT_BUFCLEAR) {
    RF_ASSERT(col != rctrl->fcol);
  }
  
  RF_ASSERT( row >= 0 && row <= raidPtr->numRow && col >=0 && col <= raidPtr->numCol );
  RF_LOCK_MUTEX(rctrl->eq_mutex);
  RF_ASSERT( (rctrl->eventQueue==NULL) == (rctrl->eq_count == 0));  /* q null and count==0 must be equivalent conditions */
  event->next = rctrl->eventQueue;
  rctrl->eventQueue = event;
  rctrl->eq_count++;
  RF_UNLOCK_MUTEX(rctrl->eq_mutex);

#ifndef SIMULATE
  DO_SIGNAL(rctrl);
#else /* !SIMULATE */
  (rctrl->continueFunc)(rctrl->continueArg);
#endif /* !SIMULATE */
}

/* allocates and initializes a recon event descriptor */
static RF_ReconEvent_t *GetReconEventDesc(row, col, arg, type)
  RF_RowCol_t   row;
  RF_RowCol_t   col;
  void         *arg;
  RF_Revent_t   type;
{
	RF_ReconEvent_t *t;

	RF_FREELIST_GET(rf_revent_freelist,t,next,(RF_ReconEvent_t *));
	if (t == NULL)
		return(NULL);
	t->col = col;
	t->arg = arg;
	t->type = type;
	return(t);
}

void rf_FreeReconEventDesc(event)
  RF_ReconEvent_t  *event;
{
	RF_FREELIST_FREE(rf_revent_freelist,event,next);
}
