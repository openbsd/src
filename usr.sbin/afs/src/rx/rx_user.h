/*	$OpenBSD: rx_user.h,v 1.1.1.1 1998/09/14 21:53:17 art Exp $	*/
/* $KTH: rx_user.h,v 1.5 1998/03/28 16:35:47 lha Exp $ */

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

#ifdef	VALIDATE
error - foo error - foo error - foo
#endif				       /* VALIDATE */
#ifndef RX_USER_INCLUDE
#define RX_USER_INCLUDE

/* rx_user.h:  definitions specific to the user-level implementation of Rx */

#include <stdio.h>
#include <lwp.h>

#ifdef RXDEBUG
extern FILE *rx_debugFile;

#endif

/* These routines are no-ops in the user level implementation */
#define SPLVAR
#define NETPRI
#define USERPRI
#if defined(AFS_SGIMP_ENV)
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define ISAFS_GLOCK()
#endif

extern PROCESS rx_listenerPid;	       /* LWP process id of socket listener
				        * process */
void rxi_StartListener(void);
void rxi_StartServerProcs(int);
void rxi_ReScheduleEvents(void);

void rxi_PacketsUnWait(void);

/*
 *Some "operating-system independent" stuff, for the user mode implementation
 */
typedef short osi_socket;

osi_socket rxi_GetUDPSocket(u_short);

#define	OSI_NULLSOCKET	((osi_socket) -1)

#define	rx_Sleep(x)		    osi_Sleep(x)
#define	rx_Wakeup(x)		    osi_Wakeup(x)
#define	osi_rxSleep(x)		    osi_Sleep(x)
#define	osi_rxWakeup(x)		    osi_Wakeup(x)
#define	osi_Sleep(x)		    LWP_WaitProcess(x)
#define	osi_Wakeup(x)		    LWP_NoYieldSignal(x)
/* osi_WakeupAndYieldIfPossible doesn't actually have to yield, but its better if it does */
#define	osi_WakeupAndYieldIfPossible(x)	    LWP_SignalProcess(x)
#define	osi_YieldIfPossible()	    LWP_DispatchProcess();

#ifndef	AFS_AIX32_ENV

#ifndef osi_Alloc
#define	osi_Alloc(size)		    ((char *) malloc(size))
#endif

#ifndef osi_Free
#define	osi_Free(ptr, size)	    free((char *)(ptr))
#endif

#endif

#ifdef USE_MMAPTIME
#define	osi_GetTime(timevalptr)	    mmaptime_gettimeofday(timevalptr, 0)
#else
#define	osi_GetTime(timevalptr)	    gettimeofday(timevalptr, 0)
#endif

/* Just in case it's possible to distinguish between relatively long-lived stuff and stuff which will be freed very soon, but which needs quick allocation (e.g. dynamically allocated xdr things) */
#define	osi_QuickFree(ptr, size)    osi_Free(ptr, size)
#define	osi_QuickAlloc(size)	    osi_Alloc(size)

void osi_Panic(const char *fmt, ...);

int fprintf(FILE *, const char *, ...);

#define	osi_Msg			    fprintf)(stderr,

#endif				       /* RX_USER_INCLUDE */
