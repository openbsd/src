/*	$OpenBSD: iomgr.c,v 1.1.1.1 1998/09/14 21:53:11 art Exp $	*/
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
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/


/*
 *	IO Manager routines & server process for VICE server.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: iomgr.c,v 1.18 1998/07/09 19:56:27 art Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include "lwp.h"
#include <sys/time.h>
#include "timer.h"
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include "q.h"
#include <unistd.h>
#include <string.h>
#include <roken.h>

/* Prototypes */
static void SignalIO(int, fd_set *, fd_set *, fd_set *) ;
static void SignalTimeout(int fds, struct timeval *timeout) ;
static int SignalSignals (void);

int FT_GetTimeOfDay(register struct timeval *, register struct timezone *);

/********************************\
* 				 *
*  Stuff for managing IoRequests *
* 				 *
\********************************/

struct IoRequest {

    /* Pid of process making request (for use in IOMGR_Cancel) */
    PROCESS		pid;

    /* Descriptor masks for requests */
    fd_set		readfds;
    fd_set		writefds;
    fd_set		exceptfds;

    fd_set		*rfds;
    fd_set		*wfds;
    fd_set		*efds;

    int			nfds;

    struct TM_Elem	timeout;

    /* Result of select call */
    int result;
    struct IoRequest *next;
};

/********************************\
* 				 *
*  Stuff for managing signals    *
* 				 *
\********************************/

#define badsig(signo)		(((signo) <= 0) || ((signo) >= NSIG))
#define mysigmask(signo)		(1 << ((signo)-1))

extern int errno;

static long openMask;		/* mask of open files on an IOMGR abort */
static long sigsHandled;        /* sigmask(signo) is on if we handle signo */
static int anySigsDelivered;		/* true if any have been delivered. */
#ifdef AFS_POSIX_SIGNALS
static struct sigaction oldVecs[NSIG];  /* the old signal vectors */
#else
static struct sigvec oldVecs[NSIG];	/* the old signal vectors */
#endif
static char *sigEvents[NSIG];		/* the event to do an LWP signal on */
static int sigDelivered[NSIG];		/* 
					 * True for signals delivered so far.
					 * This is an int array to make sure
					 * there are no conflicts when trying 
					 * to write it 
					 */
/* software 'signals' */
#define NSOFTSIG		4
static void (*sigProc[NSOFTSIG])();
static char *sigRock[NSOFTSIG];


static struct IoRequest *iorFreeList = 0;

static struct TM_Elem *Requests;      /* List of requests */
static struct timeval iomgr_timeout;  /* global so signal handler can zap it */

/* stuff for debugging */
static int iomgr_errno;
static struct timeval iomgr_badtv;
static PROCESS iomgr_badpid;

void
FreeRequest(struct IoRequest *req)
{
    req->next = iorFreeList; 
    iorFreeList = req;
}

/* stuff for handling select fd_sets */

#ifndef FD_COPY
#define FD_COPY(f, t)	memcpy((t), (f), sizeof(*(f)))
#endif

/*
 * FD_OR - bitwise or of two fd_sets
 * The result goes into set1
 */
static void
FD_OR(fd_set *set1, fd_set *set2)
{
    int i;

#ifdef FD_SPEED_HACK
    unsigned long *s1 = (unsigned long *)set1;
    unsigned long *s2 = (unsigned long *)set2;

    for (i = 0; i < sizeof(fd_set)/sizeof(unsigned long); i++)
	s1[i] |= s2[i];
#else
    for (i = 0; i < FD_SETSIZE; i++)
        if (FD_ISSET(i, set1) || FD_ISSET(i, set2))
            FD_SET(i, set1);
#endif
}

/*
 * FD_AND - bitwise and of two fd_sets
 * The result goes into set1
 */
static void
FD_AND(fd_set *set1, fd_set *set2)
{
    int i;

#ifdef FD_SPEED_HACK
    unsigned long *s1 = (unsigned long *)set1;
    unsigned long *s2 = (unsigned long *)set2;

    for(i = 0; i < sizeof(fd_set)/sizeof(unsigned long); i++)
	s1[i] &= s2[i];
#else
    for(i = 0; i < FD_SETSIZE; i++)
	if (FD_ISSET(i, set1) && FD_ISSET(i, set2))
	    FD_SET(i, set1);
#endif
}

