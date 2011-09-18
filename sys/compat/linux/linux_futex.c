/* $OpenBSD: linux_futex.c,v 1.1 2011/09/18 02:23:18 pirofti Exp $ */
/*	$NetBSD: linux_futex.c,v 1.26 2010/07/07 01:30:35 chs Exp $ */

/*-
 * Copyright (c) 2011 Paul Irofti <pirofti@openbsd.org>
 * Copyright (c) 2005 Emmanuel Dreyfus, all rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Emmanuel Dreyfus
 * 4. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE THE AUTHOR AND CONTRIBUTORS ``AS IS'' 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stdint.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/kernel.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_fcntl.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_mmap.h>
#include <compat/linux/linux_sched.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_dirent.h>
#include <compat/linux/linux_emuldata.h>

#include <compat/linux/linux_time.h>
#include <compat/linux/linux_futex.h>

#ifdef COMPATFUTEX_DEBUG
#define DPRINTF(x) 	printf x
#else
#define DPRINTF(x)
#endif

struct pool futex_pool;
struct pool futex_wp_pool;

struct futex;

struct waiting_proc {
	struct proc *p;
	struct futex *wp_new_futex;
	TAILQ_ENTRY(waiting_proc) wp_list;
	TAILQ_ENTRY(waiting_proc) wp_rqlist;
};

struct futex {
	void *f_uaddr;
	int f_refcount;
	LIST_ENTRY(futex) f_list;
	TAILQ_HEAD(, waiting_proc) f_waiting_proc;
	TAILQ_HEAD(, waiting_proc) f_requeue_proc;
};

static LIST_HEAD(futex_list, futex) futex_list;

struct mutex futex_lock;
void futex_pool_init(void);

int linux_do_futex(struct proc *, const struct linux_sys_futex_args *,
    register_t *, struct timespec *);

struct futex * futex_get(void *);
void futex_ref(struct futex *);
void futex_put(struct futex *);

int futex_sleep(struct futex **, struct proc *, int, struct waiting_proc *);
int futex_wake(struct futex *, int, struct futex *, int);
int futex_atomic_op(struct proc *, int, void *);

int futex_itimespecfix(struct timespec *ts);

int
linux_sys_futex(struct proc *p, void *v, register_t *retval)
{
        struct linux_sys_futex_args /* {
		syscallarg(int *) uaddr;
		syscallarg(int) op;
		syscallarg(int) val;
		syscallarg(const struct linux_timespec *) timeout;
		syscallarg(int *) uaddr2;
		syscallarg(int) val3;
	} */ *uap = v;

	struct l_timespec lts;
	struct timespec ts = {0, 0};
	int error;

	if ((SCARG(uap, op) & ~LINUX_FUTEX_PRIVATE_FLAG) == LINUX_FUTEX_WAIT &&
	    SCARG(uap, timeout) != NULL) {
		if ((error = copyin(SCARG(uap, timeout),
		    &lts, sizeof(lts))) != 0) {
			return error;
		}
		linux_to_native_timespec(&ts, &lts);
	}
	
	return linux_do_futex(p, uap, retval, &ts);
}

