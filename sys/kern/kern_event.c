/*	$OpenBSD: kern_event.c,v 1.131 2020/04/07 13:27:51 visa Exp $	*/

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
#include <sys/atomic.h>
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
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/wait.h>

void	kqueue_init(void);
void	KQREF(struct kqueue *);
void	KQRELE(struct kqueue *);

int	kqueue_sleep(struct kqueue *, struct timespec *);
int	kqueue_scan(struct kqueue *kq, int maxevents,
		    struct kevent *ulistp, struct timespec *timeout,
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

static void	kqueue_expand_hash(struct kqueue *kq);
static void	kqueue_expand_list(struct kqueue *kq, int fd);
static void	kqueue_task(void *);

const struct fileops kqueueops = {
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
int	knote_acquire(struct knote *kn);
void	knote_release(struct knote *kn);

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

const struct filterops kqread_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_kqdetach,
	.f_event	= filt_kqueue,
};

const struct filterops proc_filtops = {
	.f_flags	= 0,
	.f_attach	= filt_procattach,
	.f_detach	= filt_procdetach,
	.f_event	= filt_proc,
};

const struct filterops file_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= filt_fileattach,
	.f_detach	= NULL,
	.f_event	= NULL,
};

const struct filterops timer_filtops = {
	.f_flags	= 0,
	.f_attach	= filt_timerattach,
	.f_detach	= filt_timerdetach,
	.f_event	= filt_timer,
};

struct	pool knote_pool;
struct	pool kqueue_pool;
int kq_ntimeouts = 0;
int kq_timeoutmax = (4 * 1024);

#define KN_HASH(val, mask)	(((val) ^ (val >> 8)) & (mask))

/*
 * Table for for all system-defined filters.
 */
const struct filterops *const sysfilt_ops[] = {
	&file_filtops,			/* EVFILT_READ */
	&file_filtops,			/* EVFILT_WRITE */
	NULL, /*&aio_filtops,*/		/* EVFILT_AIO */
	&file_filtops,			/* EVFILT_VNODE */
	&proc_filtops,			/* EVFILT_PROC */
	&sig_filtops,			/* EVFILT_SIGNAL */
	&timer_filtops,			/* EVFILT_TIMER */
	&file_filtops,			/* EVFILT_DEVICE */
};

void
KQREF(struct kqueue *kq)
{
	atomic_inc_int(&kq->kq_refs);
}

void
KQRELE(struct kqueue *kq)
{
	struct filedesc *fdp;

	if (atomic_dec_int_nv(&kq->kq_refs) > 0)
		return;

	fdp = kq->kq_fdp;
	if (rw_status(&fdp->fd_lock) == RW_WRITE) {
		LIST_REMOVE(kq, kq_next);
	} else {
		fdplock(fdp);
		LIST_REMOVE(kq, kq_next);
		fdpunlock(fdp);
	}

	free(kq->kq_knlist, M_KEVENT, kq->kq_knlistsize *
	    sizeof(struct knlist));
	hashfree(kq->kq_knhash, KN_HASHSIZE, M_KEVENT);
	pool_put(&kqueue_pool, kq);
}

void
kqueue_init(void)
{
	pool_init(&kqueue_pool, sizeof(struct kqueue), 0, IPL_MPFLOOR,
	    PR_WAITOK, "kqueuepl", NULL);
	pool_init(&knote_pool, sizeof(struct knote), 0, IPL_MPFLOOR,
	    PR_WAITOK, "knotepl", NULL);
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
	klist_insert(&kq->kq_sel.si_note, kn);
	return (0);
}

void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	klist_remove(&kq->kq_sel.si_note, kn);
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
	klist_insert(&pr->ps_klist, kn);

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
	klist_remove(&pr->ps_klist, kn);
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
		int s;

		s = splhigh();
		kn->kn_status |= KN_DETACHED;
		splx(s);
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		kn->kn_data = W_EXITCODE(pr->ps_xexit, pr->ps_xsig);
		klist_remove(&pr->ps_klist, kn);
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
	knote_activate(kn);

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

