/*	$OpenBSD: kern_event.c,v 1.60 2014/12/09 07:05:06 doug Exp $	*/

/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon <jlemon@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/kern/kern_event.c,v 1.22 2001/02/23 20:32:42 jlemon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/selinfo.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/ktrace.h>
#include <sys/pool.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/syscallargs.h>
#include <sys/timeout.h>

int	kqueue_scan(struct kqueue *kq, int maxevents,
		    struct kevent *ulistp, const struct timespec *timeout,
		    struct proc *p, int *retval);

int	kqueue_read(struct file *fp, off_t *poff, struct uio *uio,
		    struct ucred *cred);
int	kqueue_write(struct file *fp, off_t *poff, struct uio *uio,
		    struct ucred *cred);
int	kqueue_ioctl(struct file *fp, u_long com, caddr_t data,
		    struct proc *p);
int	kqueue_poll(struct file *fp, int events, struct proc *p);
int	kqueue_kqfilter(struct file *fp, struct knote *kn);
int	kqueue_stat(struct file *fp, struct stat *st, struct proc *p);
int	kqueue_close(struct file *fp, struct proc *p);
void	kqueue_wakeup(struct kqueue *kq);

struct fileops kqueueops = {
	kqueue_read,
	kqueue_write,
	kqueue_ioctl,
	kqueue_poll,
	kqueue_kqfilter,
	kqueue_stat,
	kqueue_close
};

void	knote_attach(struct knote *kn, struct filedesc *fdp);
void	knote_drop(struct knote *kn, struct proc *p, struct filedesc *fdp);
void	knote_enqueue(struct knote *kn);
void	knote_dequeue(struct knote *kn);
#define knote_alloc() ((struct knote *)pool_get(&knote_pool, PR_WAITOK))
#define knote_free(kn) pool_put(&knote_pool, (kn))

void	filt_kqdetach(struct knote *kn);
int	filt_kqueue(struct knote *kn, long hint);
int	filt_procattach(struct knote *kn);
void	filt_procdetach(struct knote *kn);
int	filt_proc(struct knote *kn, long hint);
int	filt_fileattach(struct knote *kn);
void	filt_timerexpire(void *knx);
int	filt_timerattach(struct knote *kn);
void	filt_timerdetach(struct knote *kn);
int	filt_timer(struct knote *kn, long hint);
void	filt_seltruedetach(struct knote *kn);

struct filterops kqread_filtops =
	{ 1, NULL, filt_kqdetach, filt_kqueue };
struct filterops proc_filtops =
	{ 0, filt_procattach, filt_procdetach, filt_proc };
struct filterops file_filtops =
	{ 1, filt_fileattach, NULL, NULL };
struct filterops timer_filtops =
        { 0, filt_timerattach, filt_timerdetach, filt_timer };

struct	pool knote_pool;
struct	pool kqueue_pool;
int kq_ntimeouts = 0;
int kq_timeoutmax = (4 * 1024);

#define KNOTE_ACTIVATE(kn) do {						\
	kn->kn_status |= KN_ACTIVE;					\
	if ((kn->kn_status & (KN_QUEUED | KN_DISABLED)) == 0)		\
		knote_enqueue(kn);					\
} while(0)

#define	KN_HASHSIZE		64		/* XXX should be tunable */
#define KN_HASH(val, mask)	(((val) ^ (val >> 8)) & (mask))

extern struct filterops sig_filtops;
#ifdef notyet
extern struct filterops aio_filtops;
#endif

/*
 * Table for for all system-defined filters.
 */
struct filterops *sysfilt_ops[] = {
	&file_filtops,			/* EVFILT_READ */
	&file_filtops,			/* EVFILT_WRITE */
	NULL, /*&aio_filtops,*/		/* EVFILT_AIO */
	&file_filtops,			/* EVFILT_VNODE */
	&proc_filtops,			/* EVFILT_PROC */
	&sig_filtops,			/* EVFILT_SIGNAL */
	&timer_filtops,			/* EVFILT_TIMER */
};

void KQREF(struct kqueue *);
void KQRELE(struct kqueue *);

void
KQREF(struct kqueue *kq)
{
	++kq->kq_refs;
}

void
KQRELE(struct kqueue *kq)
{
	if (--kq->kq_refs == 0) {
		pool_put(&kqueue_pool, kq);
	}
}

