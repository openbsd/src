/*	$OpenBSD: uthread_cancel.c,v 1.2 1999/11/25 07:01:32 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 */
#include <sys/errno.h>
#include <pthread.h>
#include "pthread_private.h"

int
pthread_cancel(pthread)
	pthread_t pthread;
{
	int ret;

	if ((ret = _find_thread(pthread))) {
	} else if ((pthread->flags & PTHREAD_FLAGS_CANCELED) == 0) {
		/* Set the thread's I've-been-cancelled flag: */
		pthread->flags |= PTHREAD_FLAGS_CANCELED;
		/* Check if we need to kick it back into the run queue: */
		if ((pthread->cancelstate == PTHREAD_CANCEL_ENABLE) &&
		    ((pthread->canceltype == PTHREAD_CANCEL_ASYNCHRONOUS) ||
		     (pthread->flags & PTHREAD_FLAGS_CANCELPT)))
			switch (pthread->state) {
			case PS_WAIT_WAIT:
			case PS_FDR_WAIT:
			case PS_FDW_WAIT:
			case PS_SLEEP_WAIT:
			case PS_SELECT_WAIT:
			case PS_POLL_WAIT:
			case PS_SIGSUSPEND:
				/* Interrupt and resume: */
				pthread->interrupted = 1;
				if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
					PTHREAD_WORKQ_REMOVE(pthread);
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				break;
			case PS_MUTEX_WAIT:
			case PS_COND_WAIT:
			case PS_SIGWAIT:
			case PS_FDLR_WAIT:
			case PS_FDLW_WAIT:
			case PS_FILE_WAIT:
			case PS_JOIN:
			case PS_SUSPENDED:
			case PS_SIGTHREAD:
				/* Simply wake: */
				/* XXX may be incorrect */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				break;
			case PS_RUNNING:
			case PS_DEADLOCK:
			case PS_SPINBLOCK:
			case PS_DEAD:
				/* Ignore */
				break;
		}
		ret = 0;
	}
	return (ret);
}

int
pthread_setcancelstate(state, oldstate)
	int state;
	int *oldstate;
{
	int ostate;
	int ret;

	ostate = _thread_run->cancelstate;

	switch (state) {
	case PTHREAD_CANCEL_ENABLE:
		if (oldstate)
			*oldstate = ostate;
		_thread_run->cancelstate = PTHREAD_CANCEL_ENABLE;
		if (_thread_run->canceltype == PTHREAD_CANCEL_ASYNCHRONOUS)
			_thread_cancellation_point();
		ret = 0;
		break;
	case PTHREAD_CANCEL_DISABLE:
		if (oldstate)
			*oldstate = ostate;
		_thread_run->cancelstate = PTHREAD_CANCEL_DISABLE;
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	return (ret);
}


int
pthread_setcanceltype(type, oldtype)
	int type;
	int *oldtype;
{
	int otype;
	int ret;

	otype = _thread_run->canceltype;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		if (oldtype)
			*oldtype = otype;
		_thread_run->canceltype = PTHREAD_CANCEL_ASYNCHRONOUS;
		_thread_cancellation_point();
		ret = 0;
		break;
	case PTHREAD_CANCEL_DEFERRED:
		if (oldtype)
			*oldtype = otype;
		_thread_run->canceltype = PTHREAD_CANCEL_DEFERRED;
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	return (ret);
}

void
pthread_testcancel()
{

	_thread_cancellation_point();
}

void
_thread_enter_cancellation_point()
{

	/* Look for a cancellation before we block: */
	_thread_cancellation_point();
	_thread_run->flags |= PTHREAD_FLAGS_CANCELPT;
}

void
_thread_leave_cancellation_point()
{

	_thread_run->flags &=~ PTHREAD_FLAGS_CANCELPT;
	/* Look for a cancellation after we unblock: */
	_thread_cancellation_point();
}

/*
 * Must only be called when in asynchronous cancel mode, or
 * from pthread_testcancel().
 */
void
_thread_cancellation_point()
{

	if ((_thread_run->cancelstate == PTHREAD_CANCEL_ENABLE) &&
	    ((_thread_run->flags & (PTHREAD_FLAGS_CANCELED|PTHREAD_EXITING)) ==
		PTHREAD_FLAGS_CANCELED)) {
		_thread_run->flags &=~ PTHREAD_FLAGS_CANCELED;
		pthread_exit(PTHREAD_CANCELED);
		PANIC("cancel");
	}
}
