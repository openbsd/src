/*	$OpenBSD: rf_fifo.c,v 1.1 1999/01/11 14:29:22 niklas Exp $	*/
/*	$NetBSD: rf_fifo.c,v 1.1 1998/11/13 04:20:29 oster Exp $	*/
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

/***************************************************
 *
 * rf_fifo.c --  prioritized fifo queue code.  
 * There are only two priority levels: hi and lo.
 *
 * Aug 4, 1994, adapted from raidSim version (MCH)
 *
 ***************************************************/

/*
 * :  
 * Log: rf_fifo.c,v 
 * Revision 1.20  1996/06/18 20:53:11  jimz
 * fix up disk queueing (remove configure routine,
 * add shutdown list arg to create routines)
 *
 * Revision 1.19  1996/06/14  00:08:21  jimz
 * make happier in all environments
 *
 * Revision 1.18  1996/06/13  20:41:24  jimz
 * add random queueing
 *
 * Revision 1.17  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.16  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.15  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.14  1996/06/06  01:15:02  jimz
 * added debugging
 *
 * Revision 1.13  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.12  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.11  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.10  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.9  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.8  1995/12/01  18:22:15  root
 * added copyright info
 *
 * Revision 1.7  1995/11/07  15:32:16  wvcii
 * added function FifoPeek()
 *
 */

#include "rf_types.h"
#include "rf_alloclist.h"
#include "rf_stripelocks.h"
#include "rf_layout.h"
#include "rf_diskqueue.h"
#include "rf_fifo.h"
#include "rf_debugMem.h"
#include "rf_general.h"
#include "rf_threadid.h"
#include "rf_options.h"

#if !defined(KERNEL) && RF_INCLUDE_QUEUE_RANDOM > 0
#include "rf_randmacros.h"
RF_DECLARE_STATIC_RANDOM
#endif /* !KERNEL && RF_INCLUDE_QUEUE_RANDOM > 0 */

/* just malloc a header, zero it (via calloc), and return it */
/*ARGSUSED*/
void *rf_FifoCreate(sectPerDisk, clList, listp)
  RF_SectorCount_t      sectPerDisk;
  RF_AllocListElem_t   *clList;
  RF_ShutdownList_t   **listp;
{
  RF_FifoHeader_t *q;

#if !defined(KERNEL) && RF_INCLUDE_QUEUE_RANDOM > 0
  RF_INIT_STATIC_RANDOM(1);
#endif /* !KERNEL && RF_INCLUDE_QUEUE_RANDOM > 0 */
  RF_CallocAndAdd(q, 1, sizeof(RF_FifoHeader_t), (RF_FifoHeader_t *), clList);
  q->hq_count = q->lq_count = 0;
#if !defined(KERNEL) && RF_INCLUDE_QUEUE_RANDOM > 0
  q->rval = (long)RF_STATIC_RANDOM();
#endif /* !KERNEL && RF_INCLUDE_QUEUE_RANDOM > 0 */
  return((void *)q);
}

void rf_FifoEnqueue(q_in, elem, priority)
  void                *q_in;
  RF_DiskQueueData_t  *elem;
  int                  priority;
{
  RF_FifoHeader_t *q = (RF_FifoHeader_t *)q_in;

  RF_ASSERT(priority == RF_IO_NORMAL_PRIORITY || priority == RF_IO_LOW_PRIORITY);

  elem->next = NULL;
  if (priority == RF_IO_NORMAL_PRIORITY) {
    if (!q->hq_tail) {
      RF_ASSERT(q->hq_count == 0 && q->hq_head == NULL);
      q->hq_head = q->hq_tail = elem;
    } else {
      RF_ASSERT(q->hq_count != 0 && q->hq_head != NULL);
      q->hq_tail->next = elem;
      q->hq_tail = elem;
    }
    q->hq_count++;
  }
  else {
    RF_ASSERT(elem->next == NULL);
    if (rf_fifoDebug) {
      int tid;
      rf_get_threadid(tid);
      printf("[%d] fifo: ENQ lopri\n", tid);
    }
    if (!q->lq_tail) {
      RF_ASSERT(q->lq_count == 0 && q->lq_head == NULL);
      q->lq_head = q->lq_tail = elem;
    } else {
      RF_ASSERT(q->lq_count != 0 && q->lq_head != NULL);
      q->lq_tail->next = elem;
      q->lq_tail = elem;
    }
    q->lq_count++;
  }
  if ((q->hq_count + q->lq_count)!= elem->queue->queueLength) {
	  printf("Queue lengths differ!: %d %d %d\n",
		 q->hq_count, q->lq_count, (int)elem->queue->queueLength);
	  printf("%d %d %d %d\n",
		 (int)elem->queue->numOutstanding,
		 (int)elem->queue->maxOutstanding,
		 (int)elem->queue->row,
		 (int)elem->queue->col);
  }
  RF_ASSERT((q->hq_count + q->lq_count) == elem->queue->queueLength);
}

RF_DiskQueueData_t *rf_FifoDequeue(q_in)
  void  *q_in;
{
  RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;
  RF_DiskQueueData_t *nd;

  RF_ASSERT(q);
  if (q->hq_head) {
    RF_ASSERT(q->hq_count != 0 && q->hq_tail != NULL);
    nd = q->hq_head; q->hq_head = q->hq_head->next;
    if (!q->hq_head) q->hq_tail = NULL;
    nd->next = NULL;
    q->hq_count--;
  } else if (q->lq_head) {
    RF_ASSERT(q->lq_count != 0 && q->lq_tail != NULL);
    nd = q->lq_head; q->lq_head = q->lq_head->next;
    if (!q->lq_head) q->lq_tail = NULL;
    nd->next = NULL;
    q->lq_count--;
    if (rf_fifoDebug) {
      int tid;
      rf_get_threadid(tid);
      printf("[%d] fifo: DEQ lopri %lx\n", tid, (long)nd);
    }
  } else {
    RF_ASSERT(q->hq_count == 0 && q->lq_count == 0 && q->hq_tail == NULL && q->lq_tail == NULL);
    nd = NULL;
  }
  return(nd);
}

