/*	$OpenBSD: rf_revent.c,v 1.10 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_revent.c,v 1.7 2000/05/30 02:04:29 oster Exp $	*/

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
 * revent.c -- Reconstruction event handling code.
 */

#include <sys/errno.h>

#include "rf_raid.h"
#include "rf_revent.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_freelist.h"
#include "rf_desc.h"
#include "rf_shutdown.h"

static RF_FreeList_t *rf_revent_freelist;
#define	RF_MAX_FREE_REVENT	128
#define	RF_REVENT_INC		  8
#define	RF_REVENT_INITIAL	  8



#include <sys/proc.h>
#include <sys/kernel.h>

#define	DO_WAIT(_rc)							\
	tsleep(&(_rc)->eventQueue, PRIBIO, "RAIDframe eventq", 0)

#define	DO_SIGNAL(_rc)	wakeup(&(_rc)->eventQueue)


void rf_ShutdownReconEvent(void *);

RF_ReconEvent_t *GetReconEventDesc(RF_RowCol_t, RF_RowCol_t, void *,
	RF_Revent_t);

void
rf_ShutdownReconEvent(void *ignored)
{
	RF_FREELIST_DESTROY(rf_revent_freelist, next, (RF_ReconEvent_t *));
}

int
rf_ConfigureReconEvent(RF_ShutdownList_t **listp)
{
	int rc;

	RF_FREELIST_CREATE(rf_revent_freelist, RF_MAX_FREE_REVENT,
	    RF_REVENT_INC, sizeof(RF_ReconEvent_t));
	if (rf_revent_freelist == NULL)
		return (ENOMEM);
	rc = rf_ShutdownCreate(listp, rf_ShutdownReconEvent, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d"
		    " rc=%d\n", __FILE__, __LINE__, rc);
		rf_ShutdownReconEvent(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME(rf_revent_freelist, RF_REVENT_INITIAL, next,
	    (RF_ReconEvent_t *));
	return (0);
}

/*
 * Returns the next reconstruction event, blocking the calling thread
 * until one becomes available. Will now return null if it is blocked
 * or will return an event if it is not.
 */

RF_ReconEvent_t *
rf_GetNextReconEvent(RF_RaidReconDesc_t *reconDesc, RF_RowCol_t row,
    void (*continueFunc) (void *), void *continueArg)
{
	RF_Raid_t *raidPtr = reconDesc->raidPtr;
	RF_ReconCtrl_t *rctrl = raidPtr->reconControl[row];
	RF_ReconEvent_t *event;

	RF_ASSERT(row >= 0 && row <= raidPtr->numRow);
	RF_LOCK_MUTEX(rctrl->eq_mutex);
	/* q NULL and count==0 must be equivalent conditions. */
	RF_ASSERT((rctrl->eventQueue == NULL) == (rctrl->eq_count == 0));

	rctrl->continueFunc = continueFunc;
	rctrl->continueArg = continueArg;

	/*
	 * mpsleep timeout value: secs = timo_val/hz. 'ticks' here is
	 * defined as cycle-counter ticks, not softclock ticks.
	 */

#define	MAX_RECON_EXEC_USECS		(100 * 1000)	/* 100 ms */
#define	RECON_DELAY_MS			25
#define	RECON_TIMO			((RECON_DELAY_MS * hz) / 1000)

	/*
	 * We are not pre-emptible in the kernel, but we don't want to run
	 * forever. If we run w/o blocking for more than MAX_RECON_EXEC_USECS
	 * delay for RECON_DELAY_MS before continuing. This may murder us with
	 * context switches, so we may need to increase both the
	 * MAX...TICKS and the RECON_DELAY_MS.
	 */
	if (reconDesc->reconExecTimerRunning) {
		int status;

		RF_ETIMER_STOP(reconDesc->recon_exec_timer);
		RF_ETIMER_EVAL(reconDesc->recon_exec_timer);
		reconDesc->reconExecTicks +=
		    RF_ETIMER_VAL_US(reconDesc->recon_exec_timer);
		if (reconDesc->reconExecTicks > reconDesc->maxReconExecTicks)
			reconDesc->maxReconExecTicks =
			    reconDesc->reconExecTicks;
		if (reconDesc->reconExecTicks >= MAX_RECON_EXEC_USECS) {
			/* We've been running too long - sleep. */
#if RF_RECON_STATS > 0
			reconDesc->numReconExecDelays++;
#endif /* RF_RECON_STATS > 0 */
			status = tsleep(&reconDesc->reconExecTicks,
			    PRIBIO, "recon delay", RECON_TIMO);
			RF_ASSERT(status == EWOULDBLOCK);
			reconDesc->reconExecTicks = 0;
		}
	}
	while (!rctrl->eventQueue) {
#if RF_RECON_STATS > 0
		reconDesc->numReconEventWaits++;
#endif				/* RF_RECON_STATS > 0 */
		DO_WAIT(rctrl);
		reconDesc->reconExecTicks = 0;	/* We've just waited. */
	}

	RF_ETIMER_START(reconDesc->recon_exec_timer);
	reconDesc->reconExecTimerRunning = 1;

	event = rctrl->eventQueue;
	rctrl->eventQueue = event->next;
	event->next = NULL;
	rctrl->eq_count--;

	/* q NULL and count==0 must be equivalent conditions. */
	RF_ASSERT((rctrl->eventQueue == NULL) == (rctrl->eq_count == 0));
	RF_UNLOCK_MUTEX(rctrl->eq_mutex);
	return (event);
}

/* Enqueues a reconstruction event on the indicated queue. */
void
rf_CauseReconEvent(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col,
    void *arg, RF_Revent_t type)
{
	RF_ReconCtrl_t *rctrl = raidPtr->reconControl[row];
	RF_ReconEvent_t *event = GetReconEventDesc(row, col, arg, type);

	if (type == RF_REVENT_BUFCLEAR) {
		RF_ASSERT(col != rctrl->fcol);
	}
	RF_ASSERT(row >= 0 && row <= raidPtr->numRow && col >= 0 &&
	    col <= raidPtr->numCol);
	RF_LOCK_MUTEX(rctrl->eq_mutex);
	/* q NULL and count==0 must be equivalent conditions. */
	RF_ASSERT((rctrl->eventQueue == NULL) == (rctrl->eq_count == 0));
	event->next = rctrl->eventQueue;
	rctrl->eventQueue = event;
	rctrl->eq_count++;
	RF_UNLOCK_MUTEX(rctrl->eq_mutex);

	DO_SIGNAL(rctrl);
}

/* Allocates and initializes a recon event descriptor. */
RF_ReconEvent_t *
GetReconEventDesc(RF_RowCol_t row, RF_RowCol_t col, void *arg, RF_Revent_t type)
{
	RF_ReconEvent_t *t;

	RF_FREELIST_GET(rf_revent_freelist, t, next, (RF_ReconEvent_t *));
	if (t == NULL)
		return (NULL);
	t->col = col;
	t->arg = arg;
	t->type = type;
	return (t);
}

void
rf_FreeReconEventDesc(RF_ReconEvent_t *event)
{
	RF_FREELIST_FREE(rf_revent_freelist, event, next);
}
