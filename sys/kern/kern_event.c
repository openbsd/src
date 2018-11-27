/*	$OpenBSD: kern_event.c,v 1.101 2018/11/27 15:52:50 cheloha Exp $	*/

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
#include <sys/pledge.h>
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

int	kqueue_read(struct file *, struct uio *, int);
int	kqueue_write(struct file *, struct uio *, int);
int	kqueue_ioctl(struct file *fp, u_long com, caddr_t data,
		    struct proc *p);
int	kqueue_poll(struct file *fp, int events, struct proc *p);
int	kqueue_kqfilter(struct file *fp, struct knote *kn);
int	kqueue_stat(struct file *fp, struct stat *st, struct proc *p);
int	kqueue_close(struct file *fp, struct proc *p);
void	kqueue_wakeup(struct kqueue *kq);

struct fileops kqueueops = {
	.fo_read	= kqueue_read,
	.fo_write	= kqueue_write,
	.fo_ioctl	= kqueue_ioctl,
	.fo_poll	= kqueue_poll,
	.fo_kqfilter	= kqueue_kqfilter,
	.fo_stat	= kqueue_stat,
	.fo_close	= kqueue_close
};

void	knote_attach(struct knote *kn);
void	knote_drop(struct knote *kn, struct proc *p);
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
	&file_filtops,			/* EVFILT_DEVICE */
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
	if (--kq->kq_refs > 0)
		return;

	LIST_REMOVE(kq, kq_next);
	free(kq->kq_knlist, M_TEMP, kq->kq_knlistsize * sizeof(struct klist));
	hashfree(kq->kq_knhash, KN_HASHSIZE, M_TEMP);
	pool_put(&kqueue_pool, kq);
}

void kqueue_init(void);

void
kqueue_init(void)
{

	pool_init(&kqueue_pool, sizeof(struct kqueue), 0, IPL_NONE, PR_WAITOK,
	    "kqueuepl", NULL);
	pool_init(&knote_pool, sizeof(struct knote), 0, IPL_NONE, PR_WAITOK,
	    "knotepl", NULL);
}

int
filt_fileattach(struct knote *kn)
{
	struct file *fp = kn->kn_fp;

	return fp->f_ops->fo_kqfilter(fp, kn);
}

int
kqueue_kqfilter(struct file *fp, struct knote *kn)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	if (kn->kn_filter != EVFILT_READ)
		return (EINVAL);

	kn->kn_fop = &kqread_filtops;
	SLIST_INSERT_HEAD(&kq->kq_sel.si_note, kn, kn_selnext);
	return (0);
}

void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	SLIST_REMOVE(&kq->kq_sel.si_note, kn, knote, kn_selnext);
}

int
filt_kqueue(struct knote *kn, long hint)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	kn->kn_data = kq->kq_count;
	return (kn->kn_data > 0);
}

