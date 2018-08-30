/*	$OpenBSD: sys_futex.c,v 1.9 2018/08/30 03:30:25 visa Exp $ */

/*
 * Copyright (c) 2016-2017 Martin Pieuchot
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/pool.h>
#include <sys/time.h>
#include <sys/rwlock.h>
#include <sys/futex.h>

#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <uvm/uvm.h>

/*
 * Atomicity is only needed on MULTIPROCESSOR kernels.  Fall back on
 * copyin(9) until non-MULTIPROCESSOR architectures have a copyin32(9)
 * implementation.
 */
#ifndef MULTIPROCESSOR
#define copyin32(uaddr, kaddr)	copyin((uaddr), (kaddr), sizeof(uint32_t))
#endif

/*
 * Kernel representation of a futex.
 */
struct futex {
	LIST_ENTRY(futex)	 ft_list;	/* list of all futexes */
	TAILQ_HEAD(, proc)	 ft_threads;	/* sleeping queue */
	struct uvm_object	*ft_obj;	/* UVM object */
	voff_t			 ft_off;	/* UVM offset */
	unsigned int		 ft_refcnt;	/* # of references */
};

/* Syscall helpers. */
int	 futex_wait(uint32_t *, uint32_t, const struct timespec *, int);
int	 futex_wake(uint32_t *, uint32_t, int);
int	 futex_requeue(uint32_t *, uint32_t, uint32_t *, uint32_t, int);

/* Flags for futex_get(). */
#define FT_CREATE	0x1	/* Create a futex if it doesn't exist. */
#define FT_PRIVATE	0x2	/* Futex is process-private. */

struct futex *futex_get(uint32_t *, int);
void	 futex_put(struct futex *);

/*
 * The global futex lock serializes futex(2) calls so that no wakeup
 * event is lost, and protects all futex lists and futex states.
 */
struct rwlock			ftlock = RWLOCK_INITIALIZER("futex");
static struct futex_list	ftlist_shared =
				    LIST_HEAD_INITIALIZER(ftlist_shared);
struct pool			ftpool;


void
futex_init(void)
{
	pool_init(&ftpool, sizeof(struct futex), 0, IPL_NONE,
	    PR_WAITOK | PR_RWLOCK, "futexpl", NULL);
}

int
sys_futex(struct proc *p, void *v, register_t *retval)
{
	struct sys_futex_args /* {
		syscallarg(uint32_t *) f;
		syscallarg(int) op;
		syscallarg(inr) val;
		syscallarg(const struct timespec *) timeout;
		syscallarg(uint32_t *) g;
	} */ *uap = v;
	uint32_t *uaddr = SCARG(uap, f);
	int op = SCARG(uap, op);
	uint32_t val = SCARG(uap, val);
	const struct timespec *timeout = SCARG(uap, timeout);
	void *g = SCARG(uap, g);
	int flags = 0;

	if (op & FUTEX_PRIVATE_FLAG)
		flags |= FT_PRIVATE;

	switch (op) {
	case FUTEX_WAIT:
	case FUTEX_WAIT_PRIVATE:
		KERNEL_LOCK();
		rw_enter_write(&ftlock);
		*retval = futex_wait(uaddr, val, timeout, flags);
		rw_exit_write(&ftlock);
		KERNEL_UNLOCK();
		break;
	case FUTEX_WAKE:
	case FUTEX_WAKE_PRIVATE:
		rw_enter_write(&ftlock);
		*retval = futex_wake(uaddr, val, flags);
		rw_exit_write(&ftlock);
		break;
	case FUTEX_REQUEUE:
	case FUTEX_REQUEUE_PRIVATE:
		rw_enter_write(&ftlock);
		*retval = futex_requeue(uaddr, val, g, (u_long)timeout, flags);
		rw_exit_write(&ftlock);
		break;
	default:
		*retval = ENOSYS;
		break;
	}

	return 0;
}

/*
 * Return an existing futex matching userspace address ``uaddr''.
 *
 * If such futex does not exist and FT_CREATE is given, create it.
 */