int
linux_do_futex(struct proc *p, const struct linux_sys_futex_args *uap,
    register_t *retval, struct timespec *ts)
{
	/* {
		syscallarg(int *) uaddr;
		syscallarg(int) op;
		syscallarg(int) val;
		syscallarg(const struct linux_timespec *) timeout;
		syscallarg(int *) uaddr2;
		syscallarg(int) val3;
	} */
	int val;
	int ret;
	int error = 0;
	struct futex *f;
	struct futex *newf;
	int timeout_hz;
	struct timeval tv;
	struct futex *f2;
	struct waiting_proc *wp;
	int op_ret;

	int args_val = SCARG(uap, val);

	/*
	 * Our implementation provides only private futexes. Most of the apps
	 * should use private futexes but don't claim so. Therefore we treat
	 * all futexes as private by clearing the FUTEX_PRIVATE_FLAG. It works
	 * in most cases (ie. when futexes are not shared on file descriptor
	 * or between different processes).
	 */
	switch (SCARG(uap, op) & ~LINUX_FUTEX_PRIVATE_FLAG) {
	case LINUX_FUTEX_WAIT:

		mtx_enter(&futex_lock);
		if ((error = copyin(SCARG(uap, uaddr), 
		    &val, sizeof(val))) != 0) {
			mtx_leave(&futex_lock);
			return error;
		}

		if (val != args_val) {
			mtx_leave(&futex_lock);
			return EWOULDBLOCK;
		}

		DPRINTF(("FUTEX_WAIT %d: val = %d, uaddr = %p, "
		    "*uaddr = %d, timeout = %lld.%09ld\n", 
		    p->p_pid, args_val, SCARG(uap, uaddr), val,
		    (long long)ts->tv_sec, ts->tv_nsec));

		if ((error = futex_itimespecfix(ts)) != 0) {
			mtx_leave(&futex_lock);
			return error;
		}
		TIMESPEC_TO_TIMEVAL(&tv, ts);
		timeout_hz = tvtohz(&tv);

		/*
		 * If the user process requests a non null timeout,
		 * make sure we do not turn it into an infinite
		 * timeout because timeout_hz is 0.
		 *
		 * We use a minimal timeout of 1/hz. Maybe it would make
		 * sense to just return ETIMEDOUT without sleeping.
		 */
		if (SCARG(uap, timeout) != NULL && timeout_hz == 0)
			timeout_hz = 1;

		wp = pool_get(&futex_wp_pool, PR_WAITOK);

		f = futex_get(SCARG(uap, uaddr));
		ret = futex_sleep(&f, p, timeout_hz, wp);
		futex_put(f);
		mtx_leave(&futex_lock);

		pool_put(&futex_wp_pool, wp);

		DPRINTF(("FUTEX_WAIT %d: uaddr = %p, "
		    "ret = %d\n", p->p_pid,
		    SCARG(uap, uaddr), ret));

		switch (ret) {
		case EWOULDBLOCK:	/* timeout */
			return ETIMEDOUT;
			break;
		case EINTR:		/* signal */
			return EINTR;
			break;
		case 0:			/* FUTEX_WAKE received */
			DPRINTF(("FUTEX_WAIT %d: uaddr = %p, got it\n",
			    p->p_pid, SCARG(uap, uaddr)));
			return 0;
			break;
		default:
			DPRINTF(("FUTEX_WAIT: unexpected ret = %d\n", ret));
			break;
		}

		/* NOTREACHED */
		break;
		
	case LINUX_FUTEX_WAKE:
		/* 
		 * XXX: Linux is able cope with different addresses 
		 * corresponding to the same mapped memory in the sleeping 
		 * and the waker process(es).
		 */
		DPRINTF(("FUTEX_WAKE %d: uaddr = %p, val = %d\n",
		    p->p_pid, SCARG(uap, uaddr), args_val));

		if (args_val < 0)
			return EINVAL;

		mtx_enter(&futex_lock);
		f = futex_get(SCARG(uap, uaddr));
		*retval = futex_wake(f, args_val, NULL, 0);
		futex_put(f);
		mtx_leave(&futex_lock);

		break;

	case LINUX_FUTEX_CMP_REQUEUE:

		if (args_val < 0)
			return EINVAL;

		mtx_enter(&futex_lock);
		if ((error = copyin(SCARG(uap, uaddr), 
		    &val, sizeof(val))) != 0) {
			mtx_leave(&futex_lock);
			return error;
		}

		if (val != SCARG(uap, val3)) {
			mtx_leave(&futex_lock);
			return EAGAIN;
		}

		DPRINTF(("FUTEX_CMP_REQUEUE %d: uaddr = %p, val = %d, "
		    "uaddr2 = %p, val2 = %d\n",
		    p->p_pid, SCARG(uap, uaddr), args_val, SCARG(uap, uaddr2),
		    (int)(unsigned long)SCARG(uap, timeout)));

		f = futex_get(SCARG(uap, uaddr));
		newf = futex_get(SCARG(uap, uaddr2));
		*retval = futex_wake(f, args_val, newf,
		    (int)(unsigned long)SCARG(uap, timeout));
		futex_put(f);
		futex_put(newf);
		mtx_leave(&futex_lock);

		break;

	case LINUX_FUTEX_REQUEUE:
		DPRINTF(("FUTEX_REQUEUE %d: uaddr = %p, val = %d, "
		    "uaddr2 = %p, val2 = %d\n",
		    p->p_pid, SCARG(uap, uaddr), args_val, SCARG(uap, uaddr2),
		    (int)(unsigned long)SCARG(uap, timeout)));

		if (args_val < 0)
			return EINVAL;

		mtx_enter(&futex_lock);
		f = futex_get(SCARG(uap, uaddr));
		newf = futex_get(SCARG(uap, uaddr2));
		*retval = futex_wake(f, args_val, newf,
		    (int)(unsigned long)SCARG(uap, timeout));
		futex_put(f);
		futex_put(newf);
		mtx_leave(&futex_lock);

		break;

	case LINUX_FUTEX_FD:
		DPRINTF(("linux_sys_futex: unimplemented op %d\n", 
		    SCARG(uap, op)));
		return ENOSYS;
	case LINUX_FUTEX_WAKE_OP:
		DPRINTF(("FUTEX_WAKE_OP %d: uaddr = %p, op = %d, "
		    "val = %d, uaddr2 = %p, val2 = %d\n",
		    p->p_pid, SCARG(uap, uaddr), SCARG(uap, op), args_val,
		    SCARG(uap, uaddr2),
		    (int)(unsigned long)SCARG(uap, timeout)));

		if (args_val < 0)
			return EINVAL;

		mtx_enter(&futex_lock);
		f = futex_get(SCARG(uap, uaddr));
		f2 = futex_get(SCARG(uap, uaddr2));
		mtx_leave(&futex_lock);

		/*
		 * This function returns a positive number as results and
		 * negative as errors
		 */
		op_ret = futex_atomic_op(p, SCARG(uap, val3),
		    SCARG(uap, uaddr2));
		if (op_ret < 0) {
			futex_put(f);
			futex_put(f2);
			return -op_ret;
		}

		mtx_enter(&futex_lock);
		ret = futex_wake(f, args_val, NULL, 0);
		futex_put(f);
		if (op_ret > 0) {
			op_ret = 0;
			/*
			 * Linux abuses the address of the timespec parameter
			 * as the number of retries
			 */
			op_ret += futex_wake(f2,
			    (int)(unsigned long)SCARG(uap, timeout), NULL, 0);
			ret += op_ret;
		}
		futex_put(f2);
		mtx_leave(&futex_lock);
		*retval = ret;
		break;
	default:
		DPRINTF(("linux_sys_futex: unknown op %d\n", 
		    SCARG(uap, op)));
		return ENOSYS;
	}
	return 0;
}