const struct filterops seltrue_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_seltruedetach,
	.f_event	= filt_seltrue,
};

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

static int
filt_dead(struct knote *kn, long hint)
{
	kn->kn_flags |= (EV_EOF | EV_ONESHOT);
	kn->kn_data = 0;
	return (1);
}

static void
filt_deaddetach(struct knote *kn)
{
	/* Nothing to do */
}

static const struct filterops dead_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_deaddetach,
	.f_event	= filt_dead,
};

int
sys_kqueue(struct proc *p, void *v, register_t *retval)
{
	struct filedesc *fdp = p->p_fd;
	struct kqueue *kq;
	struct file *fp;
	int fd, error;

	kq = pool_get(&kqueue_pool, PR_WAITOK | PR_ZERO);
	kq->kq_refs = 1;
	kq->kq_fdp = fdp;
	TAILQ_INIT(&kq->kq_head);
	task_set(&kq->kq_task, kqueue_task, kq);

	fdplock(fdp);
	error = falloc(p, &fp, &fd);
	if (error)
		goto out;
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	fp->f_data = kq;
	*retval = fd;
	LIST_INSERT_HEAD(&fdp->fd_kqlist, kq, kq_next);
	kq = NULL;
	fdinsert(fdp, fd, 0, fp);
	FRELE(fp, p);
out:
	fdpunlock(fdp);
	if (kq != NULL)
		pool_put(&kqueue_pool, kq);
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
	struct timespec *tsp = NULL;
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
		if (ts.tv_sec < 0 || !timespecisvalid(&ts)) {
			error = EINVAL;
			goto done;
		}
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
		tsp = &ts;
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
	    tsp, p, &n);
	KQRELE(kq);
	*retval = n;
	return (error);

 done:
	FRELE(fp, p);
	return (error);
}

#ifdef KQUEUE_DEBUG
void
kqueue_do_check(struct kqueue *kq, const char *func, int line)
{
	struct knote *kn;
	int count = 0, nmarker = 0;

	KERNEL_ASSERT_LOCKED();
	splassert(IPL_HIGH);

	TAILQ_FOREACH(kn, &kq->kq_head, kn_tqe) {
		if (kn->kn_filter == EVFILT_MARKER) {
			if ((kn->kn_status & KN_QUEUED) != 0)
				panic("%s:%d: kq=%p kn=%p marker QUEUED",
				    func, line, kq, kn);
			nmarker++;
		} else {
			if ((kn->kn_status & KN_ACTIVE) == 0)
				panic("%s:%d: kq=%p kn=%p knote !ACTIVE",
				    func, line, kq, kn);
			if ((kn->kn_status & KN_QUEUED) == 0)
				panic("%s:%d: kq=%p kn=%p knote !QUEUED",
				    func, line, kq, kn);
			if (kn->kn_kq != kq)
				panic("%s:%d: kq=%p kn=%p kn_kq=%p != kq",
				    func, line, kq, kn, kn->kn_kq);
			count++;
			if (count > kq->kq_count)
				goto bad;
		}
	}
	if (count != kq->kq_count) {
bad:
		panic("%s:%d: kq=%p kq_count=%d count=%d nmarker=%d",
		    func, line, kq, kq->kq_count, count, nmarker);
	}
}
#define kqueue_check(kq)	kqueue_do_check((kq), __func__, __LINE__)
#else
#define kqueue_check(kq)	do {} while (0)
#endif