void kqueue_init(void);

void
kqueue_init(void)
{

	pool_init(&kqueue_pool, sizeof(struct kqueue), 0, 0, 0, "kqueuepl",
	    &pool_allocator_nointr);
	pool_init(&knote_pool, sizeof(struct knote), 0, 0, 0, "knotepl",
	    &pool_allocator_nointr);
}

int
filt_fileattach(struct knote *kn)
{
	struct file *fp = kn->kn_fp;

	return ((*fp->f_ops->fo_kqfilter)(fp, kn));
}

int
kqueue_kqfilter(struct file *fp, struct knote *kn)
{
	struct kqueue *kq = (struct kqueue *)kn->kn_fp->f_data;

	if (kn->kn_filter != EVFILT_READ)
		return (EINVAL);

	kn->kn_fop = &kqread_filtops;
	SLIST_INSERT_HEAD(&kq->kq_sel.si_note, kn, kn_selnext);
	return (0);
}

void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq = (struct kqueue *)kn->kn_fp->f_data;

	SLIST_REMOVE(&kq->kq_sel.si_note, kn, knote, kn_selnext);
}

/*ARGSUSED*/
int
filt_kqueue(struct knote *kn, long hint)
{
	struct kqueue *kq = (struct kqueue *)kn->kn_fp->f_data;

	kn->kn_data = kq->kq_count;
	return (kn->kn_data > 0);
}

int
filt_procattach(struct knote *kn)
{
	struct process *pr;

	pr = prfind(kn->kn_id);
	if (pr == NULL)
		return (ESRCH);

	/* exiting processes can't be specified */
	if (pr->ps_flags & PS_EXITING)
		return (ESRCH);

	/*
	 * Fail if it's not owned by you, or the last exec gave us
	 * setuid/setgid privs (unless you're root).
	 */
	if (pr != curproc->p_p &&
	    (pr->ps_ucred->cr_ruid != curproc->p_ucred->cr_ruid ||
	    (pr->ps_flags & PS_SUGID)) && suser(curproc, 0) != 0)
		return (EACCES);

	kn->kn_ptr.p_process = pr;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	/*
	 * internal flag indicating registration done by kernel
	 */
	if (kn->kn_flags & EV_FLAG1) {
		kn->kn_data = kn->kn_sdata;		/* ppid */
		kn->kn_fflags = NOTE_CHILD;
		kn->kn_flags &= ~EV_FLAG1;
	}

	/* XXX lock the proc here while adding to the list? */
	SLIST_INSERT_HEAD(&pr->ps_klist, kn, kn_selnext);

	return (0);
}

/*
 * The knote may be attached to a different process, which may exit,
 * leaving nothing for the knote to be attached to.  So when the process
 * exits, the knote is marked as DETACHED and also flagged as ONESHOT so
 * it will be deleted when read out.  However, as part of the knote deletion,
 * this routine is called, so a check is needed to avoid actually performing
 * a detach, because the original process does not exist any more.
 */
void
filt_procdetach(struct knote *kn)
{
	struct process *pr = kn->kn_ptr.p_process;

	if (kn->kn_status & KN_DETACHED)
		return;

	/* XXX locking?  this might modify another process. */
	SLIST_REMOVE(&pr->ps_klist, kn, knote, kn_selnext);
}

int
filt_proc(struct knote *kn, long hint)
{
	u_int event;

	/*
	 * mask off extra data
	 */
	event = (u_int)hint & NOTE_PCTRLMASK;

	/*
	 * if the user is interested in this event, record it.
	 */
	if (kn->kn_sfflags & event)
		kn->kn_fflags |= event;

	/*
	 * process is gone, so flag the event as finished and remove it
	 * from the process's klist
	 */
	if (event == NOTE_EXIT) {
		struct process *pr = kn->kn_ptr.p_process;

		kn->kn_status |= KN_DETACHED;
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		kn->kn_data = pr->ps_mainproc->p_xstat;
		SLIST_REMOVE(&pr->ps_klist, kn, knote, kn_selnext);
		return (1);
	}

	/*
	 * process forked, and user wants to track the new process,
	 * so attach a new knote to it, and immediately report an
	 * event with the parent's pid.
	 */
	if ((event == NOTE_FORK) && (kn->kn_sfflags & NOTE_TRACK)) {
		struct kevent kev;
		int error;

		/*
		 * register knote with new process.
		 */
		kev.ident = hint & NOTE_PDATAMASK;	/* pid */
		kev.filter = kn->kn_filter;
		kev.flags = kn->kn_flags | EV_ADD | EV_ENABLE | EV_FLAG1;
		kev.fflags = kn->kn_sfflags;
		kev.data = kn->kn_id;			/* parent */
		kev.udata = kn->kn_kevent.udata;	/* preserve udata */
		error = kqueue_register(kn->kn_kq, &kev, NULL);
		if (error)
			kn->kn_fflags |= NOTE_TRACKERR;
	}

	return (kn->kn_fflags != 0);
}

