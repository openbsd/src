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

/* $arla: rx_trace.h,v 1.5 2002/04/28 22:19:39 lha Exp $ */

#ifndef	_RX_TRACE
#define _RX_TRACE

#ifndef	RXDEBUG
#define rxi_calltrace(a,b)
#define rxi_flushtrace()
#else
void rxi_calltrace(unsigned int, struct rx_call*);
void rxi_flushtrace(void);

#define RX_CALL_ARRIVAL 0
#define RX_CALL_START 1
#define RX_CALL_END 2
#define RX_TRACE_DROP 3

struct rx_trace {
    unsigned long cid;
    unsigned short call;
    unsigned short qlen;
    unsigned long now;
    unsigned long waittime;
    unsigned long servicetime;
    unsigned long event;
};

extern char rxi_tracename[80];

#endif				       /* RXDEBUG */

#endif				       /* _RX_TRACE */
