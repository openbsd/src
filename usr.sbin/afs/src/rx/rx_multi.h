/* $arla: rx_multi.h,v 1.4 2003/01/19 08:48:22 lha Exp $ */

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

#ifndef	_RX_MULTI_
#define _RX_MULTI_

#ifdef	KERNEL
#include "../rx/rx.h"
#else				       /* KERNEL */
#include "rx.h"
#endif				       /* KERNEL */

struct multi_handle {
    int nConns;
    struct rx_call **calls;
    short *ready;
    short nReady;		       /* XXX UNALIGNED */
    short *nextReady;
    short *firstNotReady;
};

void
multi_Finalize (struct multi_handle*);

struct multi_handle *
multi_Init (struct rx_connection **, int);

void
multi_Ready (struct rx_call*, struct multi_handle*, int);

int
multi_Select (struct multi_handle *);

#define multi_Rx(conns, nConns) \
    do {\
	struct multi_handle *multi_h;\
	int multi_i;\
	struct rx_call *multi_call;\
	multi_h = multi_Init(conns, nConns);\
	for (multi_i = 0; multi_i < nConns; multi_i++)

#define multi_Body(startProc, endProc)\
	multi_call = multi_h->calls[multi_i];\
	startProc;\
	rx_FlushWrite(multi_call);\
	}\
	while ((multi_i = multi_Select(multi_h)) >= 0) {\
	    uint32_t multi_error;\
	    multi_call = multi_h->calls[multi_i];\
	    multi_error = rx_EndCall(multi_call, endProc);\
	    multi_h->calls[multi_i] = (struct rx_call *) 0

#define	multi_Abort break

#define multi_End\
	multi_Finalize(multi_h);\
    } while (0)


#endif				       /* _RX_MULTI_	 End of rx_multi.h */