void
filt_timerexpire(void *knx)
{
	struct knote *kn = knx;
	struct timeval tv;
	int tticks;

	kn->kn_data++;
	KNOTE_ACTIVATE(kn);

	if ((kn->kn_flags & EV_ONESHOT) == 0) {
		tv.tv_sec = kn->kn_sdata / 1000;
		tv.tv_usec = (kn->kn_sdata % 1000) * 1000;
		tticks = tvtohz(&tv);
		timeout_add((struct timeout *)kn->kn_hook, tticks);
	}
}


/*
 * data contains amount of time to sleep, in milliseconds
 */
int
filt_timerattach(struct knote *kn)
{
	struct timeout *to;
	struct timeval tv;
	int tticks;

	if (kq_ntimeouts > kq_timeoutmax)
		return (ENOMEM);
	kq_ntimeouts++;

	tv.tv_sec = kn->kn_sdata / 1000;
	tv.tv_usec = (kn->kn_sdata % 1000) * 1000;
	tticks = tvtohz(&tv);

	kn->kn_flags |= EV_CLEAR;	/* automatically set */
	to = malloc(sizeof(*to), M_KEVENT, M_WAITOK);
	timeout_set(to, filt_timerexpire, kn);
	timeout_add(to, tticks);
	kn->kn_hook = to;

	return (0);
}

void
filt_timerdetach(struct knote *kn)
{
	struct timeout *to;

	to = (struct timeout *)kn->kn_hook;
	timeout_del(to);
	free(to, M_KEVENT, sizeof(*to));
	kq_ntimeouts--;
}

int
filt_timer(struct knote *kn, long hint)
{
	return (kn->kn_data != 0);
}


/*
 * filt_seltrue:
 *
 *	This filter "event" routine simulates seltrue().
 */
int
filt_seltrue(struct knote *kn, long hint)
{

	/*
	 * We don't know how much data can be read/written,
	 * but we know that it *can* be.  This is about as
	 * good as select/poll does as well.
	 */
	kn->kn_data = 0;
	return (1);
}

/*
 * This provides full kqfilter entry for device switch tables, which
 * has same effect as filter using filt_seltrue() as filter method.
 */
void
filt_seltruedetach(struct knote *kn)
{
	/* Nothing to do */
}

const struct filterops seltrue_filtops =
	{ 1, NULL, filt_seltruedetach, filt_seltrue };

int
seltrue_kqfilter(dev_t dev, struct knote *kn)
{
	switch (kn->kn_filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
		kn->kn_fop = &seltrue_filtops;
		break;
	default:
		return (EINVAL);
	}

	/* Nothing more to do */
	return (0);
}

int
sys_kqueue(struct proc *p, void *v, register_t *retval)
{
	struct filedesc *fdp = p->p_fd;
	struct kqueue *kq;
	struct file *fp;
	int fd, error;

	fdplock(fdp);
	error = falloc(p, &fp, &fd);
	fdpunlock(fdp);
	if (error)
		return (error);
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	kq = pool_get(&kqueue_pool, PR_WAITOK|PR_ZERO);
	TAILQ_INIT(&kq->kq_head);
	fp->f_data = kq;
	KQREF(kq);
	*retval = fd;
	if (fdp->fd_knlistsize < 0)
		fdp->fd_knlistsize = 0;		/* this process has a kq */
	kq->kq_fdp = fdp;
	FILE_SET_MATURE(fp, p);
	return (0);
}

