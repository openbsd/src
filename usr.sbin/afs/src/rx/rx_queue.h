/* $arla: /afs/stacken.kth.se/src/SourceRepository/arla/rx/rx_queue.h,v 1.4 2000/10/02 21:08:29 haba Exp $ */
/* $arla: /afs/stacken.kth.se/src/SourceRepository/arla/rx/rx_queue.h,v $ */

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

/* queue.h:  Simple double linked queue package */

/* It's simple, but, I think, it's pretty nice to use, and it's *very* efficient (especially so with a good optimizing compiler).   WARNING:  Since these functions are implemented as macros, it is best to use only *VERY* simple expressions for all parameters.  Double warning:  this uses a lot of type coercion, so you have to be *REAL* careful.  But C doesn't give me a reasonable alternative (i.e.. in-line expanded functions). */

/* Author: Bob Sidebotham, ITC */

#ifndef _RX_QUEUE_
#define _RX_QUEUE_

/* A queue head is simply a queue element linked to itself (i.e. the null queue is a queue with exactly one element).  Queue elements can be prepended to any structure:  these macros assume that the structure passed is coercible to a (struct q).  Since all of these operations are implemented as macros, the user should beware of side-effects in macro parameters.  Also beware that implicit casting of queue types occurs, so be careful to supply the right parameters at the right times! */
#undef queue			       /* Since some OS (ultrix, etc) have
				        * their own */
struct rx_queue {
    struct rx_queue *prev;
    struct rx_queue *next;
};

/* Sample usages:

(*A queue head:*)
struct rx_queue myqueue;

(*An element for my queue type:*)
struct myelement {
    struct rx_queue queue_header;
    int mydata;
};

(*Initialize the queue:*)
queue_Init(&myqueue);

(*Append a bunch of items to the queue:*)
for (i=0; i<20; i++) {
    struct myelement *item = (struct myelement *) malloc(sizeof *item);
    item->mydata = i;
    queue_Append(&myqueue, item);
}

(*Scan a queue, incrementing the mydata field in each element, and removing any entries for which mydata>MAX.  Nqe is used by the scan to hold the next queue element, so the current queue element may be removed safely. *)
struct myelement *qe, *nqe;
for (queue_Scan(&myqueue, qe, nqe, myelement)) {
    if (++qe->mydata > MAX)  queue_Remove(qe);
}

(* Count the number of elements in myqueue.  The queue_Scan macro specifies all three elements of the for loop, but an additional initializer and an additional incrementor can be added *)
struct myelement *qe, *nqe;
int n;
for (n=0, queue_Scan(&myqueue, qe, nqe, myelement), n++) {}

*/

/* INTERNAL macros */

/* This one coerces the user's structure to a queue element (or queue head) */
#define	_RX_QUEUE(x) ((struct rx_queue *)(x))

/* This one adds a queue element (i) before or after another queue element (or queue head) (q), doubly linking everything together.  It's called by the user usable macros, below.  If (a,b) is (next,prev) then the element i is linked after q; if it is (prev,next) then it is linked before q */
/* N.B.  I don't think it is possible to write this expression, correctly, with less than one comma (you can easily write an alternative expression with no commas that works with most or all compilers, but it's not clear that it really is un-ambiguous, legal C-code). */
#define _QA(q,i,a,b) (((i->a=q->a)->b=i)->b=q, q->a=i)

/* These ones splice two queues together.  If (a,b) is (next,prev) then (*q2) is appended to (*q1), otherwise (*q2) is prepended to (*q1). */
#define _QS(q1,q2,a,b) if (queue_IsEmpty(q2)); else \
    ((((q2->a->b=q1)->a->b=q2->b)->a=q1->a, q1->a=q2->a), queue_Init(q2))

/* Basic remove operation.  Doesn't update the queue item to indicate it's been removed */
#define _QR(i) ((_RX_QUEUE(i)->prev->next=_RX_QUEUE(i)->next)->prev=_RX_QUEUE(i)->prev)

/* EXPORTED macros */

/* Initialize a queue head (*q).  A queue head is just a queue element */
#define queue_Init(q) (_RX_QUEUE(q))->prev = (_RX_QUEUE(q))->next = (_RX_QUEUE(q))

/* Prepend a queue element (*i) to the head of the queue, after the queue head (*q).  The new queue element should not currently be on any list. */
#define queue_Prepend(q,i) _QA(_RX_QUEUE(q),_RX_QUEUE(i),next,prev)

/* Append a queue element (*i) to the end of the queue, before the queue head (*q).  The new queue element should not currently be on any list. */
#define queue_Append(q,i) _QA(_RX_QUEUE(q),_RX_QUEUE(i),prev,next)

/* Insert a queue element (*i2) before another element (*i1) in the queue.  The new queue element should not currently be on any list. */
#define queue_InsertBefore(i1,i2) _QA(_RX_QUEUE(i1),_RX_QUEUE(i2),prev,next)

/* Insert a queue element (*i2) after another element (*i1) in the queue.  The new queue element should not currently be on any list. */
#define queue_InsertAfter(i1,i2) _QA(_RX_QUEUE(i1),_RX_QUEUE(i2),next,prev)

