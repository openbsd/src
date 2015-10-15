/* $OpenBSD: rthread_cancel.c,v 1.10 2015/10/15 16:38:04 deraadt Exp $ */
/* $snafu: libc_tag.c,v 1.4 2004/11/30 07:00:06 marc Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "thread_private.h"	/* in libc/include */

#include "rthread.h"

/*
 * If you add anything here, make sure to add it to the list in the
 * pthread_testcancel(3) manpage too.
 */

int	_thread_sys_accept(int, struct sockaddr *, socklen_t *);
int	_thread_sys_accept4(int, struct sockaddr *, socklen_t *, int);
int	_thread_sys_close(int);
int	_thread_sys_closefrom(int);
int	_thread_sys_connect(int, const struct sockaddr *, socklen_t);
int	_thread_sys_fcntl(int, int, ...);
int	_thread_sys_fsync(int);
int	_thread_sys_msgrcv(int, void *, size_t, long, int);
int	_thread_sys_msgsnd(int, const void *, size_t, int);
int	_thread_sys_msync(void *, size_t, int);
int	_thread_sys_nanosleep(const struct timespec *, struct timespec *);
int	_thread_sys_open(const char *, int, ...);
int	_thread_sys_openat(int, const char *, int, ...);
int	_thread_sys_poll(struct pollfd *, nfds_t, int);
int	_thread_sys_ppoll(struct pollfd *, nfds_t, const struct timespec *,
	    const sigset_t *);
ssize_t	_thread_sys_pread(int, void *, size_t, off_t);
ssize_t	_thread_sys_preadv(int, const struct iovec *, int, off_t);
int	_thread_sys_pselect(int, fd_set *, fd_set *, fd_set *,
	    const struct timespec *, const sigset_t *);
ssize_t	_thread_sys_pwrite(int, const void *, size_t, off_t);
ssize_t	_thread_sys_pwritev(int, const struct iovec *, int, off_t);
ssize_t	_thread_sys_read(int, void *, size_t);
ssize_t	_thread_sys_readv(int, const struct iovec *, int);
ssize_t	_thread_sys_recvfrom(int, void *, size_t, int, struct sockaddr *,
	    socklen_t *);
ssize_t	_thread_sys_recvmsg(int, struct msghdr *, int);
int	_thread_sys_select(int, fd_set *, fd_set *, fd_set *,
	    struct timeval *);
ssize_t	_thread_sys_sendmsg(int, const struct msghdr *, int);
ssize_t	_thread_sys_sendto(int, const void *, size_t, int,
	    const struct sockaddr *, socklen_t);
int	_thread_sys_sigsuspend(const sigset_t *);
pid_t	_thread_sys_wait4(pid_t, int *, int, struct rusage *);
ssize_t	_thread_sys_write(int, const void *, size_t);
ssize_t	_thread_sys_writev(int, const struct iovec *, int);

void
_enter_cancel(pthread_t self)
{
	if (self->flags & THREAD_CANCEL_ENABLE) {
		self->cancel_point++;
		if (IS_CANCELED(self))
			pthread_exit(PTHREAD_CANCELED);
	}
}

void
_leave_cancel(pthread_t self)
{
	if (self->flags & THREAD_CANCEL_ENABLE) 
		self->cancel_point--;
}

void
_enter_delayed_cancel(pthread_t self)
{
	if (self->flags & THREAD_CANCEL_ENABLE) {
		self->delayed_cancel = 0;
		self->cancel_point++;
		if (IS_CANCELED(self))
			pthread_exit(PTHREAD_CANCELED);
		_rthread_setflag(self, THREAD_CANCEL_DELAY);
	}
}

void
_leave_delayed_cancel(pthread_t self, int can_cancel)
{
	if (self->flags & THREAD_CANCEL_ENABLE) {
		if (self->flags & THREAD_CANCEL_DELAY) {
			self->cancel_point--;
			_rthread_clearflag(self, THREAD_CANCEL_DELAY);
		}
		if (IS_CANCELED(self) && can_cancel)
			pthread_exit(PTHREAD_CANCELED);
		self->delayed_cancel = 0;
	}
}

int
accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_accept(fd, addr, addrlen);
	_leave_cancel(self);
	return (rv);
}