/*
 * FD_LOGAND - "logical" and of two fd_sets
 * returns 0 if there are no fds that are the same in both fd_sets
 * otherwise it returns 1
 */
static int
FD_LOGAND(fd_set *set1, fd_set *set2)
{
#ifdef FD_SPEED_HACK
    int i;

    unsigned long *s1 = (unsigned long *)set1;
    unsigned long *s2 = (unsigned long *)set2;

    for(i = 0; i < sizeof(fd_set)/sizeof(unsigned long); i++)
	if ((s1[i] & s2[i]) != 0)
	    return 1;
    return 0;
#else
    fd_set tmp;

    FD_COPY(set1, &tmp);
    FD_AND(&tmp, set2);

    return !FD_ISZERO(&tmp);
#endif
}

static struct IoRequest *
NewRequest(void)
{
    register struct IoRequest *request;

    if ((request = iorFreeList) != NULL) 
	iorFreeList = request->next;
    else 
	request = (struct IoRequest *) malloc(sizeof(struct IoRequest));

    return request;
}

#define Purge(list) FOR_ALL_ELTS(req, list, { free(req->BackPointer); })
#define MAX_FDS 32

/* The IOMGR process */

/*
 * Important invariant: process->iomgrRequest is null iff request not in
 * timer queue also, request->pid is valid while request is in queue, also,
 * don't signal selector while request in queue, since selector free's request.
 */

