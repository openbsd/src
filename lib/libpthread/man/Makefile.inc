# $OpenBSD: Makefile.inc,v 1.37 2019/01/12 00:16:03 jca Exp $
# $FreeBSD: Makefile.inc,v 1.6 1999/08/28 00:03:02 peter Exp $

# POSIX thread man files

.PATH: ${SRCDIR}/man

MAN+=	\
	pthread_atfork.3 \
	pthread_attr_init.3 \
	pthread_attr_setstack.3 \
	pthread_attr_setstackaddr.3 \
	pthread_attr_setstacksize.3 \
	pthread_attr_setguardsize.3 \
	pthread_attr_setdetachstate.3 \
	pthread_barrier_init.3 \
	pthread_barrier_wait.3 \
	pthread_barrierattr_init.3 \
	pthread_barrierattr_getpshared.3 \
	pthread_cleanup_pop.3 \
	pthread_cleanup_push.3 \
	pthread_condattr_init.3 \
	pthread_cond_broadcast.3 \
	pthread_cond_destroy.3 \
	pthread_cond_init.3 \
	pthread_cond_signal.3 \
	pthread_cond_timedwait.3 \
	pthread_cond_wait.3 \
	pthread_cancel.3 \
	pthread_create.3 \
	pthread_detach.3 \
	pthread_equal.3 \
	pthread_exit.3 \
	pthread_getconcurrency.3 \
	pthread_getcpuclockid.3 \
	pthread_getspecific.3 \
	pthread_join.3 \
	pthread_key_create.3 \
	pthread_key_delete.3 \
	pthread_kill.3 \
	pthread_main_np.3 \
	pthread_mutexattr.3 \
	pthread_mutex_destroy.3 \
	pthread_mutex_init.3 \
	pthread_mutex_lock.3 \
	pthread_mutex_unlock.3 \
	pthread_once.3 \
	pthread_rwlock_destroy.3 \
	pthread_rwlock_init.3 \
	pthread_rwlock_rdlock.3 \
	pthread_rwlock_unlock.3 \
	pthread_rwlock_wrlock.3 \
	pthread_rwlockattr_destroy.3 \
	pthread_rwlockattr_getpshared.3 \
	pthread_rwlockattr_init.3 \
	pthread_rwlockattr_setpshared.3 \
	pthread_schedparam.3 \
	pthread_self.3 \
	pthread_set_name_np.3 \
	pthread_setspecific.3 \
	pthread_sigmask.3 \
	pthread_spin_init.3 \
	pthread_spin_lock.3 \
	pthread_spin_unlock.3 \
	pthread_stackseg_np.3 \
	pthread_testcancel.3 \
	pthread_yield.3 \
	sched_get_priority_min.3 \
	sem_destroy.3 \
	sem_getvalue.3 \
	sem_init.3 \
	sem_open.3 \
	sem_post.3 \
	sem_wait.3

MAN+=	pthreads.3 \
	flockfile.3 \
	getc_unlocked.3 \
	putc_unlocked.3