/* This never gets used!! No loss (I hope) if we don't include it... GO */
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(_KERNEL)

static RF_DiskQueueData_t *n_in_q(headp, tailp, countp, n, deq)
  RF_DiskQueueData_t  **headp;
  RF_DiskQueueData_t  **tailp;
  int                  *countp;
  int                   n;
  int                   deq;
{
  RF_DiskQueueData_t *r, *s;
  int i;

  for(s=NULL,i=n,r=*headp;r;s=r,r=r->next) {
    if (i == 0)
      break;
    i--;
  }
  RF_ASSERT(r != NULL);
  if (deq == 0)
    return(r);
  if (s) {
    s->next = r->next;
  }
  else {
    *headp = r->next;
  }
  if (*tailp == r)
    *tailp = s;
  (*countp)--;
  return(r);
}
#endif

#if !defined(KERNEL) && RF_INCLUDE_QUEUE_RANDOM > 0
RF_DiskQueueData_t *rf_RandomPeek(q_in)
  void  *q_in;
{
  RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;
  RF_DiskQueueData_t *req;
  int n;

  if (q->hq_head) {
    n = q->rval % q->hq_count;
    req = n_in_q(&q->hq_head, &q->hq_tail, &q->hq_count, n, 0);
  }
  else {
    RF_ASSERT(q->hq_count == 0);
    if (q->lq_head == NULL) {
      RF_ASSERT(q->lq_count == 0);
      return(NULL);
    }
    n = q->rval % q->lq_count;
    req = n_in_q(&q->lq_head, &q->lq_tail, &q->lq_count, n, 0);
  }
  RF_ASSERT((q->hq_count + q->lq_count) == req->queue->queueLength);
  RF_ASSERT(req != NULL);
  return(req);
}

RF_DiskQueueData_t *rf_RandomDequeue(q_in)
  void  *q_in;
{
  RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;
  RF_DiskQueueData_t *req;
  int n;

  if (q->hq_head) {
    n = q->rval % q->hq_count;
    q->rval = (long)RF_STATIC_RANDOM();
    req = n_in_q(&q->hq_head, &q->hq_tail, &q->hq_count, n, 1);
  }
  else {
    RF_ASSERT(q->hq_count == 0);
    if (q->lq_head == NULL) {
      RF_ASSERT(q->lq_count == 0);
      return(NULL);
    }
    n = q->rval % q->lq_count;
    q->rval = (long)RF_STATIC_RANDOM();
    req = n_in_q(&q->lq_head, &q->lq_tail, &q->lq_count, n, 1);
  }
  RF_ASSERT((q->hq_count + q->lq_count) == (req->queue->queueLength-1));
  return(req);
}
#endif /* !KERNEL && RF_INCLUDE_QUEUE_RANDOM > 0 */

/* Return ptr to item at head of queue.  Used to examine request
 * info without actually dequeueing the request.
 */
RF_DiskQueueData_t *rf_FifoPeek(void *q_in)
{
  RF_DiskQueueData_t *headElement = NULL;
  RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;

  RF_ASSERT(q);
  if (q->hq_head)
    headElement = q->hq_head;
  else if (q->lq_head)
    headElement = q->lq_head;
  return(headElement);
}

/* We sometimes need to promote a low priority access to a regular priority access.
 * Currently, this is only used when the user wants to write a stripe which is currently
 * under reconstruction.
 * This routine will promote all accesses tagged with the indicated parityStripeID from
 * the low priority queue to the end of the normal priority queue.
 * We assume the queue is locked upon entry.
 */
int rf_FifoPromote(q_in, parityStripeID, which_ru)
  void               *q_in;
  RF_StripeNum_t      parityStripeID;
  RF_ReconUnitNum_t   which_ru;
{
  RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;
  RF_DiskQueueData_t *lp = q->lq_head, *pt = NULL;  /* lp = lo-pri queue pointer, pt = trailer */
  int retval = 0;

  while (lp) {

    /* search for the indicated parity stripe in the low-pri queue */
    if (lp->parityStripeID == parityStripeID && lp->which_ru == which_ru) {
      /*printf("FifoPromote:  promoting access for psid %ld\n",parityStripeID);*/
      if (pt) pt->next = lp->next;                              /* delete an entry other than the first */
      else q->lq_head = lp->next;                               /* delete the head entry */
      
      if (!q->lq_head) q->lq_tail = NULL;                       /* we deleted the only entry */
      else if (lp == q->lq_tail) q->lq_tail = pt;               /* we deleted the tail entry */
      
      lp->next = NULL;
      q->lq_count--;
      
      if (q->hq_tail) {q->hq_tail->next = lp; q->hq_tail = lp;} /* append to hi-priority queue */
      else {q->hq_head = q->hq_tail = lp;}
      q->hq_count++;

      /*UpdateShortestSeekFinishTimeForced(lp->requestPtr, lp->diskState);*/     /* deal with this later, if ever */

      lp = (pt) ? pt->next : q->lq_head;		       /* reset low-pri pointer and continue */
      retval++;
      
    } else {pt = lp; lp = lp->next;}
  }
  
  /* sanity check.  delete this if you ever put more than one entry in the low-pri queue */
  RF_ASSERT(retval == 0 || retval == 1);
  if (rf_fifoDebug) {
    int tid;
    rf_get_threadid(tid);
    printf("[%d] fifo: promote %d\n", tid, retval);
  }
  return(retval);
}
