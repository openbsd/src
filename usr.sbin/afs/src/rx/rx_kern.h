/*	$OpenBSD: rx_kern.h,v 1.1.1.1 1998/09/14 21:53:15 art Exp $	*/
/* $KTH: rx_kern.h,v 1.3 1998/02/22 19:46:53 joda Exp $ */

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

/* Definitions specific to the in-kernel implementation of Rx, for in-kernel clients */

#ifndef __RX_KERNEL_INCL_
#define	__RX_KERNEL_INCL_   1

#ifdef AFS_AIX32_ENV
/*
 * vrmix has some rather peculiar ideas about vax-style processor levels,
 * as evidenced in "net/spl.h".
 */
#include "net/spl.h"
#endif

extern int (*rxk_GetPacketProc) ();    /* set to packet allocation procedure */
extern int (*rxk_PacketArrivalProc) ();

#ifdef	AFS_SUN5_ENV
#define	SPLVAR
#define	NETPRI
#define	USERPRI
#else
#define	SPLVAR	    register int splvar
#define	NETPRI	    splvar=splnet()
#define	USERPRI	    splx(splvar)
#endif

rxi_StartListener(void);

#define	rxi_ReScheduleEvents	0      /* Not needed by kernel */

/* This is a no-op, because the kernel server procs are pre-allocated */
#define rxi_StartServerProcs(x)

/* Socket stuff */
typedef struct socket *osi_socket;

#define	OSI_NULLSOCKET	((osi_socket) 0)

extern rx_ReScheduleEvents();
extern osi_socket rxi_GetUDPSocket();

#define	osi_Msg printf)(
#if !defined(AFS_SGI_ENV) && !defined(AFS_SUN5_ENV)
#define	osi_rxSleep(a)	osi_Sleep(a)
#define	osi_rxWakeup(a)	osi_Wakeup(a)
#endif
#if !defined(AFS_SGI_ENV)
extern int printf();

#endif

#define	osi_YieldIfPossible()
#define	osi_WakeupAndYieldIfPossible(x)	    rx_Wakeup(x)

#include "../afs/longc_procs.h"

#endif				       /* __RX_KERNEL_INCL_ */