static void
IOMGR(char *dummy)
{
    for (;;) {
 /*
  * commented out to match use - kazar	
  * static struct timeval Poll = { 0, 0 }; 
  */
	int fds;
	int nfds;
	fd_set readfds, writefds, exceptfds;
	fd_set *rfds, *wfds, *efds;
	struct TM_Elem *earliest;
	struct timeval timeout, junk;
	bool woke_someone;

	/* 
	 * Wake up anyone who has expired or who has received a
	 * Unix signal between executions.  Keep going until we
	 * run out. 
	 */
	do {
	    woke_someone = FALSE;
	    /* Wake up anyone waiting on signals. */
	    /* Note: SignalSignals() may yield! */
	    if (anySigsDelivered && SignalSignals ())
		woke_someone = TRUE;
	    TM_Rescan(Requests);
	    for (;;) {
		register struct IoRequest *req;
		struct TM_Elem *expired;
		expired = TM_GetExpired(Requests);
		if (expired == NULL) break;
		woke_someone = TRUE;
		req = (struct IoRequest *) expired -> BackPointer;
#ifdef DEBUG
		if (lwp_debug != 0) puts("[Polling SELECT]");
#endif /* DEBUG */
		FD_ZERO(&req->readfds);
		FD_ZERO(&req->writefds);
		FD_ZERO(&req->exceptfds);
		/* no data ready */
		req->nfds = 0;
		req->rfds = req->wfds = req->efds = NULL;

		req->result = 0;				
                /* no fds ready */

		TM_Remove(Requests, &req->timeout);
#ifdef DEBUG
		req -> timeout.Next = (struct TM_Elem *) 2;
		req -> timeout.Prev = (struct TM_Elem *) 2;
#endif /* DEBUG */
		LWP_QSignal(req->pid);
		req->pid->iomgrRequest = 0;
	    }
	    if (woke_someone) LWP_DispatchProcess();
	} while (woke_someone);


	/* Collect requests & update times */
	FD_ZERO(&readfds);	/* XXX - should not be needed */
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	nfds = 0;
	rfds = wfds = efds = NULL;
	FOR_ALL_ELTS(r, Requests, {
	    register struct IoRequest *req;
	    req = (struct IoRequest *) r -> BackPointer;
	    if (req->rfds) {
		if (rfds)
		    FD_OR(rfds, req->rfds);
		else {
		    rfds = &readfds;
		    FD_COPY(req->rfds, rfds);
		}
	    }
		    
	    if (req->wfds) {
		if (wfds)
		    FD_OR(wfds, req->wfds);
		else {
		    wfds = &readfds;
		    FD_COPY(req->wfds, wfds);
		}
	    }

	    if (req->efds) {
		if (efds)
		    FD_OR(efds, req->efds);
		else {
		    efds = &readfds;
		    FD_COPY(req->efds, efds);
		}
	    }
	    nfds = max(nfds, req->nfds);
	})
	earliest = TM_GetEarliest(Requests);
	if (earliest != NULL) {
	    timeout = earliest -> TimeLeft;

	    /* Do select */
#ifdef DEBUG
	    if (lwp_debug != 0) {
		printf("[select(%d, %p, %p, %p, ", nfds, 
		       rfds, wfds, efds);
		if (timeout.tv_sec == -1 && timeout.tv_usec == -1)
		    puts("INFINITE)]");
		else
		    printf("<%d, %d>)]\n", timeout.tv_sec, timeout.tv_usec);
	    }
#endif /* DEBUG */
	    iomgr_timeout = timeout;
	    if (timeout.tv_sec == -1 && timeout.tv_usec == -1) {
		/* infinite, sort of */
		iomgr_timeout.tv_sec = 100000000;
		iomgr_timeout.tv_usec = 0;
	    }
	    /* 
	     * Check one last time for a signal delivery.  If one comes after
	     * this, the signal handler will set iomgr_timeout to zero, 
	     * causing the select to return immediately.  The timer package
	     * won't return a zero timeval because all of those guys were
	     * handled above.
	     *
	     * I'm assuming that the kernel masks signals while it's picking up
	     * the parameters to select.  This may a bad assumption.  -DN 
	     */
	    if (anySigsDelivered)
		continue;	/* go to the top and handle them. */

	    /* 
	     * select runs much faster if NULL's are passed instead of &0s 
	     */

	    fds = select(nfds, rfds, wfds, efds, &iomgr_timeout);

	    /*
	     * For SGI and SVR4 - poll & select can return EAGAIN ...
	     */
	    if (fds < 0 && errno != EINTR && errno != EAGAIN) {
		iomgr_errno = errno;
		for(fds = 0; fds < FD_SETSIZE; fds++) {
		    if (fcntl(fds, F_GETFD, 0) < 0 && errno == EBADF) 
			openMask |= (1<<fds);
		}
		abort();
	    }

	    /* force a new gettimeofday call so FT_AGetTimeOfDay calls work */
	    FT_GetTimeOfDay(&junk, 0);

	    /* See what happened */
	    if (fds > 0)
		/* Action -- wake up everyone involved */
		SignalIO(fds, rfds, wfds, efds);
	    else if (fds == 0
		&& (iomgr_timeout.tv_sec != 0 || iomgr_timeout.tv_usec != 0))
		/* Real timeout only if signal handler hasn't set
		   iomgr_timeout to zero. */
		SignalTimeout(fds, &timeout);

	}
	LWP_DispatchProcess();
    }
}

/************************\
* 			 *
*  Signalling routines 	 *
* 			 *
\************************/

static void
SignalIO(int fds, fd_set *rfds, fd_set *wfds, fd_set *efds)
{
    /* Look at everyone who's bit mask was affected */
    FOR_ALL_ELTS(r, Requests, {
	register struct IoRequest *req;
	register PROCESS pid;
	int doit = 0;
	req = (struct IoRequest *) r -> BackPointer;

	if (rfds && req->rfds && FD_LOGAND(req->rfds, rfds)) {
	    FD_AND(req->rfds, rfds);
	    doit = 1;
	}
	if (wfds && req->wfds && FD_LOGAND(req->wfds, wfds)) {
	    FD_AND(req->wfds, wfds);
	    doit = 1;
	}
	if (efds && req->efds && FD_LOGAND(req->efds, efds)) {
	    FD_AND(req->efds, efds);
	    doit = 1;
	}

	if (doit) {
	    req -> result = fds;
	    TM_Remove(Requests, &req->timeout);
	    LWP_QSignal(pid=req->pid);
	    pid->iomgrRequest = 0;

	}
    })
}