int
sys_kevent(struct proc *p, void *v, register_t *retval)
{
	struct filedesc* fdp = p->p_fd;
	struct sys_kevent_args /* {
		syscallarg(int)	fd;
		syscallarg(const struct kevent *) changelist;
		syscallarg(int)	nchanges;
		syscallarg(struct kevent *) eventlist;
		syscallarg(int)	nevents;
		syscallarg(const struct timespec *) timeout;
	} */ *uap = v;
	struct kevent *kevp;
	struct kqueue *kq;
	struct file *fp;
	struct timespec ts;
	int i, n, nerrors, error;

	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL ||
	    (fp->f_type != DTYPE_KQUEUE))
		return (EBADF);

	FREF(fp);

	if (SCARG(uap, timeout) != NULL) {
		error = copyin(SCARG(uap, timeout), &ts, sizeof(ts));
		if (error)
			goto done;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
		SCARG(uap, timeout) = &ts;
	}

	kq = (struct kqueue *)fp->f_data;
	nerrors = 0;

	while (SCARG(uap, nchanges) > 0) {
		n = SCARG(uap, nchanges) > KQ_NEVENTS
			? KQ_NEVENTS : SCARG(uap, nchanges);
		error = copyin(SCARG(uap, changelist), kq->kq_kev,
		    n * sizeof(struct kevent));
		if (error)
			goto done;
		for (i = 0; i < n; i++) {
			kevp = &kq->kq_kev[i];
			kevp->flags &= ~EV_SYSFLAGS;
			error = kqueue_register(kq, kevp, p);
			if (error) {
				if (SCARG(uap, nevents) != 0) {
					kevp->flags = EV_ERROR;
					kevp->data = error;
					copyout(kevp, SCARG(uap, eventlist),
					    sizeof(*kevp));
					SCARG(uap, eventlist)++;
					SCARG(uap, nevents)--;
					nerrors++;
				} else {
					goto done;
				}
			}
		}
		SCARG(uap, nchanges) -= n;
		SCARG(uap, changelist) += n;
	}
	if (nerrors) {
		*retval = nerrors;
		error = 0;
		goto done;
	}

	KQREF(kq);
	FRELE(fp, p);
	error = kqueue_scan(kq, SCARG(uap, nevents), SCARG(uap, eventlist),
			    SCARG(uap, timeout), p, &n);
	KQRELE(kq);
	*retval = n;
	return (error);

 done:
	FRELE(fp, p);
	return (error);
}

int
kqueue_register(struct kqueue *kq, struct kevent *kev, struct proc *p)
{
	struct filedesc *fdp = kq->kq_fdp;
	struct filterops *fops = NULL;
	struct file *fp = NULL;
	struct knote *kn = NULL;
	int s, error = 0;

	if (kev->filter < 0) {
		if (kev->filter + EVFILT_SYSCOUNT < 0)
			return (EINVAL);
		fops = sysfilt_ops[~kev->filter];	/* to 0-base index */
	}

	if (fops == NULL) {
		/*
		 * XXX
		 * filter attach routine is responsible for ensuring that
		 * the identifier can be attached to it.
		 */
		return (EINVAL);
	}

	if (fops->f_isfd) {
		/* validate descriptor */
		if ((fp = fd_getfile(fdp, kev->ident)) == NULL)
			return (EBADF);
		FREF(fp);

		if (kev->ident < fdp->fd_knlistsize) {
			SLIST_FOREACH(kn, &fdp->fd_knlist[kev->ident], kn_link)
				if (kq == kn->kn_kq &&
				    kev->filter == kn->kn_filter)
					break;
		}
	} else {
		if (fdp->fd_knhashmask != 0) {
			struct klist *list;

			list = &fdp->fd_knhash[
			    KN_HASH((u_long)kev->ident, fdp->fd_knhashmask)];
			SLIST_FOREACH(kn, list, kn_link)
				if (kev->ident == kn->kn_id &&
				    kq == kn->kn_kq &&
				    kev->filter == kn->kn_filter)
					break;
		}
	}

	if (kn == NULL && ((kev->flags & EV_ADD) == 0)) {
		error = ENOENT;
		goto done;
	}

	/*
	 * kn now contains the matching knote, or NULL if no match
	 */
	if (kev->flags & EV_ADD) {

		if (kn == NULL) {
			kn = knote_alloc();
			if (kn == NULL) {
				error = ENOMEM;
				goto done;
			}
			kn->kn_fp = fp;
			kn->kn_kq = kq;
			kn->kn_fop = fops;

			/*
			 * apply reference count to knote structure, and
			 * do not release it at the end of this routine.
			 */
			fp = NULL;

			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kev->fflags = 0;
			kev->data = 0;
			kn->kn_kevent = *kev;

			knote_attach(kn, fdp);
			if ((error = fops->f_attach(kn)) != 0) {
				knote_drop(kn, p, fdp);
				goto done;
			}
		} else {
			/*
			 * The user may change some filter values after the
			 * initial EV_ADD, but doing so will not reset any
			 * filters which have already been triggered.
			 */
			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kn->kn_kevent.udata = kev->udata;
		}

		s = splhigh();
		if (kn->kn_fop->f_event(kn, 0))
			KNOTE_ACTIVATE(kn);
		splx(s);

	} else if (kev->flags & EV_DELETE) {
		kn->kn_fop->f_detach(kn);
		knote_drop(kn, p, p->p_fd);
		goto done;
	}

	if ((kev->flags & EV_DISABLE) &&
	    ((kn->kn_status & KN_DISABLED) == 0)) {
		s = splhigh();
		kn->kn_status |= KN_DISABLED;
		splx(s);
	}

	if ((kev->flags & EV_ENABLE) && (kn->kn_status & KN_DISABLED)) {
		s = splhigh();
		kn->kn_status &= ~KN_DISABLED;
		if ((kn->kn_status & KN_ACTIVE) &&
		    ((kn->kn_status & KN_QUEUED) == 0))
			knote_enqueue(kn);
		splx(s);
	}

done:
	if (fp != NULL)
		FRELE(fp, p);
	return (error);
}