void
futex_pool_init(void)
{
	DPRINTF(("Inside futex_pool_init()\n"));
	pool_init(&futex_pool, sizeof(struct futex), 0, 0, PR_DEBUGCHK,
	    "futexpl", &pool_allocator_nointr);
	pool_init(&futex_wp_pool, sizeof(struct waiting_proc), 0, 0,
	    PR_DEBUGCHK, "futexwppl", &pool_allocator_nointr);
}

/*
 * Get a futex.
 * If we have an existing one, we will return that with the refcount bumped.
 * Otherwise we will allocate and hook up a new one.
 * Must be called with futex_lock held, but we may unlock it in order to
 * sleep for allocation.
 */
struct futex *
futex_get(void *uaddr)
{
	struct futex *f, *newf;

	MUTEX_ASSERT_LOCKED(&futex_lock);
	LIST_FOREACH(f, &futex_list, f_list) {
		if (f->f_uaddr == uaddr) {
			f->f_refcount++;
			return f;
		}
	}
	mtx_leave(&futex_lock);

	/* Not found, create it */
	newf = pool_get(&futex_pool, PR_WAITOK|PR_ZERO);

	mtx_enter(&futex_lock);
	/* Did someone else create it in the meantime? */
	LIST_FOREACH(f, &futex_list, f_list) {
		if (f->f_uaddr == uaddr) {
			f->f_refcount++;
			pool_put(&futex_pool, newf);
			return f;
		}
	}
	newf->f_uaddr = uaddr;
	newf->f_refcount = 1;
	TAILQ_INIT(&newf->f_waiting_proc);
	TAILQ_INIT(&newf->f_requeue_proc);
	LIST_INSERT_HEAD(&futex_list, newf, f_list);

	return newf;
}