static void
SignalTimeout(int fds, register struct timeval *timeout)
{
    /* Find everyone who has specified timeout */
    FOR_ALL_ELTS(r, Requests, {
	register struct IoRequest *req;
	register PROCESS pid;
	req = (struct IoRequest *) r -> BackPointer;
	if (TM_eql(&r->TimeLeft, timeout)) {
	    req -> result = fds;
	    TM_Remove(Requests, &req->timeout);
	    LWP_QSignal(pid=req->pid);
	    pid->iomgrRequest = 0;
	} else
	    return;
    })
}

/*****************************************************\
*						      *
*  Signal handling routine (not to be confused with   *
*  signalling routines, above).			      *
*						      *
\*****************************************************/
static RETSIGTYPE
SigHandler (int signo)
{
    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
	return;		/* can't happen. */
    sigDelivered[signo] = TRUE;
    anySigsDelivered = TRUE;
    /* Make sure that the IOMGR process doesn't pause on the select. */
    iomgr_timeout.tv_sec = 0;
    iomgr_timeout.tv_usec = 0;
}

/* Alright, this is the signal signalling routine.  It delivers LWP signals
   to LWPs waiting on Unix signals. NOW ALSO CAN YIELD!! */
static int
SignalSignals (void)
{
    bool gotone = FALSE;
    register int i;
    register void (*p)();
    long stackSize;

    anySigsDelivered = FALSE;

    /* handle software signals */
    stackSize = (AFS_LWP_MINSTACKSIZE < lwp_MaxStackSeen? 
		 lwp_MaxStackSeen : AFS_LWP_MINSTACKSIZE);
    for (i=0; i < NSOFTSIG; i++) {
	PROCESS pid;
	if ((p = sigProc[i]) != NULL) /* This yields!!! */
	    LWP_CreateProcess(p, stackSize, LWP_NORMAL_PRIORITY, sigRock[i],
		"SignalHandler", &pid);
	sigProc[i] = 0;
    }

    for (i = 1; i <= NSIG; ++i)  /* forall !badsig(i) */
	if ((sigsHandled & mysigmask(i)) && sigDelivered[i] == TRUE) {
	    sigDelivered[i] = FALSE;
	    LWP_NoYieldSignal (sigEvents[i]);
	    gotone = TRUE;
	}
    return gotone;
}


/***************************\
* 			    *
*  User-callable routines   *
* 			    *
\***************************/


/* Keep IOMGR process id */
static PROCESS IOMGR_Id = NULL;

int
IOMGR_SoftSig(void (*aproc)(), char *arock)
{
    register int i;
    for (i=0;i<NSOFTSIG;i++) {
	if (sigProc[i] == 0) {
	    /* a free entry */
	    sigProc[i] = aproc;
	    sigRock[i] = arock;
	    anySigsDelivered = TRUE;
	    iomgr_timeout.tv_sec = 0;
	    iomgr_timeout.tv_usec = 0;
	    return 0;
	}
    }
    return -1;
}


int
IOMGR_Initialize(void)
{
    extern int TM_Init();
    PROCESS pid;

    /* If lready initialized, just return */
    if (IOMGR_Id != NULL) return LWP_SUCCESS;

    /* Init LWP if someone hasn't yet. */
    if (LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid) 
	!= LWP_SUCCESS)
	return -1;

    /* Initialize request lists */
    if (TM_Init(&Requests) < 0) return -1;

    /* Initialize signal handling stuff. */
    sigsHandled = 0;
    anySigsDelivered = TRUE; /* 
			      * A soft signal may have happened before
			      * IOMGR_Initialize:  so force a check for
			      * signals regardless 
			      */

    return LWP_CreateProcess(IOMGR, AFS_LWP_MINSTACKSIZE, 0, 0, 
			     "IO MANAGER", &IOMGR_Id);
}

int
IOMGR_Finalize(void)
{
    int status;

    Purge(Requests)
    TM_Final(&Requests);
    status = LWP_DestroyProcess(IOMGR_Id);
    IOMGR_Id = NULL;
    return status;
}

