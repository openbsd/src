/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

#include "rx_locl.h"

RCSID("$arla: rx_event.c,v 1.6 2003/01/19 08:46:29 lha Exp $");

/*
 * All event processing is relative to the apparent current time
 * given by clock_GetTime
 */

/* This should be static, but event_test wants to look at the free list... */
struct rx_queue rxevent_free;	       /* It's somewhat bogus to use a
				        * doubly-linked queue for the free
				        * list */
static struct rx_queue rxevent_queue;  /* The list of waiting events */
static int rxevent_allocUnit = 10;     /* Allocation unit (number of event
				        * records to allocate at one time) */
int rxevent_nFree;		       /* Number of free event records */
int rxevent_nPosted;		       /* Current number of posted events */
static void (*rxevent_ScheduledEarlierEvent) ();/* Proc to call when an event
						 * is scheduled that is
						 * earlier than all other
						 * events */
static struct xfreelist {
    struct xfreelist *next;
} *xfreemallocs = 0, *xsp = 0;

#ifdef RXDEBUG
FILE *rxevent_debugFile;	       /* Set to an stdio descriptor for
				        * event logging to that file */
#endif

/* Pass in the number of events to allocate at a time */
static int initialized = 0;

void
rxevent_Init(int nEvents, void (*scheduler) ())
{
    if (initialized)
	return;
    clock_Init();
    if (nEvents)
	rxevent_allocUnit = nEvents;
    queue_Init(&rxevent_free);
    queue_Init(&rxevent_queue);
    rxevent_nFree = rxevent_nPosted = 0;
    rxevent_ScheduledEarlierEvent = scheduler;
    initialized = 1;
}

/* Add the indicated event (function, arg) at the specified clock time */
struct rxevent *
rxevent_Post(struct clock * when, void (*func)(), void *arg, void *arg1)
/* when - When event should happen, in clock (clock.h) units */
{
    struct rxevent *ev, *qe, *qpr;

#ifdef RXDEBUG
    if (Log) {
	struct clock now;

	clock_GetTime(&now);
	fprintf(Log, "%ld.%ld: rxevent_Post(%ld.%ld, %p, %p)\n",
		now.sec, now.usec, when->sec, when->usec, func, arg);
    }
#endif
#if defined(AFS_SGIMP_ENV)
    ASSERT(osi_rxislocked());
#endif

    /*
     * If we're short on free event entries, create a block of new ones and
     * add them to the free queue
     */
    if (queue_IsEmpty(&rxevent_free)) {
	int i;

#if	defined(AFS_AIX32_ENV) && defined(KERNEL)
	ev = (struct rxevent *) rxi_Alloc(sizeof(struct rxevent));
	queue_Append(&rxevent_free, &ev[0]), rxevent_nFree++;
#else
	ev = (struct rxevent *) osi_Alloc(sizeof(struct rxevent) *
					  rxevent_allocUnit);
	xsp = xfreemallocs;
	xfreemallocs = (struct xfreelist *) ev;
	xfreemallocs->next = xsp;
	for (i = 0; i < rxevent_allocUnit; i++)
	    queue_Append(&rxevent_free, &ev[i]), rxevent_nFree++;
#endif
    }
    /* Grab and initialize a new rxevent structure */
    ev = queue_First(&rxevent_free, rxevent);
    queue_Remove(ev);
    rxevent_nFree--;

    /* Record user defined event state */
    ev->eventTime = *when;
    ev->func = func;
    ev->arg = arg;
    ev->arg1 = arg1;
    rxevent_nPosted += 1;	       /* Rather than ++, to shut high-C up
				        * regarding never-set variables */

    /*
     * Locate a slot for the new entry.  The queue is ordered by time, and we
     * assume that a new entry is likely to be greater than a majority of the
     * entries already on the queue (unless there's very few entries on the
     * queue), so we scan it backwards
     */
    for (queue_ScanBackwards(&rxevent_queue, qe, qpr, rxevent)) {
	if (clock_Ge(when, &qe->eventTime)) {
	    queue_InsertAfter(qe, ev);
	    return ev;
	}
    }
    /* The event is to expire earlier than any existing events */
    queue_Prepend(&rxevent_queue, ev);
    if (rxevent_ScheduledEarlierEvent)
	(*rxevent_ScheduledEarlierEvent) ();	/* Notify our external
						 * scheduler */
    return ev;
}

/*
 * Cancel an event by moving it from the event queue to the free list.
 * Warning, the event must be on the event queue! If not, this should core
 * dump (reference through 0).  This routine should be called using the macro
 * event_Cancel, which checks for a null event and also nulls the caller's
 * event pointer after cancelling the event.
 */
void
rxevent_Cancel_1(struct rxevent * ev)
{
#ifdef RXDEBUG
    if (Log) {
	struct clock now;

	clock_GetTime(&now);
	fprintf(Log, "%ld.%ld: rxevent_Cancel_1(%ld.%ld, %p, %p)\n",
		now.sec, now.usec, ev->eventTime.sec, ev->eventTime.usec,
		ev->func, ev->arg);
    }
#endif
    /*
     * Append it to the free list (rather than prepending) to keep
     * the free list hot so nothing pages out
     */
#if defined(AFS_SGIMP_ENV)
    ASSERT(osi_rxislocked());
#endif
    queue_MoveAppend(&rxevent_free, ev);
    rxevent_nPosted--;
    rxevent_nFree++;
}

/*
 * Process all events that have expired relative to the current clock time
 * (which is not re-evaluated unless clock_NewTime has been called).
 * The relative time to the next event is returned in the output parameter
 * next and the function returns 1.  If there are is no next event,
 * the function returns 0.
 */
int
rxevent_RaiseEvents(struct clock * next)
{
    struct rxevent *qe;
    struct clock now;

#ifdef RXDEBUG
    if (Log)
	fprintf(Log, "rxevent_RaiseEvents(%ld.%ld)\n", now.sec, now.usec);
#endif

    /*
     * Events are sorted by time, so only scan until an event is found that
     * has not yet timed out
     */
    while (queue_IsNotEmpty(&rxevent_queue)) {
	clock_GetTime(&now);
	qe = queue_First(&rxevent_queue, rxevent);
	if (clock_Lt(&now, &qe->eventTime)) {
	    *next = qe->eventTime;
	    clock_Sub(next, &now);
	    return 1;
	}
	queue_Remove(qe);
	rxevent_nPosted--;
	qe->func(qe, qe->arg, qe->arg1);
	queue_Append(&rxevent_free, qe);
	rxevent_nFree++;
    }
    return 0;
}

#if 0
static void
shutdown_rxevent(void)
{
    struct xfreelist *xp, *nxp;

    initialized = 0;
#if	defined(AFS_AIX32_ENV) && defined(KERNEL)
    /* Everything is freed in afs_osinet.c */
#else
    while ((xp = xfreemallocs) != NULL) {
	nxp = xp->next;
	osi_Free((char *) xp, sizeof(struct rxevent) * rxevent_allocUnit);
    }
#endif
}
#endif
