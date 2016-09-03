/*	$OpenBSD: rthread.h,v 1.59 2016/09/03 16:44:20 akfaew Exp $ */
/*
 * Copyright (c) 2004,2005 Ted Unangst <tedu@openbsd.org>
 * All Rights Reserved.
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
/*
 * Private data structures that back up the typedefs in pthread.h.
 * Since only the thread library cares about their size or arrangement,
 * it should be possible to switch libraries without relinking.
 *
 * Do not reorder struct _spinlock and sem_t variables in the structs.
 * This is due to alignment requirements of certain arches like hppa.
 * The current requirement is 16 bytes.
 *
 * THE MACHINE DEPENDENT CERROR CODE HAS HARD CODED OFFSETS INTO PTHREAD_T!
 */

#include <sys/queue.h>
#include <semaphore.h>
#include <machine/spinlock.h>

#ifdef __LP64__
#define RTHREAD_STACK_SIZE_DEF (512 * 1024)
#else
#define RTHREAD_STACK_SIZE_DEF (256 * 1024)
#endif

/*
 * tickets don't work yet? (or seem much slower, with lots of system time)
 * until then, keep the struct around to avoid excessive changes going
 * back and forth.
 */
struct _spinlock {
	_atomic_lock_t ticket;
};

#define	_SPINLOCK_UNLOCKED { _ATOMIC_LOCK_UNLOCKED }
extern struct _spinlock _SPINLOCK_UNLOCKED_ASSIGN;

struct stack {
	SLIST_ENTRY(stack)	link;	/* link for free default stacks */
	void	*sp;			/* machine stack pointer */
	void	*base;			/* bottom of allocated area */
	size_t	guardsize;		/* size of PROT_NONE zone or */
					/* ==1 if application alloced */
	size_t	len;			/* total size of allocated stack */
};

struct __sem {
	struct _spinlock lock;
	volatile int waitcount;
	volatile int value;
	int shared;
};

TAILQ_HEAD(pthread_queue, pthread);

struct pthread_mutex {
	struct _spinlock lock;
	struct pthread_queue lockers;
	int type;
	pthread_t owner;
	int count;
	int prioceiling;
};

struct pthread_mutex_attr {
	int ma_type;
	int ma_protocol;
	int ma_prioceiling;
};

struct pthread_cond {
	struct _spinlock lock;
	struct pthread_queue waiters;
	struct pthread_mutex *mutex;
	clockid_t clock;
};

struct pthread_cond_attr {
	clockid_t ca_clock;
};

struct pthread_rwlock {
	struct _spinlock lock;
	pthread_t owner;
	struct pthread_queue writers;
	int readers;
};

struct pthread_rwlockattr {
	int pshared;
};

struct pthread_attr {
	void *stack_addr;
	size_t stack_size;
	size_t guard_size;
	int detach_state;
	int contention_scope;
	int sched_policy;
	struct sched_param sched_param;
	int sched_inherit;
};

#define	PTHREAD_MIN_PRIORITY	0
#define	PTHREAD_MAX_PRIORITY	31

struct rthread_key {
	int used;
	void (*destructor)(void *);
};

struct rthread_storage {
	int keyid;
	struct rthread_storage *next;
	void *data;
};

struct rthread_cleanup_fn {
	void (*fn)(void *);
	void *arg;
	struct rthread_cleanup_fn *next;
};

struct pthread_barrier {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int threshold;
	int in;
	int out;
	int generation;
};

struct pthread_barrierattr {
	int pshared;
};

struct pthread_spinlock {
	struct _spinlock lock;
	pthread_t owner;
};

struct tib;
struct pthread {
	struct __sem donesem;
	unsigned int flags;
	struct _spinlock flags_lock;
	struct tib *tib;
	void *retval;
	void *(*fn)(void *);
	void *arg;
	char name[32];
	struct stack *stack;
	LIST_ENTRY(pthread) threads;
	TAILQ_ENTRY(pthread) waiting;
	pthread_cond_t blocking_cond;
	struct pthread_attr attr;
	struct rthread_storage *local_storage;
	struct rthread_cleanup_fn *cleanup_fns;
	int myerrno;

	/* cancel received in a delayed cancel block? */
	int delayed_cancel;
};
/* flags in pthread->flags */
#define	THREAD_DONE		0x001
#define	THREAD_DETACHED		0x002

/* flags in tib->tib_thread_flags */
#define	TIB_THREAD_ASYNC_CANCEL		0x001
#define	TIB_THREAD_INITIAL_STACK	0x002	/* has stack from exec */

#define ENTER_DELAYED_CANCEL_POINT(tib, self)				\
	(self)->delayed_cancel = 0;					\
	ENTER_CANCEL_POINT_INNER(tib, 1, 1)

#define	ROUND_TO_PAGE(size) \
	(((size) + (_thread_pagesize - 1)) & ~(_thread_pagesize - 1))

__BEGIN_HIDDEN_DECLS
void	_spinlock(volatile struct _spinlock *);
int	_spinlocktry(volatile struct _spinlock *);
void	_spinunlock(volatile struct _spinlock *);
int	_sem_wait(sem_t, int, const struct timespec *, int *);
int	_sem_post(sem_t);

void	_rthread_init(void);
struct stack *_rthread_alloc_stack(pthread_t);
void	_rthread_free_stack(struct stack *);
void	_rthread_tls_destructors(pthread_t);
void	_rthread_debug(int, const char *, ...)
		__attribute__((__format__ (printf, 2, 3)));
void	_rthread_debug_init(void);
#ifndef NO_PIC
void	_rthread_dl_lock(int what);
#endif
void	_thread_malloc_reinit(void);

extern int _threads_ready;
extern size_t _thread_pagesize;
extern LIST_HEAD(listhead, pthread) _thread_list;
extern struct _spinlock _thread_lock;
extern struct pthread_attr _rthread_attr_default;
__END_HIDDEN_DECLS

void	_thread_dump_info(void);

/* syscalls not declared in system headers */
#define REDIRECT_SYSCALL(x)		typeof(x) x asm("_thread_sys_"#x)
void	__threxit(pid_t *);
int	__thrsleep(const volatile void *, clockid_t, const struct timespec *,
	    volatile void *, const int *);
int	__thrwakeup(const volatile void *, int n);
int	__thrsigdivert(sigset_t, siginfo_t *, const struct timespec *);
