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

/* Elapsed time package */

#include "rx_locl.h"

RCSID("$arla: rx_clock.c,v 1.15 2000/11/26 22:29:28 lha Exp $");

#ifndef KERNEL

struct clock clock_now;

int clock_haveCurrentTime;

int clock_nUpdates;

/* Magic tdiff that guarantees a monotonically increasing clock value. */ 
static struct clock tdiff;

void
clock_Init(void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);
    tdiff.sec  = tv.tv_sec;
    tdiff.usec = tv.tv_usec;
    clock_now.sec = clock_now.usec = 0;
    clock_haveCurrentTime = 1;
    clock_nUpdates = 0;
}

/* Refresh value of clock_now. */
void 
clock_UpdateTime(void)
{
    struct timeval tv;
    struct clock t;
    
    gettimeofday(&tv, 0);
    t.sec  = tv.tv_sec;
    t.usec = tv.tv_usec;
    clock_Sub(&t, &tdiff);
    
    /* We can't have time running backwards!!! */
    if (clock_Le(&t, &clock_now))
    {
	/* Calculate new tdiff. */
	t.sec = tv.tv_sec;
	t.usec = tv.tv_usec;
	clock_Sub(&t, &clock_now);
	tdiff.sec  = t.sec;
	tdiff.usec = t.usec;
	
	/* Fake new time. */
	t.sec = tv.tv_sec;
	t.usec = tv.tv_usec;
	clock_Sub(&t, &tdiff);
	t.usec++;
	if (t.usec >= 1000000)
	{
	    t.sec += 1;
	    t.usec = 0;
	}
    }
    
    clock_now = t;
    clock_haveCurrentTime = 1;
    clock_nUpdates++;
}

void
clock_ReInit(void)
{
}

#endif				       /* KERNEL */