/* 
 * signal I/O for anyone who is waiting for a FD or a timeout;
 * not too cheap, since forces select and timeofday check 
 */
long
IOMGR_Poll(void)
{
    fd_set readfds, writefds, exceptfds;
    fd_set *rfds, *wfds, *efds;
    long code;
    struct timeval tv;
    int nfds;

    FT_GetTimeOfDay(&tv, 0);    /* force accurate time check */
    TM_Rescan(Requests);
    for (;;) {
	register struct IoRequest *req;
	struct TM_Elem *expired;
	expired = TM_GetExpired(Requests);
	if (expired == NULL) break;
	req = (struct IoRequest *) expired -> BackPointer;
#ifdef DEBUG
	if (lwp_debug != 0) puts("[Polling SELECT]");
#endif /* DEBUG */
	/* no data ready */

	FD_ZERO(&req->readfds);
	FD_ZERO(&req->writefds);
	FD_ZERO(&req->exceptfds);

	req->nfds = 0;
	req->rfds = req->wfds = req->efds = NULL;

	req->result = 0;	/* no fds ready */
	TM_Remove(Requests, &req->timeout);
#ifdef DEBUG
	req -> timeout.Next = (struct TM_Elem *) 2;
	req -> timeout.Prev = (struct TM_Elem *) 2;
#endif /* DEBUG */
	LWP_QSignal(req->pid);
	req->pid->iomgrRequest = 0;
    }

    /* Collect requests & update times */
    FD_ZERO(&readfds);		/* XXX - should not be needed */
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    rfds = wfds = efds = NULL;
    nfds = 0;
    FOR_ALL_ELTS(r, Requests, {
	struct IoRequest *req;
	req = (struct IoRequest *) r -> BackPointer;

	if (req->rfds) {
	    if (rfds)
		FD_OR(rfds, req->rfds);
	    else {
		rfds = &readfds;
		FD_COPY(req->rfds, rfds);
	    }
	}
		    
	if (req->wfds) {
	    if (wfds)
		FD_OR(wfds, req->wfds);
	    else {
		wfds = &readfds;
		FD_COPY(req->wfds, wfds);
	    }
	}

	if (req->efds) {
	    if (efds)
		FD_OR(efds, req->efds);
	    else {
		efds = &readfds;
		FD_COPY(req->efds, efds);
	    }
	}
	nfds = max(nfds, req->nfds);
    })
    
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    code = select(nfds, &readfds, &writefds, &exceptfds, &tv);

    if (code > 0) {
	SignalIO(code, rfds, wfds, efds);
    }

    LWP_DispatchProcess();  /* make sure others run */
    LWP_DispatchProcess();
    return 0;
}

int
IOMGR_Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, 
	     struct timeval *timeout)
{
    register struct IoRequest *request;
    int result;

    /* See if polling request. If so, handle right here */
    if (timeout != NULL) {
	if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
	    int fds;
#ifdef DEBUG
	    if (lwp_debug != 0) puts("[Polling SELECT]");
#endif /* DEBUG */

	    do {
		timeout->tv_sec = 0;
		timeout->tv_usec = 0;
		fds = select(nfds, readfds, writefds, exceptfds, 
			      timeout);
	    } while (fds < 0 && errno == EAGAIN);

	    return (fds > 1 ? 1 : fds);
	}
    }

    /* Construct request block & insert */
    request = NewRequest();
    if (readfds == NULL)
	request->rfds = NULL;
    else {
	request->rfds = &request->readfds;
	FD_COPY(readfds, request->rfds);
    }

    if (writefds == NULL)
	request->wfds = NULL;
    else {
	request->wfds = &request->writefds;
	FD_COPY(writefds, request->wfds);
    }

    if (exceptfds == NULL)
	request->efds = NULL;
    else {
	request->efds = &request->exceptfds;
	FD_COPY(exceptfds, request->efds);
    }
    
    request->nfds = nfds;

    if (timeout == NULL) {
	request -> timeout.TotalTime.tv_sec = -1;
	request -> timeout.TotalTime.tv_usec = -1;
    } else {
	request -> timeout.TotalTime = *timeout;
	/* check for bad request */
	if (timeout->tv_sec < 0 || 
	    timeout->tv_usec < 0 || 
	    timeout->tv_usec > 999999) {
	    /* invalid arg */
	    iomgr_badtv = *timeout;
	    iomgr_badpid = LWP_ActiveProcess;
	    /* now fixup request */
	    if(request->timeout.TotalTime.tv_sec < 0)
		request->timeout.TotalTime.tv_sec = 1;
	    request->timeout.TotalTime.tv_usec = 100000;
	}
    }

    request -> timeout.BackPointer = (char *) request;

    /* Insert my PID in case of IOMGR_Cancel */
    request -> pid = LWP_ActiveProcess;
    LWP_ActiveProcess -> iomgrRequest = request;