/*
 * Grab a reference on a futex.
 * The futex lock must be locked.
 */
void
futex_ref(struct futex *f)
{
	MUTEX_ASSERT_LOCKED(&futex_lock);
	f->f_refcount++;
}

/*
 * Release our reference on the futex.
 * must be called with the futex_lock held.
 */
void 
futex_put(struct futex *f)
{
	MUTEX_ASSERT_LOCKED(&futex_lock);
	f->f_refcount--;
	if (f->f_refcount == 0) {
		KASSERT(TAILQ_EMPTY(&f->f_waiting_proc));
		KASSERT(TAILQ_EMPTY(&f->f_requeue_proc));
		LIST_REMOVE(f, f_list);
		pool_put(&futex_pool, f);
	}
}

int 
futex_sleep(struct futex **fp, struct proc *p, int timeout,
    struct waiting_proc *wp)
{
	struct futex *f, *newf;
	int ret;

	MUTEX_ASSERT_LOCKED(&futex_lock);

	f = *fp;
	wp->p = p;
	wp->wp_new_futex = NULL;

requeue:
	TAILQ_INSERT_TAIL(&f->f_waiting_proc, wp, wp_list);

	ret = msleep(&f, &futex_lock, PUSER | PCATCH, "futex_sleep", timeout);

	TAILQ_REMOVE(&f->f_waiting_proc, wp, wp_list);

	/* if futex_wake() tells us to requeue ... */
	newf = wp->wp_new_futex;
	if (ret == 0 && newf != NULL) {
		/* ... requeue ourselves on the new futex */
		futex_put(f);
		wp->wp_new_futex = NULL;
		TAILQ_REMOVE(&newf->f_requeue_proc, wp, wp_rqlist);
		*fp = f = newf;
		goto requeue;
	}
	return ret;
}

int
futex_wake(struct futex *f, int n, struct futex *newf, int n2)
{
	struct waiting_proc *wp;
	int count;

	KASSERT(newf != f);
	MUTEX_ASSERT_LOCKED(&futex_lock);

	count = newf ? 0 : 1;

	/*
	 * first, wake up any threads sleeping on this futex.
	 * note that sleeping threads are not in the process of requeueing.
	 */
	if (!TAILQ_EMPTY(&f->f_waiting_proc))
		wakeup(&f); /* only call wakeup once */
	TAILQ_FOREACH(wp, &f->f_waiting_proc, wp_list) {
		KASSERT(wp->wp_new_futex == NULL);

		DPRINTF(("futex_wake: signal f %p ref %d\n",
			     f, f->f_refcount));
		if (count <= n) {
			count++;
		} else {
			if (newf == NULL)
				break;

			/* matching futex_put() is called by the other thread. */
			futex_ref(newf);
			wp->wp_new_futex = newf;
			TAILQ_INSERT_TAIL(&newf->f_requeue_proc, wp, wp_rqlist);
			DPRINTF(("futex_wake: requeue newf %p ref %d\n",
				     newf, newf->f_refcount));
			if (count - n >= n2)
				goto out;
		}
	}

	/*
	 * next, deal with threads that are requeuing to this futex.
	 * we don't need to signal these threads, any thread on the
	 * requeue list has already been signaled but hasn't had a chance
	 * to run and requeue itself yet.  if we would normally wake
	 * a thread, just remove the requeue info.  if we would normally
	 * requeue a thread, change the requeue target.
	 */
	while ((wp = TAILQ_FIRST(&f->f_requeue_proc)) != NULL) {
		/* XXX: talk to oga, should mtx_enter again, recursive */
		KASSERT(wp->wp_new_futex == f);

		DPRINTF(("futex_wake: unrequeue f %p ref %d\n",
			     f, f->f_refcount));
		wp->wp_new_futex = NULL;
		TAILQ_REMOVE(&f->f_requeue_proc, wp, wp_rqlist);
		futex_put(f);

		if (count <= n) {
			count++;
		} else {
			if (newf == NULL) {
				break;
			}

			/*matching futex_put() is called by the other thread.*/
			futex_ref(newf);
			wp->wp_new_futex = newf;
			TAILQ_INSERT_TAIL(&newf->f_requeue_proc, wp, wp_rqlist);
			DPRINTF(("futex_wake: rerequeue newf %p ref %d\n",
				     newf, newf->f_refcount));
			if (count - n >= n2)
				break;
		}
	}

out:
	return count;
}