int
kqueue_register(struct kqueue *kq, struct kevent *kev, struct proc *p)
{
	struct filedesc *fdp = kq->kq_fdp;
	const struct filterops *fops = NULL;
	struct file *fp = NULL;
	struct knote *kn = NULL, *newkn = NULL;
	struct knlist *list = NULL;
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

	if (fops->f_flags & FILTEROP_ISFD) {
		/* validate descriptor */
		if (kev->ident > INT_MAX)
			return (EBADF);
	}

	if (kev->flags & EV_ADD)
		newkn = pool_get(&knote_pool, PR_WAITOK | PR_ZERO);

again:
	if (fops->f_flags & FILTEROP_ISFD) {
		if ((fp = fd_getfile(fdp, kev->ident)) == NULL) {
			error = EBADF;
			goto done;
		}
		if (kev->flags & EV_ADD)
			kqueue_expand_list(kq, kev->ident);
		if (kev->ident < kq->kq_knlistsize)
			list = &kq->kq_knlist[kev->ident];
	} else {
		if (kev->flags & EV_ADD)
			kqueue_expand_hash(kq);
		if (kq->kq_knhashmask != 0) {
			list = &kq->kq_knhash[
			    KN_HASH((u_long)kev->ident, kq->kq_knhashmask)];
		}
	}
	if (list != NULL) {
		SLIST_FOREACH(kn, list, kn_link) {
			if (kev->filter == kn->kn_filter &&
			    kev->ident == kn->kn_id) {
				s = splhigh();
				if (!knote_acquire(kn)) {
					splx(s);
					if (fp != NULL) {
						FRELE(fp, p);
						fp = NULL;
					}
					goto again;
				}
				splx(s);
				break;
			}
		}
	}
	KASSERT(kn == NULL || (kn->kn_status & KN_PROCESSING) != 0);

	if (kn == NULL && ((kev->flags & EV_ADD) == 0)) {
		error = ENOENT;
		goto done;
	}

	/*
	 * kn now contains the matching knote, or NULL if no match.
	 * If adding a new knote, sleeping is not allowed until the knote
	 * has been inserted.
	 */
	if (kev->flags & EV_ADD) {
		if (kn == NULL) {
			kn = newkn;
			newkn = NULL;
			kn->kn_status = KN_PROCESSING;
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

			/*
			 * If this is a file descriptor filter, check if
			 * fd was closed while the knote was being added.
			 * knote_fdclose() has missed kn if the function
			 * ran before kn appeared in kq_knlist.
			 */
			if ((fops->f_flags & FILTEROP_ISFD) &&
			    fd_checkclosed(fdp, kev->ident, kn->kn_fp)) {
				/*
				 * Drop the knote silently without error
				 * because another thread might already have
				 * seen it. This corresponds to the insert
				 * happening in full before the close.
				 */
				kn->kn_fop->f_detach(kn);
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
			knote_activate(kn);
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

	s = splhigh();
	knote_release(kn);
	splx(s);
done:
	if (fp != NULL)
		FRELE(fp, p);
	if (newkn != NULL)
		pool_put(&knote_pool, newkn);
	return (error);
}

int
kqueue_sleep(struct kqueue *kq, struct timespec *tsp)
{
	struct timespec elapsed, start, stop;
	uint64_t nsecs;
	int error;

	splassert(IPL_HIGH);

	if (tsp != NULL) {
		getnanouptime(&start);
		nsecs = MIN(TIMESPEC_TO_NSEC(tsp), MAXTSLP);
	} else
		nsecs = INFSLP;
	error = tsleep_nsec(kq, PSOCK | PCATCH, "kqread", nsecs);
	if (tsp != NULL) {
		getnanouptime(&stop);
		timespecsub(&stop, &start, &elapsed);
		timespecsub(tsp, &elapsed, tsp);
		if (tsp->tv_sec < 0)
			timespecclear(tsp);
	}

	return (error);
}

int
kqueue_scan(struct kqueue *kq, int maxevents, struct kevent *ulistp,
    struct timespec *tsp, struct proc *p, int *retval)
{
	struct kevent *kevp;
	struct knote mend, mstart, *kn;
	int s, count, nkev = 0, error = 0;
	struct kevent kev[KQ_NEVENTS];

	count = maxevents;
	if (count == 0)
		goto done;

	memset(&mstart, 0, sizeof(mstart));
	memset(&mend, 0, sizeof(mend));

retry:
	if (kq->kq_state & KQ_DYING) {
		error = EBADF;
		goto done;
	}

	kevp = &kev[0];
	s = splhigh();
	if (kq->kq_count == 0) {
		if (tsp != NULL && !timespecisset(tsp)) {
			splx(s);
			error = 0;
			goto done;
		}
		kq->kq_state |= KQ_SLEEP;
		error = kqueue_sleep(kq, tsp);
		splx(s);
		if (error == 0 || error == EWOULDBLOCK)
			goto retry;
		/* don't restart after signals... */
		if (error == ERESTART)
			error = EINTR;
		goto done;
	}

	mstart.kn_filter = EVFILT_MARKER;
	mstart.kn_status = KN_PROCESSING;
	TAILQ_INSERT_HEAD(&kq->kq_head, &mstart, kn_tqe);
	mend.kn_filter = EVFILT_MARKER;
	mend.kn_status = KN_PROCESSING;
	TAILQ_INSERT_TAIL(&kq->kq_head, &mend, kn_tqe);
	while (count) {
		kn = TAILQ_NEXT(&mstart, kn_tqe);
		if (kn->kn_filter == EVFILT_MARKER) {
			if (kn == &mend) {
				TAILQ_REMOVE(&kq->kq_head, &mend, kn_tqe);
				TAILQ_REMOVE(&kq->kq_head, &mstart, kn_tqe);
				splx(s);
				if (count == maxevents)
					goto retry;
				goto done;
			}

			/* Move start marker past another thread's marker. */
			TAILQ_REMOVE(&kq->kq_head, &mstart, kn_tqe);
			TAILQ_INSERT_AFTER(&kq->kq_head, kn, &mstart, kn_tqe);
			continue;
		}

		if (!knote_acquire(kn))
			continue;

		kqueue_check(kq);
		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
		kn->kn_status &= ~KN_QUEUED;
		kq->kq_count--;
		kqueue_check(kq);

		if (kn->kn_status & KN_DISABLED) {
			knote_release(kn);
			continue;
		}
		if ((kn->kn_flags & EV_ONESHOT) == 0 &&
		    kn->kn_fop->f_event(kn, 0) == 0) {
			if ((kn->kn_status & KN_QUEUED) == 0)
				kn->kn_status &= ~KN_ACTIVE;
			knote_release(kn);
			kqueue_check(kq);
			continue;
		}
		*kevp = kn->kn_kevent;
		kevp++;
		nkev++;
		if (kn->kn_flags & EV_ONESHOT) {
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
			if ((kn->kn_status & KN_QUEUED) == 0)
				kn->kn_status &= ~KN_ACTIVE;
			knote_release(kn);
		} else {
			if ((kn->kn_status & KN_QUEUED) == 0) {
				kqueue_check(kq);
				kq->kq_count++;
				kn->kn_status |= KN_QUEUED;
				TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
			}
			knote_release(kn);
		}
		kqueue_check(kq);
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
	TAILQ_REMOVE(&kq->kq_head, &mend, kn_tqe);
	TAILQ_REMOVE(&kq->kq_head, &mstart, kn_tqe);
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

	KASSERT(klist_empty(&kq->kq_sel.si_note));
	task_del(systq, &kq->kq_task);

	KQRELE(kq);

	KERNEL_UNLOCK();

	return (0);
}

static void
kqueue_task(void *arg)
{
	struct kqueue *kq = arg;

	if (kq->kq_state & KQ_SEL) {
		kq->kq_state &= ~KQ_SEL;
		selwakeup(&kq->kq_sel);
	} else {
		KNOTE(&kq->kq_sel.si_note, 0);
	}
	KQRELE(kq);
}

void
kqueue_wakeup(struct kqueue *kq)
{

	if (kq->kq_state & KQ_SLEEP) {
		kq->kq_state &= ~KQ_SLEEP;
		wakeup(kq);
	}
	if ((kq->kq_state & KQ_SEL) || !klist_empty(&kq->kq_sel.si_note)) {
		/* Defer activation to avoid recursion. */
		KQREF(kq);
		if (!task_add(systq, &kq->kq_task))
			KQRELE(kq);
	}
}

static void
kqueue_expand_hash(struct kqueue *kq)
{
	struct knlist *hash;
	u_long hashmask;

	if (kq->kq_knhashmask == 0) {
		hash = hashinit(KN_HASHSIZE, M_KEVENT, M_WAITOK, &hashmask);
		if (kq->kq_knhashmask == 0) {
			kq->kq_knhash = hash;
			kq->kq_knhashmask = hashmask;
		} else {
			/* Another thread has allocated the hash. */
			hashfree(hash, KN_HASHSIZE, M_KEVENT);
		}
	}
}

static void
kqueue_expand_list(struct kqueue *kq, int fd)
{
	struct knlist *list;
	int size;

	if (kq->kq_knlistsize <= fd) {
		size = kq->kq_knlistsize;
		while (size <= fd)
			size += KQEXTENT;
		list = mallocarray(size, sizeof(*list), M_KEVENT, M_WAITOK);
		if (kq->kq_knlistsize <= fd) {
			memcpy(list, kq->kq_knlist,
			    kq->kq_knlistsize * sizeof(*list));
			memset(&list[kq->kq_knlistsize], 0,
			    (size - kq->kq_knlistsize) * sizeof(*list));
			free(kq->kq_knlist, M_KEVENT,
			    kq->kq_knlistsize * sizeof(*list));
			kq->kq_knlist = list;
			kq->kq_knlistsize = size;
		} else {
			/* Another thread has expanded the list. */
			free(list, M_KEVENT, size * sizeof(*list));
		}
	}
}

/*
 * Acquire a knote, return non-zero on success, 0 on failure.
 *
 * If we cannot acquire the knote we sleep and return 0.  The knote
 * may be stale on return in this case and the caller must restart
 * whatever loop they are in.
 */
int
knote_acquire(struct knote *kn)
{
	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);

	if (kn->kn_status & KN_PROCESSING) {
		kn->kn_status |= KN_WAITING;
		tsleep_nsec(kn, 0, "kqepts", SEC_TO_NSEC(1));
		/* knote may be stale now */
		return (0);
	}
	kn->kn_status |= KN_PROCESSING;
	return (1);
}

/*
 * Release an acquired knote, clearing KN_PROCESSING.
 */
void
knote_release(struct knote *kn)
{
	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);
	KASSERT(kn->kn_status & KN_PROCESSING);

	if (kn->kn_status & KN_WAITING) {
		kn->kn_status &= ~KN_WAITING;
		wakeup(kn);
	}
	kn->kn_status &= ~KN_PROCESSING;
	/* kn should not be accessed anymore */
}

/*
 * activate one knote.
 */
void
knote_activate(struct knote *kn)
{
	int s;

	s = splhigh();
	kn->kn_status |= KN_ACTIVE;
	if ((kn->kn_status & (KN_QUEUED | KN_DISABLED)) == 0)
		knote_enqueue(kn);
	splx(s);
}

/*
 * walk down a list of knotes, activating them if their event has triggered.
 */
void
knote(struct klist *list, long hint)
{
	struct knote *kn, *kn0;

	SLIST_FOREACH_SAFE(kn, &list->kl_list, kn_selnext, kn0)
		if (kn->kn_fop->f_event(kn, hint))
			knote_activate(kn);
}

/*
 * remove all knotes from a specified knlist
 */
void
knote_remove(struct proc *p, struct knlist *list)
{
	struct knote *kn;
	int s;

	while ((kn = SLIST_FIRST(list)) != NULL) {
		s = splhigh();
		if (!knote_acquire(kn)) {
			splx(s);
			continue;
		}
		splx(s);
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
	struct filedesc *fdp = p->p_p->ps_fd;
	struct kqueue *kq;
	struct knlist *list;

	/*
	 * fdplock can be ignored if the file descriptor table is being freed
	 * because no other thread can access the fdp.
	 */
	if (fdp->fd_refcnt != 0)
		fdpassertlocked(fdp);

	if (LIST_EMPTY(&fdp->fd_kqlist))
		return;

	KERNEL_LOCK();
	LIST_FOREACH(kq, &fdp->fd_kqlist, kq_next) {
		if (fd >= kq->kq_knlistsize)
			continue;

		list = &kq->kq_knlist[fd];
		knote_remove(p, list);
	}
	KERNEL_UNLOCK();
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
	knote_remove(p, &pr->ps_klist.kl_list);
}

void
knote_attach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	struct knlist *list;

	if (kn->kn_fop->f_flags & FILTEROP_ISFD) {
		KASSERT(kq->kq_knlistsize > kn->kn_id);
		list = &kq->kq_knlist[kn->kn_id];
	} else {
		KASSERT(kq->kq_knhashmask != 0);
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];
	}
	SLIST_INSERT_HEAD(list, kn, kn_link);
}

/*
 * should be called at spl == 0, since we don't want to hold spl
 * while calling FRELE and pool_put.
 */
void
knote_drop(struct knote *kn, struct proc *p)
{
	struct kqueue *kq = kn->kn_kq;
	struct knlist *list;
	int s;

	KASSERT(kn->kn_filter != EVFILT_MARKER);

	if (kn->kn_fop->f_flags & FILTEROP_ISFD)
		list = &kq->kq_knlist[kn->kn_id];
	else
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];

	SLIST_REMOVE(list, kn, knote, kn_link);
	s = splhigh();
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	if (kn->kn_status & KN_WAITING) {
		kn->kn_status &= ~KN_WAITING;
		wakeup(kn);
	}
	splx(s);
	if (kn->kn_fop->f_flags & FILTEROP_ISFD)
		FRELE(kn->kn_fp, p);
	pool_put(&knote_pool, kn);
}