int
filt_procattach(struct knote *kn)
{
	struct process *pr;

	if ((curproc->p_p->ps_flags & PS_PLEDGE) &&
	    (curproc->p_p->ps_pledge & PLEDGE_PROC) == 0)
		return pledge_fail(curproc, EPERM, PLEDGE_PROC);

	if (kn->kn_id > PID_MAX)
		return ESRCH;

	pr = prfind(kn->kn_id);
	if (pr == NULL)
		return (ESRCH);

	/* exiting processes can't be specified */
	if (pr->ps_flags & PS_EXITING)
		return (ESRCH);

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
		memset(&kev, 0, sizeof(kev));
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

static void
filt_timer_timeout_add(struct knote *kn)
{
	struct timeval tv;
	struct timeout *to = kn->kn_hook;
	int tticks;

	tv.tv_sec = kn->kn_sdata / 1000;
	tv.tv_usec = (kn->kn_sdata % 1000) * 1000;
	tticks = tvtohz(&tv);
	/* Remove extra tick from tvtohz() if timeout has fired before. */
	if (timeout_triggered(to))
		tticks--;
	timeout_add(to, (tticks > 0) ? tticks : 1);
}

void
filt_timerexpire(void *knx)
{
	struct knote *kn = knx;

	kn->kn_data++;
	KNOTE_ACTIVATE(kn);

	if ((kn->kn_flags & EV_ONESHOT) == 0)
		filt_timer_timeout_add(kn);
}


/*
 * data contains amount of time to sleep, in milliseconds
 */
int
filt_timerattach(struct knote *kn)
{
	struct timeout *to;

	if (kq_ntimeouts > kq_timeoutmax)
		return (ENOMEM);
	kq_ntimeouts++;

	kn->kn_flags |= EV_CLEAR;	/* automatically set */
	to = malloc(sizeof(*to), M_KEVENT, M_WAITOK);
	timeout_set(to, filt_timerexpire, kn);
	kn->kn_hook = to;
	filt_timer_timeout_add(kn);

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
	if (error)
		goto out;
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	kq = pool_get(&kqueue_pool, PR_WAITOK|PR_ZERO);
	TAILQ_INIT(&kq->kq_head);
	fp->f_data = kq;
	KQREF(kq);
	*retval = fd;
	kq->kq_fdp = fdp;
	LIST_INSERT_HEAD(&p->p_p->ps_kqlist, kq, kq_next);
	fdinsert(fdp, fd, 0, fp);
	FRELE(fp, p);
out:
	fdpunlock(fdp);
	return (error);
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
	struct kevent kev[KQ_NEVENTS];

	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);

	if (fp->f_type != DTYPE_KQUEUE) {
		error = EBADF;
		goto done;
	}

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

	kq = fp->f_data;
	nerrors = 0;

	while (SCARG(uap, nchanges) > 0) {
		n = SCARG(uap, nchanges) > KQ_NEVENTS ?
		    KQ_NEVENTS : SCARG(uap, nchanges);
		error = copyin(SCARG(uap, changelist), kev,
		    n * sizeof(struct kevent));
		if (error)
			goto done;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrevent(p, kev, n);
#endif
		for (i = 0; i < n; i++) {
			kevp = &kev[i];
			kevp->flags &= ~EV_SYSFLAGS;
			error = kqueue_register(kq, kevp, p);
			if (error || (kevp->flags & EV_RECEIPT)) {
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
		if (kev->ident > INT_MAX)
			return (EBADF);
		if ((fp = fd_getfile(fdp, kev->ident)) == NULL)
			return (EBADF);

		if (kev->ident < kq->kq_knlistsize) {
			SLIST_FOREACH(kn, &kq->kq_knlist[kev->ident], kn_link) {
				if (kev->filter == kn->kn_filter)
					break;
			}
		}
	} else {
		if (kq->kq_knhashmask != 0) {
			struct klist *list;

			list = &kq->kq_knhash[
			    KN_HASH((u_long)kev->ident, kq->kq_knhashmask)];
			SLIST_FOREACH(kn, list, kn_link) {
				if (kev->ident == kn->kn_id &&
				    kev->filter == kn->kn_filter)
					break;
			}
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

			knote_attach(kn);
			if ((error = fops->f_attach(kn)) != 0) {
				knote_drop(kn, p);
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
		knote_drop(kn, p);
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
		if (kn->kn_fop->f_event(kn, 0))
			kn->kn_status |= KN_ACTIVE;
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
	struct timespec ats, rts, tts;
	struct knote *kn, marker;
	int s, count, timeout, nkev = 0, error = 0;
	struct kevent kev[KQ_NEVENTS];

	count = maxevents;
	if (count == 0)
		goto done;

	if (tsp != NULL) {
		ats = *tsp;
		if (!timespecisset(&ats)) {
			/* No timeout, just poll */
			timeout = -1;
			goto start;
		}
		if (timespecfix(&ats)) {
			error = EINVAL;
			goto done;
		}

		timeout = ats.tv_sec > 24 * 60 * 60 ?
		    24 * 60 * 60 * hz : tstohz(&ats);

		getnanouptime(&rts);
		timespecadd(&ats, &rts, &ats);
	} else {
		timespecclear(&ats);
		timeout = 0;
	}
	goto start;

retry:
	if (timespecisset(&ats)) {
		getnanouptime(&rts);
		if (timespeccmp(&rts, &ats, >=))
			goto done;
		tts = ats;
		timespecsub(&tts, &rts, &tts);
		timeout = tts.tv_sec > 24 * 60 * 60 ?
		    24 * 60 * 60 * hz : tstohz(&tts);
	}

start:
	if (kq->kq_state & KQ_DYING) {
		error = EBADF;
		goto done;
	}

	kevp = &kev[0];
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
		if (kn == &marker) {
			TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
			splx(s);
			if (count == maxevents)
				goto retry;
			goto done;
		}

		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
		kq->kq_count--;

		if (kn->kn_status & KN_DISABLED) {
			kn->kn_status &= ~KN_QUEUED;
			continue;
		}
		if ((kn->kn_flags & EV_ONESHOT) == 0 &&
		    kn->kn_fop->f_event(kn, 0) == 0) {
			kn->kn_status &= ~(KN_QUEUED | KN_ACTIVE);
			continue;
		}
		*kevp = kn->kn_kevent;
		kevp++;
		nkev++;
		if (kn->kn_flags & EV_ONESHOT) {
			kn->kn_status &= ~KN_QUEUED;
			splx(s);
			kn->kn_fop->f_detach(kn);
			knote_drop(kn, p);
			s = splhigh();
		} else if (kn->kn_flags & (EV_CLEAR | EV_DISPATCH)) {
			if (kn->kn_flags & EV_CLEAR) {
				kn->kn_data = 0;
				kn->kn_fflags = 0;
			}
			if (kn->kn_flags & EV_DISPATCH)
				kn->kn_status |= KN_DISABLED;
			kn->kn_status &= ~(KN_QUEUED | KN_ACTIVE);
		} else {
			TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
			kq->kq_count++;
		}
		count--;
		if (nkev == KQ_NEVENTS) {
			splx(s);
#ifdef KTRACE
			if (KTRPOINT(p, KTR_STRUCT))
				ktrevent(p, kev, nkev);
#endif
			error = copyout(kev, ulistp,
			    sizeof(struct kevent) * nkev);
			ulistp += nkev;
			nkev = 0;
			kevp = &kev[0];
			s = splhigh();
			if (error)
				break;
		}
	}
	TAILQ_REMOVE(&kq->kq_head, &marker, kn_tqe);
	splx(s);
done:
	if (nkev != 0) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrevent(p, kev, nkev);
#endif
		error = copyout(kev, ulistp,
		    sizeof(struct kevent) * nkev);
	}
	*retval = maxevents - count;
	return (error);
}

/*
 * XXX
 * This could be expanded to call kqueue_scan, if desired.
 */
int
kqueue_read(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
kqueue_write(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
kqueue_ioctl(struct file *fp, u_long com, caddr_t data, struct proc *p)
{
	return (ENOTTY);
}

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

int
kqueue_stat(struct file *fp, struct stat *st, struct proc *p)
{
	struct kqueue *kq = fp->f_data;

	memset(st, 0, sizeof(*st));
	st->st_size = kq->kq_count;
	st->st_blksize = sizeof(struct kevent);
	st->st_mode = S_IFIFO;
	return (0);
}

int
kqueue_close(struct file *fp, struct proc *p)
{
	struct kqueue *kq = fp->f_data;
	int i;

	KERNEL_LOCK();

	for (i = 0; i < kq->kq_knlistsize; i++)
		knote_remove(p, &kq->kq_knlist[i]);
	if (kq->kq_knhashmask != 0) {
		for (i = 0; i < kq->kq_knhashmask + 1; i++)
			knote_remove(p, &kq->kq_knhash[i]);
	}
	fp->f_data = NULL;

	kq->kq_state |= KQ_DYING;
	kqueue_wakeup(kq);
	KQRELE(kq);

	KERNEL_UNLOCK();

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
	struct knote *kn, *kn0;

	SLIST_FOREACH_SAFE(kn, list, kn_selnext, kn0)
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
		knote_drop(kn, p);
	}
}

/*
 * remove all knotes referencing a specified fd
 */
void
knote_fdclose(struct proc *p, int fd)
{
	struct kqueue *kq;
	struct klist *list;

	LIST_FOREACH(kq, &p->p_p->ps_kqlist, kq_next) {
		if (fd >= kq->kq_knlistsize)
			continue;

		list = &kq->kq_knlist[fd];
		knote_remove(p, list);
	}
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
knote_attach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	struct klist *list;
	int size;

	if (!kn->kn_fop->f_isfd) {
		if (kq->kq_knhashmask == 0)
			kq->kq_knhash = hashinit(KN_HASHSIZE, M_TEMP,
			    M_WAITOK, &kq->kq_knhashmask);
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];
		goto done;
	}

	if (kq->kq_knlistsize <= kn->kn_id) {
		size = kq->kq_knlistsize;
		while (size <= kn->kn_id)
			size += KQEXTENT;
		list = mallocarray(size, sizeof(struct klist), M_TEMP,
		    M_WAITOK);
		memcpy(list, kq->kq_knlist,
		    kq->kq_knlistsize * sizeof(struct klist));
		memset(&list[kq->kq_knlistsize], 0,
		    (size - kq->kq_knlistsize) * sizeof(struct klist));
		free(kq->kq_knlist, M_TEMP,
		    kq->kq_knlistsize * sizeof(struct klist));
		kq->kq_knlistsize = size;
		kq->kq_knlist = list;
	}
	list = &kq->kq_knlist[kn->kn_id];
done:
	SLIST_INSERT_HEAD(list, kn, kn_link);
	kn->kn_status = 0;
}

/*
 * should be called at spl == 0, since we don't want to hold spl
 * while calling FRELE and knote_free.
 */
void
knote_drop(struct knote *kn, struct proc *p)
{
	struct kqueue *kq = kn->kn_kq;
	struct klist *list;

	if (kn->kn_fop->f_isfd)
		list = &kq->kq_knlist[kn->kn_id];
	else
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];

	SLIST_REMOVE(list, kn, knote, kn_link);
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	if (kn->kn_fop->f_isfd)
		FRELE(kn->kn_fp, p);
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