struct futex *
futex_get(uint32_t *uaddr, int flags)
{
	struct proc *p = curproc;
	vm_map_t map = &p->p_vmspace->vm_map;
	vm_map_entry_t entry;
	struct uvm_object *obj = NULL;
	voff_t off = (vaddr_t)uaddr;
	struct futex *f;
	struct futex_list *ftlist = &p->p_p->ps_ftlist;

	rw_assert_wrlock(&ftlock);

	if (!(flags & FT_PRIVATE)) {
		vm_map_lock_read(map);
		if (uvm_map_lookup_entry(map, (vaddr_t)uaddr, &entry) &&
		    UVM_ET_ISOBJ(entry) && entry->object.uvm_obj &&
		    entry->inheritance == MAP_INHERIT_SHARE) {
			ftlist = &ftlist_shared;
			obj = entry->object.uvm_obj;
			off = entry->offset + ((vaddr_t)uaddr - entry->start);
		}
		vm_map_unlock_read(map);
	}

	LIST_FOREACH(f, ftlist, ft_list) {
		if (f->ft_obj == obj && f->ft_off == off) {
			f->ft_refcnt++;
			break;
		}
	}

	if ((f == NULL) && (flags & FT_CREATE)) {
		/*
		 * We rely on the rwlock to ensure that no other thread
		 * create the same futex.
		 */
		f = pool_get(&ftpool, PR_WAITOK);
		TAILQ_INIT(&f->ft_threads);
		f->ft_obj = obj;
		f->ft_off = off;
		f->ft_refcnt = 1;
		LIST_INSERT_HEAD(ftlist, f, ft_list);
	}

	return f;
}

/*
 * Release a given futex.
 */
void
futex_put(struct futex *f)
{
	rw_assert_wrlock(&ftlock);

	KASSERT(f->ft_refcnt > 0);

	--f->ft_refcnt;
	if (f->ft_refcnt == 0) {
		KASSERT(TAILQ_EMPTY(&f->ft_threads));
		LIST_REMOVE(f, ft_list);
		pool_put(&ftpool, f);
	}
}

/*
 * Put the current thread on the sleep queue of the futex at address
 * ``uaddr''.  Let it sleep for the specified ``timeout'' time, or
 * indefinitly if the argument is NULL.
 */
int
futex_wait(uint32_t *uaddr, uint32_t val, const struct timespec *timeout,
    int flags)
{
	struct proc *p = curproc;
	struct futex *f;
	uint64_t to_ticks = 0;
	uint32_t cval;
	int error;

	/*
	 * After reading the value a race is still possible but
	 * we deal with it by serializing all futex syscalls.
	 */
	rw_assert_wrlock(&ftlock);

	/*
	 * Read user space futex value
	 */
	if ((error = copyin32(uaddr, &cval)))
		return error;

	/* If the value changed, stop here. */
	if (cval != val)
		return EAGAIN;

	if (timeout != NULL) {
		struct timespec ts;

		if ((error = copyin(timeout, &ts, sizeof(ts))))
			return error;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrabstimespec(p, &ts);
#endif
		to_ticks = (uint64_t)hz * ts.tv_sec +
		    (ts.tv_nsec + tick * 1000 - 1) / (tick * 1000) + 1;
		if (to_ticks > INT_MAX)
			to_ticks = INT_MAX;
	}

	f = futex_get(uaddr, flags | FT_CREATE);
	TAILQ_INSERT_TAIL(&f->ft_threads, p, p_fut_link);
	p->p_futex = f;

	error = rwsleep(p, &ftlock, PUSER|PCATCH, "fsleep", (int)to_ticks);
	if (error == ERESTART)
		error = ECANCELED;
	else if (error == EWOULDBLOCK) {
		/* A race occured between a wakeup and a timeout. */
		if (p->p_futex == NULL)
			error = 0;
		else
			error = ETIMEDOUT;
	}

	/* Remove ourself if we haven't been awaken. */
	if ((f = p->p_futex) != NULL) {
		p->p_futex = NULL;
		TAILQ_REMOVE(&f->ft_threads, p, p_fut_link);
		futex_put(f);
	}

	return error;
}

/*
 * Wakeup at most ``n'' sibling threads sleeping on a futex at address
 * ``uaddr'' and requeue at most ``m'' sibling threads on a futex at
 * address ``uaddr2''.
 */
int
futex_requeue(uint32_t *uaddr, uint32_t n, uint32_t *uaddr2, uint32_t m,
    int flags)
{
	struct futex *f, *g;
	struct proc *p;
	uint32_t count = 0;

	rw_assert_wrlock(&ftlock);

	f = futex_get(uaddr, flags);
	if (f == NULL)
		return 0;

	while ((p = TAILQ_FIRST(&f->ft_threads)) != NULL && (count < (n + m))) {
		p->p_futex = NULL;
		TAILQ_REMOVE(&f->ft_threads, p, p_fut_link);
		futex_put(f);

		if (count < n) {
			wakeup_one(p);
		} else if (uaddr2 != NULL) {
			g = futex_get(uaddr2, FT_CREATE);
			TAILQ_INSERT_TAIL(&g->ft_threads, p, p_fut_link);
			p->p_futex = g;
		}
		count++;
	}

	futex_put(f);

	return count;
}

/*
 * Wakeup at most ``n'' sibling threads sleeping on a futex at address
 * ``uaddr''.
 */
int
futex_wake(uint32_t *uaddr, uint32_t n, int flags)
{
	return futex_requeue(uaddr, n, NULL, 0, flags);
}