int
accept4(int fd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_accept4(fd, addr, addrlen, flags);
	_leave_cancel(self);
	return (rv);
}

#if 0
aio_suspend()			/* don't have yet */
clock_nanosleep()		/* don't have yet */
#endif

int
close(int fd)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_close(fd);
	_leave_cancel(self);
	return (rv);
}


int
closefrom(int fd)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_closefrom(fd);
	_leave_cancel(self);
	return (rv);
}

int
connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_connect(fd, addr, addrlen);
	_leave_cancel(self);
	return (rv);
}

#if 0
creat()				/* built on open() */
#endif

int
fcntl(int fd, int cmd, ...)
{
	va_list ap;
	int rv;

	va_start(ap, cmd);
	switch (cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
	case F_SETFD:
	case F_SETFL:
	case F_SETOWN:
		rv = _thread_sys_fcntl(fd, cmd, va_arg(ap, int));
		break;
	case F_GETFD:
	case F_GETFL:
	case F_GETOWN:
	case F_ISATTY:
		rv = _thread_sys_fcntl(fd, cmd);
		break;
	case F_GETLK:
	case F_SETLK:
		rv = _thread_sys_fcntl(fd, cmd, va_arg(ap, struct flock *));
		break;
	case F_SETLKW:
	{
		pthread_t self = pthread_self();

		_enter_cancel(self);
		rv = _thread_sys_fcntl(fd, cmd, va_arg(ap, struct flock *));
		_leave_cancel(self);
		break;
	}
	default:	/* should never happen? */
		rv = _thread_sys_fcntl(fd, cmd, va_arg(ap, void *));
		break;
	}
	va_end(ap);
	return (rv);
}

#if 0
fdatasync()			/* built on fsync() */
#endif

int
fsync(int fd)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_fsync(fd);
	_leave_cancel(self);
	return (rv);
}

#if 0
getmsg()			/* don't have: dumb STREAMS stuff */
getpmsg()			/* don't have: dumb STREAMS stuff */
lockf()				/* built on fcntl() */
mq_receive()			/* don't have yet */
mq_send()			/* don't have yet */
mq_timedreceive()		/* don't have yet */
mq_timedsend()			/* don't have yet */
#endif

int
msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv =  _thread_sys_msgrcv(msqid, msgp, msgsz, msgtyp, msgflg);
	_leave_cancel(self);
	return (rv);
}

int
msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv =  _thread_sys_msgsnd(msqid, msgp, msgsz, msgflg);
	_leave_cancel(self);
	return (rv);
}

int
msync(void *addr, size_t len, int flags)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv =  _thread_sys_msync(addr, len, flags);
	_leave_cancel(self);
	return (rv);
}

int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_nanosleep(rqtp, rmtp);
	_leave_cancel(self);
	return (rv);
}

int
open(const char *path, int flags, ...)
{
	pthread_t self = pthread_self();

	va_list ap;
	mode_t mode = 0;
	int rv;

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	_enter_cancel(self);
	rv = _thread_sys_open(path, flags, mode);
	_leave_cancel(self);
	return (rv);
}


int
openat(int fd, const char *path, int flags, ...)
{
	pthread_t self = pthread_self();

	va_list ap;
	mode_t mode = 0;
	int rv;

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	_enter_cancel(self);
	rv = _thread_sys_openat(fd, path, flags, mode);
	_leave_cancel(self);
	return (rv);
}

#if 0
pause()				/* built on sigsuspend() */
#endif

int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_poll(fds, nfds, timeout);
	_leave_cancel(self);
	return (rv);
}

int
ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout,
    const sigset_t *sigmask)
{
	pthread_t self = pthread_self();
	sigset_t set;
	int rv;

	if (sigmask != NULL && sigismember(sigmask, SIGTHR)) {
		set = *sigmask;
		sigdelset(&set, SIGTHR);
		sigmask = &set;
	}

	_enter_cancel(self);
	rv = _thread_sys_ppoll(fds, nfds, timeout, sigmask);
	_leave_cancel(self);
	return (rv);
}

ssize_t
pread(int fd, void *buf, size_t nbytes, off_t offset)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_pread(fd, buf, nbytes, offset);
	_leave_cancel(self);
	return (rv);
}

ssize_t
preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_preadv(fd, iov, iovcnt, offset);
	_leave_cancel(self);
	return (rv);
}