int
kqueue_scan(struct kqueue *kq, int maxevents, struct kevent *ulistp,
	const struct timespec *tsp, struct proc *p, int *retval)
{
	struct kevent *kevp;
	struct timeval atv, rtv, ttv;
	struct knote *kn, marker;
	int s, count, timeout, nkev = 0, error = 0;

	count = maxevents;
	if (count == 0)
		goto done;

	if (tsp != NULL) {
		TIMESPEC_TO_TIMEVAL(&atv, tsp);
		if (tsp->tv_sec == 0 && tsp->tv_nsec == 0) {
			/* No timeout, just poll */
			timeout = -1;
			goto start;
		}
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}

		timeout = atv.tv_sec > 24 * 60 * 60 ?
			24 * 60 * 60 * hz : tvtohz(&atv);

		getmicrouptime(&rtv);
		timeradd(&atv, &rtv, &atv);
	} else {
		atv.tv_sec = 0;
		atv.tv_usec = 0;
		timeout = 0;
	}
	goto start;

retry:
	if (atv.tv_sec || atv.tv_usec) {
		getmicrouptime(&rtv);
		if (timercmp(&rtv, &atv, >=))
			goto done;
		ttv = atv;
		timersub(&ttv, &rtv, &ttv);
		timeout = ttv.tv_sec > 24 * 60 * 60 ?
			24 * 60 * 60 * hz : tvtohz(&ttv);
	}

start:
	if (kq->kq_state & KQ_DYING) {
		error = EBADF;
		goto done;
	}

	kevp = kq->kq_kev;
	s = splhigh();
	if (kq->kq_count == 0) {
		if (timeout < 0) {
			error = EWOULDBLOCK;
		} else {
			kq->kq_state |= KQ_SLEEP;
			error = tsleep(kq, PSOCK | PCATCH, "kqread", timeout);
		}
		splx(s);
		if (error == 0)
			goto retry;
		/* don't restart after signals... */
		if (error == ERESTART)
			error = EINTR;
		else if (error == EWOULDBLOCK)
			error = 0;
		goto done;
	}

	TAILQ_INSERT_TAIL(&kq->kq_head, &marker, kn_tqe);
	while (count) {
		kn = TAILQ_FIRST(&kq->kq_head);
		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
		if (kn == &marker) {
			splx(s);
			if (count == maxevents)
				goto retry;
			goto done;
		}
		if (kn->kn_status & KN_DISABLED) {
			kn->kn_status &= ~KN_QUEUED;
			kq->kq_count--;
			continue;
		}
		if ((kn->kn_flags & EV_ONESHOT) == 0 &&
		    kn->kn_fop->f_event(kn, 0) == 0) {
			kn->kn_status &= ~(KN_QUEUED | KN_ACTIVE);
			kq->kq_count--;
			continue;
		}
		*kevp = kn->kn_kevent;
		kevp++;
		nkev++;
		if (kn->kn_flags & EV_ONESHOT) {
			kn->kn_status &= ~KN_QUEUED;
			kq->kq_count--;
			splx(s);
			kn->kn_fop->f_detach(kn);
			knote_drop(kn, p, p->p_fd);
			s = splhigh();
		} else if (kn->kn_flags & EV_CLEAR) {
			kn->kn_data = 0;
			kn->kn_fflags = 0;
			kn->kn_status &= ~(KN_QUEUED | KN_ACTIVE);
			kq->kq_count--;
		} else {
			TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
		}
		count--;
		if (nkev == KQ_NEVENTS) {
			splx(s);
			error = copyout(&kq->kq_kev, ulistp,
			    sizeof(struct kevent) * nkev);
			ulistp += nkev;
			nkev = 0;
			kevp = kq->kq_kev;
			s = splhigh();
			if (error)
				break;
		}
	}
	TAILQ_REMOVE(&kq->kq_head, &marker, kn_tqe);
	splx(s);
