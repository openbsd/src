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

RCSID("$arla: rx_multi.c,v 1.6 2000/03/03 08:56:12 assar Exp $");

/* multi.c and multi.h, together with some rxgen hooks, provide a way of making multiple, but similar, rx calls to multiple hosts simultaneously */

struct multi_handle *
multi_Init(struct rx_connection **conns, int nConns)
{
    struct rx_call **calls;
    short *ready;
    struct multi_handle *mh;
    int i;

    /*
     * Note:  all structures that are possibly referenced by other processes
     * must be allocated.  In some kernels variables allocated on a process
     * stack will not be accessible to other processes
     */
    calls = (struct rx_call **) osi_Alloc(sizeof(struct rx_call *) * nConns);
    ready = (short *) osi_Alloc(sizeof(short *) * nConns);
    mh = (struct multi_handle *) osi_Alloc(sizeof(struct multi_handle));
    if (!calls || !ready || !mh)
	osi_Panic("multi_Rx: no mem\n");
    mh->calls = calls;
    mh->nextReady = mh->firstNotReady = mh->ready = ready;
    mh->nReady = 0;
    mh->nConns = nConns;
    for (i = 0; i < nConns; i++) {
	struct rx_call *call;

	call = mh->calls[i] = rx_NewCall(conns[i]);
	rx_SetArrivalProc(call, multi_Ready, (void *) mh, (void *) i);
    }
    return mh;
}

/*
 * Return the user's connection index of the most recently ready call; that
 * is, a call that has received at least one reply packet 
 */
int 
multi_Select(struct multi_handle *mh)
{
    SPLVAR;
    NETPRI;
    if (mh->nextReady == mh->firstNotReady) {
	if (mh->nReady == mh->nConns) {
	    USERPRI;
	    return -1;
	}
	osi_rxSleep(mh);
    }
    USERPRI;
    return *mh->nextReady++;
}

/*
 * Called by Rx when the first reply packet of a call is received, or the
 * call is aborted. 
 */
void 
multi_Ready(struct rx_call *call, struct multi_handle *mh,
	    int index)
{
    *mh->firstNotReady++ = index;
    mh->nReady++;
    osi_rxWakeup(mh);
}

/*
 * Called when the multi rx call is over, or when the user aborts it 
 * (by using the macro multi_Abort)
 */
void 
multi_Finalize(struct multi_handle *mh)
{
    int i;
    int nCalls = mh->nConns;

    for (i = 0; i < nCalls; i++) {
	struct rx_call *call = mh->calls[i];

	if (call)
	    rx_EndCall(call, RX_USER_ABORT);
    }
    osi_Free(mh->calls, sizeof(struct rx_call *) * nCalls);
    osi_Free(mh->ready, sizeof(short *) * nCalls);
}