int
futex_atomic_op(struct proc *p, int encoded_op, void *uaddr)
{
	const int op = (encoded_op >> 28) & 7;
	const int cmp = (encoded_op >> 24) & 15;
	const int cmparg = (encoded_op << 20) >> 20;
	int oparg = (encoded_op << 8) >> 20;
	int error, oldval, cval;

	if (encoded_op & (FUTEX_OP_OPARG_SHIFT << 28))
		oparg = 1 << oparg;

	/* XXX: linux verifies access here and returns EFAULT */

	if (copyin(uaddr, &cval, sizeof(int)) != 0)
		return -EFAULT;

	for (;;) {
		int nval;

		switch (op) {
		case FUTEX_OP_SET:
			nval = oparg;
			break;
		case FUTEX_OP_ADD:
			nval = cval + oparg;
			break;
		case FUTEX_OP_OR:
			nval = cval | oparg;
			break;
		case FUTEX_OP_ANDN:
			nval = cval & ~oparg;
			break;
		case FUTEX_OP_XOR:
			nval = cval ^ oparg;
			break;
		default:
			return -ENOSYS;
		}

		oldval = nval;
		error = atomic_ucas_32(uaddr, cval, nval);
		if (oldval == cval || error) {
			break;
		}
		cval = oldval;
	}

	if (error)
		return -EFAULT;

	switch (cmp) {
	case FUTEX_OP_CMP_EQ:
		return (oldval == cmparg);
	case FUTEX_OP_CMP_NE:
		return (oldval != cmparg);
	case FUTEX_OP_CMP_LT:
		return (oldval < cmparg);
	case FUTEX_OP_CMP_GE:
		return (oldval >= cmparg);
	case FUTEX_OP_CMP_LE:
		return (oldval <= cmparg);
	case FUTEX_OP_CMP_GT:
		return (oldval > cmparg);
	default:
		return -ENOSYS;
	}
}

int
linux_sys_set_robust_list(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_set_robust_list_args /* {
		syscallarg(struct linux_robust_list_head *) head;
		syscallarg(size_t) len;
	} */ *uap = v;
	struct linux_emuldata *led;

	if (SCARG(uap, len) != sizeof(struct linux_robust_list_head))
		return EINVAL;
	led = p->p_emuldata;
	led->led_robust_head = SCARG(uap, head);
	*retval = 0;
	return 0;
}

int
linux_sys_get_robust_list(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_get_robust_list_args /* {
		syscallarg(int) pid;
		syscallarg(struct linux_robust_list_head **) head;
		syscallarg(size_t *) len;
	} */ *uap = v;
	struct proc *q;
	struct linux_emuldata *led;
	struct linux_robust_list_head *head;
	size_t len;
	int error = 0;

	if (!SCARG(uap, pid)) {
		led = p->p_emuldata;
		head = led->led_robust_head;
	} else {
		if (!SCARG(uap, pid))
			q = p;
		else if ((q = pfind(SCARG(uap, pid))) == NULL)
			return ESRCH;
		else if (p->p_p != q->p_p)
			return EPERM;

		led = q->p_emuldata;
		head = led->led_robust_head;
	}

	len = sizeof(*head);
	error = copyout(&len, SCARG(uap, len), sizeof(len));
	if (error)
		return error;
	return copyout(&head, SCARG(uap, head), sizeof(head));
}

int
futex_itimespecfix(struct timespec *ts)
{
	if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000)
		return EINVAL;
	if (ts->tv_sec == 0 && ts->tv_nsec != 0 && ts->tv_nsec < tick * 1000)
		ts->tv_nsec = tick * 1000;
	return 0;
}