done:
	if (nkev != 0)
		error = copyout(&kq->kq_kev, ulistp,
		    sizeof(struct kevent) * nkev);
	*retval = maxevents - count;
	return (error);
}

/*
 * XXX
 * This could be expanded to call kqueue_scan, if desired.
 */
/*ARGSUSED*/
int
kqueue_read(struct file *fp, off_t *poff, struct uio *uio, struct ucred *cred)
{
	return (ENXIO);
}

/*ARGSUSED*/
int
kqueue_write(struct file *fp, off_t *poff, struct uio *uio, struct ucred *cred)

{
	return (ENXIO);
}

/*ARGSUSED*/
int
kqueue_ioctl(struct file *fp, u_long com, caddr_t data, struct proc *p)
{
	return (ENOTTY);
}

/*ARGSUSED*/
int
kqueue_poll(struct file *fp, int events, struct proc *p)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	int revents = 0;
	int s = splhigh();

	if (events & (POLLIN | POLLRDNORM)) {
		if (kq->kq_count) {
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			selrecord(p, &kq->kq_sel);
			kq->kq_state |= KQ_SEL;
		}
	}
	splx(s);
	return (revents);
}

/*ARGSUSED*/
int
kqueue_stat(struct file *fp, struct stat *st, struct proc *p)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;

	memset(st, 0, sizeof(*st));
	st->st_size = kq->kq_count;
	st->st_blksize = sizeof(struct kevent);
	st->st_mode = S_IFIFO;
	return (0);
}

/*ARGSUSED*/
int
kqueue_close(struct file *fp, struct proc *p)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	struct filedesc *fdp = p->p_fd;
	struct knote **knp, *kn, *kn0;
	int i;

	for (i = 0; i < fdp->fd_knlistsize; i++) {
		knp = &SLIST_FIRST(&fdp->fd_knlist[i]);
		kn = *knp;
		while (kn != NULL) {
			kn0 = SLIST_NEXT(kn, kn_link);
			if (kq == kn->kn_kq) {
				kn->kn_fop->f_detach(kn);
				FRELE(kn->kn_fp, p);
				knote_free(kn);
				*knp = kn0;
			} else {
				knp = &SLIST_NEXT(kn, kn_link);
			}
			kn = kn0;
		}
	}
	if (fdp->fd_knhashmask != 0) {
		for (i = 0; i < fdp->fd_knhashmask + 1; i++) {
			knp = &SLIST_FIRST(&fdp->fd_knhash[i]);
			kn = *knp;
			while (kn != NULL) {
				kn0 = SLIST_NEXT(kn, kn_link);
				if (kq == kn->kn_kq) {
					kn->kn_fop->f_detach(kn);
		/* XXX non-fd release of kn->kn_ptr */
					knote_free(kn);
					*knp = kn0;
				} else {
					knp = &SLIST_NEXT(kn, kn_link);
				}
				kn = kn0;
			}
		}
	}
	fp->f_data = NULL;

	kq->kq_state |= KQ_DYING;
	kqueue_wakeup(kq);
	KQRELE(kq);

	return (0);
}

