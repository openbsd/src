# $Id: Makefile.inc,v 1.4 1999/01/18 00:00:32 d Exp $
# $OpenBSD: Makefile.inc,v 1.4 1999/01/18 00:00:32 d Exp $

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
	pthread_testcancel.3

MAN+=	pthreads.3 \
	flockfile.3 \
	sigwait.3

MLINKS+=flockfile.3 funlockfile.3 \
	flockfile.3 ftrylockfile.3 \
	pthread_rwlock_rdlock.3 pthread_rwlock_tryrdlock.3 \
	pthread_rwlock_wrlock.3 pthread_rwlock_trywrlock.3 \
	pthread_cancel.3 pthread_setcancelstate.3 \
	pthread_cancel.3 pthread_getcancelstate.3 \
	