#ifdef DEBUG
    request -> timeout.Next = (struct TM_Elem *) 1;
    request -> timeout.Prev = (struct TM_Elem *) 1;
#endif /* DEBUG */
    TM_Insert(Requests, &request->timeout);

    /* Wait for action */
    LWP_QWait();

    /* Update parameters & return */
    if (readfds != NULL) FD_COPY(&request->readfds, readfds);
    if (writefds != NULL) FD_COPY(&request->writefds, writefds);
    if (exceptfds != NULL) FD_COPY(&request->exceptfds, exceptfds);
    result = request -> result;
    FreeRequest(request);
    return (result > 1 ? 1 : result);
}

int
IOMGR_Cancel(register PROCESS pid)
{
    register struct IoRequest *request;

    if ((request = pid->iomgrRequest) == 0) return -1;	/* Pid not found */

    FD_ZERO(&request->readfds);
    FD_ZERO(&request->writefds);
    FD_ZERO(&request->exceptfds);
    request->rfds = request->wfds = request->efds = NULL;
    request->nfds = 0;

    request -> result = -2;
    TM_Remove(Requests, &request->timeout);
#ifdef DEBUG
    request -> timeout.Next = (struct TM_Elem *) 5;
    request -> timeout.Prev = (struct TM_Elem *) 5;
#endif /* DEBUG */
    LWP_QSignal(request->pid);
    pid->iomgrRequest = 0;

    return 0;
}

/* 
 * Cause delivery of signal signo to result in a LWP_SignalProcess of
 * event. 
 */
int
IOMGR_Signal (int signo, char *event)
{
#ifdef AFS_POSIX_SIGNALS
    struct sigaction sa;
#else
    struct sigvec sv;
#endif

    if (badsig(signo))
	return LWP_EBADSIG;
    if (event == NULL)
	return LWP_EBADEVENT;
#ifdef AFS_POSIX_SIGNALS
    sa.sa_handler = SigHandler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags=0;    
#else
    sv.sv_handler = SigHandler;
    sv.sv_mask = ~0;	/* mask all signals */
    sv.sv_onstack = 0;
#endif
    sigsHandled |= mysigmask(signo);
    sigEvents[signo] = event;
    sigDelivered[signo] = FALSE;
#ifdef AFS_POSIX_SIGNALS
    if (sigaction (signo, &sa, &oldVecs[signo]) == -1)
        return LWP_ESYSTEM;
#else
    if (sigvec (signo, &sv, &oldVecs[signo]) == -1)
	return LWP_ESYSTEM;
#endif
    return LWP_SUCCESS;
}

/* Stop handling occurances of signo. */
int
IOMGR_CancelSignal (int signo)
{
    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
	return LWP_EBADSIG;
#ifdef AFS_POSIX_SIGNALS
    sigaction (signo, &oldVecs[signo], (struct sigaction *)0);
#else
    sigvec (signo, &oldVecs[signo], (struct sigvec *)0);
#endif
    sigsHandled &= ~mysigmask(signo);
    return LWP_SUCCESS;
}

/* 
 * This routine calls select is a fashion that simulates the standard 
 * sleep routine
 */
void 
IOMGR_Sleep (unsigned int seconds)
{   
    struct timeval timeout;

    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    IOMGR_Select(0, NULL, NULL, NULL, &timeout);
}