int
pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    const struct timespec *timeout, const sigset_t *sigmask)
{
	pthread_t self = pthread_self();
	sigset_t set;
	int rv;

	if (sigmask != NULL && sigismember(sigmask, SIGTHR)) {
		set = *sigmask;
		sigdelset(&set, SIGTHR);
		sigmask = &set;
	}

	_enter_cancel(self);
	rv = _thread_sys_pselect(nfds, readfds, writefds, exceptfds, timeout,
	    sigmask);
	_leave_cancel(self);
	return (rv);
}

#if 0
putmsg()			/* don't have: dumb STREAMS stuff */
putpmsg()			/* don't have: dumb STREAMS stuff */
#endif

ssize_t
pwrite(int fd, const void *buf, size_t nbytes, off_t offset)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_pwrite(fd, buf, nbytes, offset);
	_leave_cancel(self);
	return (rv);
}

ssize_t
pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_pwritev(fd, iov, iovcnt, offset);
	_leave_cancel(self);
	return (rv);
}

ssize_t
read(int fd, void *buf, size_t nbytes)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_read(fd, buf, nbytes);
	_leave_cancel(self);
	return (rv);
}

ssize_t
readv(int fd, const struct iovec *iov, int iovcnt)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_readv(fd, iov, iovcnt);
	_leave_cancel(self);
	return (rv);
}

#if 0
recv()				/* built on recvfrom() */
#endif

ssize_t
recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *addr,
    socklen_t *addrlen)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_recvfrom(fd, buf, len, flags, addr, addrlen);
	_leave_cancel(self);
	return (rv);
}

ssize_t
recvmsg(int fd, struct msghdr *msg, int flags)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_recvmsg(fd, msg, flags);
	_leave_cancel(self);
	return (rv);
}

int
select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    struct timeval *timeout)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = _thread_sys_select(nfds, readfds, writefds, exceptfds, timeout);
	_leave_cancel(self);
	return (rv);
}

#if 0
sem_timedwait()			/* in rthread_sem.c */
sem_wait()			/* in rthread_sem.c */
send()				/* built on sendto() */
#endif

ssize_t
sendmsg(int fd, const struct msghdr *msg, int flags)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_sendmsg(fd, msg, flags);
	_leave_cancel(self);
	return (rv);
}

ssize_t
sendto(int fd, const void *msg, size_t len, int flags,
    const struct sockaddr *to, socklen_t tolen)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_sendto(fd, msg, len, flags, to, tolen);
	_leave_cancel(self);
	return (rv);
}


int
sigsuspend(const sigset_t *sigmask)
{
	pthread_t self = pthread_self();
	sigset_t set = *sigmask;
	int rv;

	sigdelset(&set, SIGTHR);
	_enter_cancel(self);
	rv = _thread_sys_sigsuspend(&set);
	_leave_cancel(self);
	return (rv);
}

#if 0
sigtimedwait()			/* don't have yet */
sigwait()			/* in rthread_sig.c */
sigwaitinfo()			/* don't have yet */
sleep()				/* built on nanosleep() */
system()			/* built on wait4()? XXX */
#endif

int
tcdrain(int fd)
{
	pthread_t self = pthread_self();
	int rv;

	_enter_cancel(self);
	rv = ioctl(fd, TIOCDRAIN, 0);
	_leave_cancel(self);
	return (rv);
}

#if 0
wait()				/* built on wait4() */
waitid()			/* don't have yet */
waitpid()			/* built on wait4() */
#endif

pid_t
wait4(pid_t wpid, int *status, int options, struct rusage *rusage)
{
	pthread_t self = pthread_self();
	pid_t rv;

	_enter_cancel(self);
	rv = _thread_sys_wait4(wpid, status, options, rusage);
	_leave_cancel(self);
	return (rv);
}

ssize_t
write(int fd, const void *buf, size_t nbytes)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_write(fd, buf, nbytes);
	_leave_cancel(self);
	return (rv);
}

ssize_t
writev(int fd, const struct iovec *iov, int iovcnt)
{
	pthread_t self = pthread_self();
	ssize_t rv;

	_enter_cancel(self);
	rv = _thread_sys_writev(fd, iov, iovcnt);
	_leave_cancel(self);
	return (rv);
}
