# $Id: Makefile.inc,v 1.5 1999/03/24 00:46:23 d Exp $
# $OpenBSD: Makefile.inc,v 1.5 1999/03/24 00:46:23 d Exp $

# POSIX thread man files

.PATH: ${.CURDIR}/man

MAN+=	pthread_atfork.3 \
	pthread_cleanup_pop.3 \
	pthread_cleanup_push.3 \
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
	pthread_getspecific.3 \
	pthread_join.3 \
	pthread_key_create.3 \
	pthread_key_delete.3 \
	pthread_mutex_destroy.3 \
	pthread_mutex_init.3 \
	pthread_mutex_lock.3 \
	pthread_mutex_trylock.3 \
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
	pthread_self.3 \
	pthread_setspecific.3 \
	pthread_sigmask.3 \
	pthread_single_np.3 \
	pthread_suspend_np.3 \
	pthread_testcancel.3

MAN+=	pthreads.3 \
	flockfile.3 \
	sigwait.3 \
	getc_unlocked.3 \
	putc_unlocked.3

MLINKS+=flockfile.3 funlockfile.3 \
	flockfile.3 ftrylockfile.3 \
	pthread_rwlock_rdlock.3 pthread_rwlock_tryrdlock.3 \
	pthread_rwlock_wrlock.3 pthread_rwlock_trywrlock.3 \
	pthread_cancel.3 pthread_setcancelstate.3 \
	pthread_cancel.3 pthread_setcanceltype.3 \
	getc_unlocked.3 getchar_unlocked.3 \
	putc_unlocked.3 putchar_unlocked.3 \
	pthread_suspend_np.3 pthread_resume_np.3 \
	pthread_single_np.3 pthread_multi_np.3
	
