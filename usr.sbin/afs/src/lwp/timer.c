/*	$OpenBSD: timer.c,v 1.1.1.1 1998/09/14 21:53:13 art Exp $	*/
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

/*******************************************************************\
* 								    *
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: timer.c,v 1.5 1998/05/18 05:44:41 art Exp $");
#endif

#include <sys/time.h>
#define _TIMER_IMPL_
#include "timer.h"
#include "q.h"

#include <stdlib.h>
#include <stdio.h>

#define FALSE	0
#define TRUE	1

#define NIL	0

#define expiration TotalTime

#define new_elem()	((struct TM_Elem *) malloc(sizeof(struct TM_Elem)))

#define MILLION	1000000

static int globalInitDone = 0;

/* t1 = t2 - t3 */

static void
subtract(register struct timeval *t1, 
	 register struct timeval *t2,
	 register struct timeval *t3)
{
    register int sec2, usec2, sec3, usec3;

    sec2 = t2 -> tv_sec;
    usec2 = t2 -> tv_usec;
    sec3 = t3 -> tv_sec;
    usec3 = t3 -> tv_usec;

    /* Take care of the probably non-existent case where the
     * usec field has more than 1 second in it. */

    while (usec3 > usec2) {
	usec2 += MILLION;
	sec2--;
    }

    /* Check for a negative time and use zero for the answer,
     * since the tv_sec field is unsigned */

    if (sec3 > sec2) {
        t1 -> tv_usec = 0;
        t1 -> tv_sec =  (unsigned long) 0;
    } else {
        t1 -> tv_usec = usec2 - usec3;
        t1 -> tv_sec = sec2 - sec3;
   }
}

/* t1 += t2; */

static void
add(register struct timeval *t1, register struct timeval *t2)
{
    t1 -> tv_usec += t2 -> tv_usec;
    t1 -> tv_sec += t2 -> tv_sec;
    if (t1->tv_usec >= MILLION) {
	t1 -> tv_sec ++;
	t1 -> tv_usec -= MILLION;
    }
}

/* t1 == t2 */

bool 
TM_eql(register struct timeval *t1, register struct timeval *t2)
{
    return (t1->tv_usec == t2->tv_usec) && (t1->tv_sec == t2->tv_sec);
}

/* t1 >= t2 */

/*
obsolete, commentless procedure, all done by hand expansion now.
static bool 
geq(register struct timeval *t1, register struct timeval *t2)
{
    return (t1->tv_sec > t2->tv_sec) ||
	   (t1->tv_sec == t2->tv_sec && t1->tv_usec >= t2->tv_usec);
}
*/

static bool
blocking(register struct TM_Elem *t)
{
    return (t->TotalTime.tv_sec < 0 || t->TotalTime.tv_usec < 0);
}



/*
 *    Initializes a list -- returns -1 if failure, else 0.
 */ 

int 
TM_Init(register struct TM_Elem **list)
{
    if (!globalInitDone) {
	FT_Init (0, 0);
	globalInitDone = 1;
    }
    *list = new_elem();
    if (*list == NIL)
	return -1;
    else {
	(*list) -> Next = *list;
	(*list) -> Prev = *list;
	(*list) -> TotalTime.tv_sec = 0;
	(*list) -> TotalTime.tv_usec = 0;
	(*list) -> TimeLeft.tv_sec = 0;
	(*list) -> TimeLeft.tv_usec = 0;
	(*list) -> BackPointer = NIL;

	return 0;
    }
}

int
TM_Final(register struct TM_Elem **list)
{
    if (list == NIL || *list == NIL)
	return -1;
    else {
	free(*list);
	*list = NIL;
	return 0;
    }
}

/*
 * Inserts elem into the timer list pointed to by *tlistPtr.
 */

void
TM_Insert(struct TM_Elem *tlistPtr, struct TM_Elem *elem)
/* pointer to head pointer of timer list */
/* element to be inserted */
{
    register struct TM_Elem *next;

    /* TimeLeft must be set for function IOMGR with infinite timeouts */
    elem -> TimeLeft = elem -> TotalTime;

    /* Special case -- infinite timeout */
    if (blocking(elem)) {
	lwp_insque(elem, tlistPtr->Prev);
	return;
    }

    /* Finite timeout, set expiration time */
    FT_AGetTimeOfDay(&elem->expiration, 0);
    add(&elem->expiration, &elem->TimeLeft);
    next = NIL;
    FOR_ALL_ELTS(p, tlistPtr, {
	if (blocking(p) || !(elem->TimeLeft.tv_sec > p->TimeLeft.tv_sec ||
	    (elem->TimeLeft.tv_sec == p->TimeLeft.tv_sec && elem->TimeLeft.tv_usec >= p->TimeLeft.tv_usec))
	    ) {
		next = p; /* Save ptr to element that will be after this one */
		break;
	}
     })

    if (next == NIL) next = tlistPtr;
    lwp_insque(elem, next->Prev);
}

/*
 * Walks through the specified list and updates the TimeLeft fields in it.
 * Returns number of expired elements in the list.
 */

int
TM_Rescan(struct TM_Elem *tlist)
/* head pointer of timer list */
{
    struct timeval time;
    register int expired;

    FT_AGetTimeOfDay(&time, 0);
    expired = 0;
    FOR_ALL_ELTS(e, tlist, {
	if (!blocking(e)) {
	    subtract(&e->TimeLeft, &e->expiration, &time);
	    if (0 > e->TimeLeft.tv_sec || (0 == e->TimeLeft.tv_sec && 0 >=
					   e->TimeLeft.tv_usec))
		expired++;
	}
    })
    return expired;
}
    
/*
 *  RETURNS POINTER TO earliest expired entry from tlist.
 *  Returns 0 if no expired entries are present.
 */

struct TM_Elem *
TM_GetExpired(struct TM_Elem *tlist)
/* head pointer of timer list */
{
    FOR_ALL_ELTS(e, tlist, {
	if (!blocking(e) &&
	    (0 > e->TimeLeft.tv_sec || (0 == e->TimeLeft.tv_sec 
					&& 0 >= e->TimeLeft.tv_usec)))
		return e;
    })
    return NIL;
}
    
/*
 *  Returns a pointer to the earliest unexpired element in tlist.
 *  Its TimeLeft field will specify how much time is left.
 *  Returns 0 if tlist is empty or if there are no unexpired elements.
 */

struct TM_Elem *
TM_GetEarliest(struct TM_Elem *tlist)
{
    register struct TM_Elem *e;

    e = tlist -> Next;
    return (e == tlist ? NIL : e);
}