/* Spice the members of (*q2) to the beginning of (*q1), re-initialize (*q2) */
#define queue_SplicePrepend(q1,q2) _QS(_RX_QUEUE(q1),_RX_QUEUE(q2),next,prev)

/* Splice the members of queue (*q2) to the end of (*q1), re-initialize (*q2) */
#define queue_SpliceAppend(q1,q2) _QS(_RX_QUEUE(q1),_RX_QUEUE(q2),prev,next)

/* Replace the queue (*q1) with the contents of the queue (*q2), re-initialize (*q2) */
#define queue_Replace(q1,q2) if (queue_IsEmpty(q2)) queue_Init(q1); else \
    (*_RX_QUEUE(q1) = *_RX_QUEUE(q2), _RX_QUEUE(q1)->next->prev = _RX_QUEUE(q1)->prev->next = _RX_QUEUE(q1), queue_Init(q2))

/* Remove a queue element (*i) from it's queue.  The next and prev field is 0'd, so that any further use of this q entry will hopefully cause a core dump.  Multiple removes of the same queue item are not supported */
#define queue_Remove(i) (_QR(i), _RX_QUEUE(i)->next = 0, _RX_QUEUE(i)->prev = 0 )

/* Move the queue element (*i) from it's queue to the end of the queue (*q) */
#define queue_MoveAppend(q,i) (_QR(i), queue_Append(q,i))

/* Move the queue element (*i) from it's queue to the head of the queue (*q) */
#define queue_MovePrepend(q,i) (_QR(i), queue_Prepend(q,i))

/* Return the first element of a queue, coerced too the specified structure s */
/* Warning:  this returns the queue head, if the queue is empty */
#define queue_First(q,s) ((struct s *)_RX_QUEUE(q)->next)

/* Return the last element of a queue, coerced to the specified structure s */
/* Warning:  this returns the queue head, if the queue is empty */
#define queue_Last(q,s) ((struct s *)_RX_QUEUE(q)->prev)

/* Return the next element in a queue, beyond the specified item, coerced to the specified structure s */
/* Warning:  this returns the queue head, if the item specified is the last in the queue */
#define queue_Next(i,s) ((struct s *)_RX_QUEUE(i)->next)

/* Return the previous element to a specified element of a queue, coerced to the specified structure s */
/* Warning:  this returns the queue head, if the item specified is the first in the queue */
#define queue_Prev(i,s) ((struct s *)_RX_QUEUE(i)->prev)

/* Return true if the queue is empty, i.e. just consists of a queue head.  The queue head must have been initialized some time prior to this call */
#define queue_IsEmpty(q) (_RX_QUEUE(q)->next == _RX_QUEUE(q))

/* Return true if the queue is non-empty, i.e. consists of a queue head plus at least one queue item */
#define queue_IsNotEmpty(q) (_RX_QUEUE(q)->next != _RX_QUEUE(q))

/* Return true if the queue item is currently in a queue */
/* Returns false if the item was removed from a queue OR is uninitialized (zero) */
#define queue_IsOnQueue(i) (_RX_QUEUE(i)->next != 0)

/* Returns true if the queue item (i) is the first element of the queue (q) */
#define queue_IsFirst(q,i) (_RX_QUEUE(q)->first == _RX_QUEUE(i))

/* Returns true if the queue item (i) is the last element of the queue (q) */
#define queue_IsLast(q,i) (_RX_QUEUE(q)->prev == _RX_QUEUE(i))

/* Returns true if the queue item (i) is the end of the queue (q), that is, i is the head of the queue */
#define queue_IsEnd(q,i) (_RX_QUEUE(q) == _RX_QUEUE(i))

/* Prototypical loop to scan an entire queue forwards.  q is the queue
 * head, qe is the loop variable, next is a variable used to store the
 * queue entry for the next iteration of the loop, s is the user's
 * queue structure name.  Called using "for (queue_Scan(...)) {...}".
 * Note that extra initializers can be added before the queue_Scan,
 * and additional expressions afterwards.  So "for (sum=0,
 * queue_Scan(...), sum += value) {value = qe->value}" is possible.
 * If the current queue entry is deleted, the loop will work
 * correctly.  Care must be taken if other elements are deleted or
 * inserted.  Next may be updated within the loop to alter the item
 * used in the next iteration. */
#define	queue_Scan(q, qe, next,	s)			\
    (qe) = queue_First(q, s), next = queue_Next(qe, s);	\
	!queue_IsEnd(q,	qe);				\
	(qe) = (next), next = queue_Next(qe, s)

/* This is similar to queue_Scan, but scans from the end of the queue to the beginning.  Next is the previous queue entry.  */
#define	queue_ScanBackwards(q, qe, prev, s)		\
    (qe) = queue_Last(q, s), prev = queue_Prev(qe, s);	\
	!queue_IsEnd(q,	qe);				\
	(qe) = prev, prev = queue_Prev(qe, s)

#endif				       /* _RX_QUEUE_ */
