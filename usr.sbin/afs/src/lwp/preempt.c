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
* 	Bradley White and M. Satyanarayanan			    *
\*******************************************************************/

#include <sys/time.h>
#include <signal.h>
#include <lwp.h>
#include "preempt.h"

RCSID("$arla: preempt.c,v 1.9 2002/06/02 11:59:54 lha Exp $");

char PRE_Block = 0;		/* used in lwp.c and process.s */

#ifdef HAVE_GETITIMER

static RETSIGTYPE
#if defined(AFS_POSIX_SIGNALS)
AlarmHandler(int sig)
#else
AlarmHandler(int sig, int code, struct sigcontext *scp)
#endif
{
#ifdef AFS_POSIX_SIGNALS
    sigset_t mask ;
#endif

    if (PRE_Block == 0 && lwp_cpptr->level == 0) {
	PRE_BeginCritical();
#if defined(AFS_POSIX_SIGNALS)
        sigemptyset(&mask);
        sigaddset(&mask, sig);
        sigprocmask(SIG_UNBLOCK, &mask, (sigset_t *)0);
#else
	sigsetmask(scp->sc_mask);
#endif
	LWP_DispatchProcess();
	PRE_EndCritical();
    }
    
}



int 
PRE_InitPreempt(struct timeval *slice)
{
    struct itimerval itv;
#ifdef AFS_POSIX_SIGNALS
    struct sigaction sa;
#else
    struct sigvec vec;
#endif

    if (lwp_cpptr == 0) return (LWP_EINIT);
    
    if (slice == 0) {
	itv.it_interval.tv_sec = itv.it_value.tv_sec = DEFAULTSLICE;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = 0;
    } else {
	itv.it_interval = itv.it_value = *slice;
    }
    
#ifdef AFS_POSIX_SIGNALS
    sa.sa_handler = AlarmHandler;
#ifndef SA_NODEFER
#define SA_NODEFER 0
#endif
#ifdef SA_SIGINFO
    sa.sa_flags = SA_SIGINFO|SA_NODEFER;
#else
    sa.sa_flags = SA_NODEFER;
#endif

    if ((sigaction(SIGALRM, &sa, (struct sigaction *)0) == -1) ||
        (setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1))
        return(LWP_ESYSTEM);
#else
    vec.sv_handler = AlarmHandler;
    vec.sv_mask = vec.sv_onstack = 0;

    if ((sigvec(SIGALRM, &vec, (struct sigvec *)0) == -1) ||
	(setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1))
	return(LWP_ESYSTEM);
#endif

    return(LWP_SUCCESS);
}

int 
PRE_EndPreempt(void)
{
    struct itimerval itv;
#ifdef AFS_POSIX_SIGNALS
    struct sigaction sa;
#else
    struct sigvec vec;
#endif

    if (lwp_cpptr == 0) 
	return (LWP_EINIT);
    
    itv.it_value.tv_sec = itv.it_value.tv_usec = 0;

#ifdef AFS_POSIX_SIGNALS
    sa.sa_handler = SIG_DFL;
    sa.sa_flags=0;

    if ((setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1) ||
        (sigaction(SIGALRM, &sa, (struct sigaction *)0) == -1))
        return(LWP_ESYSTEM);
#else
    vec.sv_handler = SIG_DFL;
    vec.sv_mask = vec.sv_onstack = 0;

    if ((setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1) ||
	(sigvec(SIGALRM, &vec, (struct sigvec *)0) == -1))
	return(LWP_ESYSTEM);
#endif

    return(LWP_SUCCESS);
}

#else /* !HAVE_GETITIMER */

int 
PRE_InitPreempt(struct timeval *slice)
{
    return LWP_SUCCESS;
}

int 
PRE_EndPreempt(void)
{
    return LWP_SUCCESS;
}

#endif /* HAVE_GETITIMER */