void
kqueue_wakeup(struct kqueue *kq)
{

	if (kq->kq_state & KQ_SLEEP) {
		kq->kq_state &= ~KQ_SLEEP;
		wakeup(kq);
	}
	if (kq->kq_state & KQ_SEL) {
		kq->kq_state &= ~KQ_SEL;
		selwakeup(&kq->kq_sel);
	} else
		KNOTE(&kq->kq_sel.si_note, 0);
}

/*
 * activate one knote.
 */
void
knote_activate(struct knote *kn)
{
	KNOTE_ACTIVATE(kn);
}

/*
 * walk down a list of knotes, activating them if their event has triggered.
 */
void
knote(struct klist *list, long hint)
{
	struct knote *kn;

	SLIST_FOREACH(kn, list, kn_selnext)
		if (kn->kn_fop->f_event(kn, hint))
			KNOTE_ACTIVATE(kn);
}

/*
 * remove all knotes from a specified klist
 */
void
knote_remove(struct proc *p, struct klist *list)
{
	struct knote *kn;

	while ((kn = SLIST_FIRST(list)) != NULL) {
		kn->kn_fop->f_detach(kn);
		knote_drop(kn, p, p->p_fd);
	}
}

/*
 * remove all knotes referencing a specified fd
 */
void
knote_fdclose(struct proc *p, int fd)
{
	struct filedesc *fdp = p->p_fd;
	struct klist *list = &fdp->fd_knlist[fd];

	knote_remove(p, list);
}

/*
 * handle a process exiting, including the triggering of NOTE_EXIT notes
 * XXX this could be more efficient, doing a single pass down the klist
 */
void
knote_processexit(struct proc *p)
{
	struct process *pr = p->p_p;

	KNOTE(&pr->ps_klist, NOTE_EXIT);

	/* remove other knotes hanging off the process */
	knote_remove(p, &pr->ps_klist);
}

void
knote_attach(struct knote *kn, struct filedesc *fdp)
{
	struct klist *list;
	int size;

	if (! kn->kn_fop->f_isfd) {
		if (fdp->fd_knhashmask == 0)
			fdp->fd_knhash = hashinit(KN_HASHSIZE, M_TEMP,
			    M_WAITOK, &fdp->fd_knhashmask);
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];
		goto done;
	}

	if (fdp->fd_knlistsize <= kn->kn_id) {
		size = fdp->fd_knlistsize;
		while (size <= kn->kn_id)
			size += KQEXTENT;
		list = mallocarray(size, sizeof(struct klist), M_TEMP,
		    M_WAITOK);
		memcpy(list, fdp->fd_knlist,
		    fdp->fd_knlistsize * sizeof(struct klist));
		memset(&list[fdp->fd_knlistsize], 0,
		    (size - fdp->fd_knlistsize) * sizeof(struct klist));
		if (fdp->fd_knlist != NULL)
			free(fdp->fd_knlist, M_TEMP, 0);
		fdp->fd_knlistsize = size;
		fdp->fd_knlist = list;
	}
	list = &fdp->fd_knlist[kn->kn_id];
done:
	SLIST_INSERT_HEAD(list, kn, kn_link);
	kn->kn_status = 0;
}

/*
 * should be called at spl == 0, since we don't want to hold spl
 * while calling FRELE and knote_free.
 */
void
knote_drop(struct knote *kn, struct proc *p, struct filedesc *fdp)
{
	struct klist *list;

	if (kn->kn_fop->f_isfd)
		list = &fdp->fd_knlist[kn->kn_id];
	else
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];

	SLIST_REMOVE(list, kn, knote, kn_link);
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	if (kn->kn_fop->f_isfd) {
		FRELE(kn->kn_fp, p);
	}
	knote_free(kn);
}


void
knote_enqueue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	int s = splhigh();

	KASSERT((kn->kn_status & KN_QUEUED) == 0);

	TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
	kn->kn_status |= KN_QUEUED;
	kq->kq_count++;
	splx(s);
	kqueue_wakeup(kq);
}

void
knote_dequeue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	int s = splhigh();

	KASSERT(kn->kn_status & KN_QUEUED);

	TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
	kn->kn_status &= ~KN_QUEUED;
	kq->kq_count--;
	splx(s);
}

void
klist_invalidate(struct klist *list)
{
	struct knote *kn;

	SLIST_FOREACH(kn, list, kn_selnext) {
		kn->kn_status |= KN_DETACHED;
		kn->kn_flags |= EV_EOF | EV_ONESHOT;
	}
}
