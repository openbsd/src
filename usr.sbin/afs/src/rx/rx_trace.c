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

RCSID("$arla: rx_trace.c,v 1.8 2002/04/28 22:19:18 lha Exp $");

#ifdef RXTRACEON
char rxi_tracename[80] = "/tmp/rxcalltrace";

#else
char rxi_tracename[80] = "\0Change This pathname (and preceding NUL) to initiate tracing";

#endif

#ifdef RXDEBUG

int rxi_logfd = 0;
char rxi_tracebuf[4096];
unsigned long rxi_tracepos = 0;

struct rx_trace rxtinfo;

void 
rxi_flushtrace(void)
{
    write(rxi_logfd, rxi_tracebuf, rxi_tracepos);
    rxi_tracepos = 0;
}

void 
rxi_calltrace(unsigned int event, struct rx_call *call)
{
    struct clock now;

    if (!rxi_tracename[0])
	return;

    if (!rxi_logfd) {
	rxi_logfd = open(rxi_tracename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	if (!rxi_logfd)
	    rxi_tracename[0] = '\0';
    }
    clock_GetTime(&now);

    rxtinfo.event = event;
    rxtinfo.now = now.sec * 1000 + now.usec / 1000;
    rxtinfo.cid = call->conn->cid;
    rxtinfo.call = *(call->callNumber);
    rxtinfo.qlen = rx_nWaiting;
    rxtinfo.servicetime = 0;
    rxtinfo.waittime = 0;

    switch (event) {
    case RX_CALL_END:
	clock_Sub(&now, &(call->traceStart));
	rxtinfo.servicetime = now.sec * 10000 + now.usec / 100;
	if (call->traceWait.sec) {
	    now = call->traceStart;
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	call->traceWait.sec = call->traceWait.usec =
	    call->traceStart.sec = call->traceStart.usec = 0;
	break;

    case RX_CALL_START:
	call->traceStart = now;
	if (call->traceWait.sec) {
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	break;

    case RX_TRACE_DROP:
	if (call->traceWait.sec) {
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	break;

    case RX_CALL_ARRIVAL:
	call->traceWait = now;
    default:
	break;
    }

    memcpy(rxi_tracebuf + rxi_tracepos, &rxtinfo, sizeof(struct rx_trace));
    rxi_tracepos += sizeof(struct rx_trace);
    if (rxi_tracepos >= (4096 - sizeof(struct rx_trace)))
	rxi_flushtrace();
}

#endif /* RXDEBUG */