void
knote_enqueue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;

	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);
	KASSERT((kn->kn_status & KN_QUEUED) == 0);

	kqueue_check(kq);
	TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
	kn->kn_status |= KN_QUEUED;
	kq->kq_count++;
	kqueue_check(kq);
	kqueue_wakeup(kq);
}

void
knote_dequeue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;

	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);
	KASSERT(kn->kn_status & KN_QUEUED);

	kqueue_check(kq);
	TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
	kn->kn_status &= ~KN_QUEUED;
	kq->kq_count--;
	kqueue_check(kq);
}

void
klist_insert(struct klist *klist, struct knote *kn)
{
	SLIST_INSERT_HEAD(&klist->kl_list, kn, kn_selnext);
}

void
klist_remove(struct klist *klist, struct knote *kn)
{
	SLIST_REMOVE(&klist->kl_list, kn, knote, kn_selnext);
}

int
klist_empty(struct klist *klist)
{
	return (SLIST_EMPTY(&klist->kl_list));
}

void
klist_invalidate(struct klist *list)
{
	struct knote *kn;
	int s;

	/*
	 * NET_LOCK() must not be held because it can block another thread
	 * in f_event with a knote acquired.
	 */
	NET_ASSERT_UNLOCKED();

	s = splhigh();
	while ((kn = SLIST_FIRST(&list->kl_list)) != NULL) {
		if (!knote_acquire(kn))
			continue;
		splx(s);
		kn->kn_fop->f_detach(kn);
		kn->kn_fop = &dead_filtops;
		knote_activate(kn);
		s = splhigh();
		knote_release(kn);
	}
	splx(s);
}
