/* $arla: /afs/stacken.kth.se/src/SourceRepository/arla/rx/rx_strm.h,v 1.3 1998/02/22 19:54:23 joda Exp $ */
/* $arla: /afs/stacken.kth.se/src/SourceRepository/arla/rx/rx_strm.h,v $ */

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

/* rx_stream.h:  the stream I/O layer for RX */

This file is now obsolete.



#ifndef _RX_STREAM_
#define _RX_STREAM_

#ifdef	KERNEL
#include "../rx/rx.h"
#else				       /* KERNEL */
#include <sys/types.h>
#include <sys/uio.h>
#include "rx.h"
#endif				       /* KERNEL */

/* Write descriptor */
struct rx_stream_wd {
    char *freePtr;		       /* Pointer to bytes in first packet */
    int nFree;			       /* Number of bytes free in first
				        * packet */
    struct rx_call *call;	       /* The call this stream is attached to */
    struct rx_queue wq;		       /* Currently allocated packets for
				        * this stream */
    int packetSize;		       /* Data size used in each packet */
};

/* Read descriptor */
struct rx_stream_rd {
    struct rx_packet *packet;	       /* The current packet */
    char *nextByte;		       /* Pointer to bytes in current packet */
    int nLeft;			       /* Number of bytes free in current
				        * packet */
    struct rx_call *call;	       /* The call this stream is attached to */
    struct rx_queue rq;		       /* Currently allocated packets for
				        * this stream */
    struct rx_queue freeTheseQ;	       /* These packets should be freed on
				        * the next operation */
};

/* Composite stream descriptor */
struct rx_stream {
    union {
	struct rx_stream_rd rd;
	struct rx_stream_wd wd;
    } sd;
};

/* Externals */
void rx_stream_InitWrite();
void rx_stream_InitRead();
void rx_stream_FinishWrite();
void rx_stream_FinishRead();
int rx_stream_Read();
int rx_stream_Write();
int rx_stream_AllocIov();

/* Write nbytes of data to the write stream.  Returns the number of bytes written */
/* If it returns 0, the call status should be checked with rx_Error. */
#define	rx_stream_Write(iod, buf, nbytes)				\
    (iod)->sd.wd.nFree > (nbytes) ?					\
	(buf) && memcpy((iod)->sd.wd.freePtr, (buf), (nbytes)),		\
	(iod)->sd.wd.nFree -= (nbytes),					\
	(iod)->sd.wd.freePtr += (nbytes), (nbytes)			\
      : rx_stream_WriteProc((iod), (buf), (nbytes))


/* Read nbytes of data from the read stream.  Returns the number of bytes read */
/* If it returns less than requested, the call status should be checked with rx_Error */
#define	rx_stream_Read(iod, buf, nbytes)					\
    (iod)->sd.rd.nLeft > (nbytes) ?						\
    memcpy((buf), (iod)->sd.rd.nextByte, (nbytes)),				\
    (iod)->sd.rd.nLeft -= (nbytes), (iod)->sd.rd.nextByte += (nbytes), (nbytes)	\
   : rx_stream_ReadProc((iod), (buf), (nbytes))

#endif				       /* _RX_STREAM_	 End of rx_stream.h */
