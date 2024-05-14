/*
 * Generated from gen_syscall_emulator.pl
 */
#include <sys/syscall.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/futex.h>
#include <sys/ioctl.h>
#include <sys/ktrace.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/poll.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <tib.h>
#include <time.h>
#include <unistd.h>
#include "syscall_emulator.h"

long
syscall_emulator(int syscall, ...)
{
	long ret = 0;
	va_list args;
	va_start(args, syscall);

	switch(syscall) {
	/* Indirect syscalls not supported
	 *case SYS_syscall:
	 *	ret = syscall(int, ...);
	 *	break;
	 */
	case SYS_exit:
		exit(va_arg(args, int)); // rval
		break;
	case SYS_fork:
		ret = fork();
		break;
	case SYS_read: {
		int fd = (int)va_arg(args, long);
		void * buf = (void *)va_arg(args, long);
		size_t nbyte = (size_t)va_arg(args, long);
		ret = read(fd, buf, nbyte);
		break;
	}
	case SYS_write: {
		int fd = (int)va_arg(args, long);
		const void * buf = (const void *)va_arg(args, long);
		size_t nbyte = (size_t)va_arg(args, long);
		ret = write(fd, buf, nbyte);
		break;
	}
	case SYS_open: {
		const char * path = (const char *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = open(path, flags, mode);
		break;
	}
	case SYS_close:
		ret = close(va_arg(args, int)); // fd
		break;
	case SYS_getentropy: {
		void * buf = (void *)va_arg(args, long);
		size_t nbyte = (size_t)va_arg(args, long);
		ret = getentropy(buf, nbyte);
		break;
	}
	/* No signature found in headers
	 *case SYS___tfork: {
	 *	const struct __tfork * param = (const struct __tfork *)va_arg(args, long);
	 *	size_t psize = (size_t)va_arg(args, long);
	 *	ret = __tfork(param, psize);
	 *	break;
	 *}
	 */
	case SYS_link: {
		const char * path = (const char *)va_arg(args, long);
		const char * _link = (const char *)va_arg(args, long);
		ret = link(path, _link);
		break;
	}
	case SYS_unlink:
		ret = unlink(va_arg(args, const char *)); // path
		break;
	case SYS_wait4: {
		pid_t pid = (pid_t)va_arg(args, long);
		int * status = (int *)va_arg(args, long);
		int options = (int)va_arg(args, long);
		struct rusage * rusage = (struct rusage *)va_arg(args, long);
		ret = wait4(pid, status, options, rusage);
		break;
	}
	case SYS_chdir:
		ret = chdir(va_arg(args, const char *)); // path
		break;
	case SYS_fchdir:
		ret = fchdir(va_arg(args, int)); // fd
		break;
	case SYS_mknod: {
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		dev_t dev = (dev_t)va_arg(args, long);
		ret = mknod(path, mode, dev);
		break;
	}
	case SYS_chmod: {
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = chmod(path, mode);
		break;
	}
	case SYS_chown: {
		const char * path = (const char *)va_arg(args, long);
		uid_t uid = (uid_t)va_arg(args, long);
		gid_t gid = (gid_t)va_arg(args, long);
		ret = chown(path, uid, gid);
		break;
	}
	/* No signature found in headers
	 *case SYS_break:
	 *	ret = break(char *);
	 *	break;
	 */
	case SYS_getdtablecount:
		ret = getdtablecount();
		break;
	case SYS_getrusage: {
		int who = (int)va_arg(args, long);
		struct rusage * rusage = (struct rusage *)va_arg(args, long);
		ret = getrusage(who, rusage);
		break;
	}
	case SYS_getpid:
		ret = getpid();
		break;
	case SYS_mount: {
		const char * type = (const char *)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		void * data = (void *)va_arg(args, long);
		ret = mount(type, path, flags, data);
		break;
	}
	case SYS_unmount: {
		const char * path = (const char *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = unmount(path, flags);
		break;
	}
	case SYS_setuid:
		ret = setuid(va_arg(args, uid_t)); // uid
		break;
	case SYS_getuid:
		ret = getuid();
		break;
	case SYS_geteuid:
		ret = geteuid();
		break;
	case SYS_ptrace: {
		int req = (int)va_arg(args, long);
		pid_t pid = (pid_t)va_arg(args, long);
		caddr_t addr = (caddr_t)va_arg(args, long);
		int data = (int)va_arg(args, long);
		ret = ptrace(req, pid, addr, data);
		break;
	}
	case SYS_recvmsg: {
		int s = (int)va_arg(args, long);
		struct msghdr * msg = (struct msghdr *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = recvmsg(s, msg, flags);
		break;
	}
	case SYS_sendmsg: {
		int s = (int)va_arg(args, long);
		const struct msghdr * msg = (const struct msghdr *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = sendmsg(s, msg, flags);
		break;
	}
	case SYS_recvfrom: {
		int s = (int)va_arg(args, long);
		void * buf = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		struct sockaddr * from = (struct sockaddr *)va_arg(args, long);
		socklen_t * fromlenaddr = (socklen_t *)va_arg(args, long);
		ret = recvfrom(s, buf, len, flags, from, fromlenaddr);
		break;
	}
	case SYS_accept: {
		int s = (int)va_arg(args, long);
		struct sockaddr * name = (struct sockaddr *)va_arg(args, long);
		socklen_t * anamelen = (socklen_t *)va_arg(args, long);
		ret = accept(s, name, anamelen);
		break;
	}
	case SYS_getpeername: {
		int fdes = (int)va_arg(args, long);
		struct sockaddr * asa = (struct sockaddr *)va_arg(args, long);
		socklen_t * alen = (socklen_t *)va_arg(args, long);
		ret = getpeername(fdes, asa, alen);
		break;
	}
	case SYS_getsockname: {
		int fdes = (int)va_arg(args, long);
		struct sockaddr * asa = (struct sockaddr *)va_arg(args, long);
		socklen_t * alen = (socklen_t *)va_arg(args, long);
		ret = getsockname(fdes, asa, alen);
		break;
	}
	case SYS_access: {
		const char * path = (const char *)va_arg(args, long);
		int amode = (int)va_arg(args, long);
		ret = access(path, amode);
		break;
	}
	case SYS_chflags: {
		const char * path = (const char *)va_arg(args, long);
		u_int flags = (u_int)va_arg(args, long);
		ret = chflags(path, flags);
		break;
	}
	case SYS_fchflags: {
		int fd = (int)va_arg(args, long);
		u_int flags = (u_int)va_arg(args, long);
		ret = fchflags(fd, flags);
		break;
	}
	case SYS_sync:
		sync();
		break;
	/* No signature found in headers
	 *case SYS_msyscall: {
	 *	void * addr = (void *)va_arg(args, long);
	 *	size_t len = (size_t)va_arg(args, long);
	 *	ret = msyscall(addr, len);
	 *	break;
	 *}
	 */
	case SYS_stat: {
		const char * path = (const char *)va_arg(args, long);
		struct stat * ub = (struct stat *)va_arg(args, long);
		ret = stat(path, ub);
		break;
	}
	case SYS_getppid:
		ret = getppid();
		break;
	case SYS_lstat: {
		const char * path = (const char *)va_arg(args, long);
		struct stat * ub = (struct stat *)va_arg(args, long);
		ret = lstat(path, ub);
		break;
	}
	case SYS_dup:
		ret = dup(va_arg(args, int)); // fd
		break;
	case SYS_fstatat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		struct stat * buf = (struct stat *)va_arg(args, long);
		int flag = (int)va_arg(args, long);
		ret = fstatat(fd, path, buf, flag);
		break;
	}
	case SYS_getegid:
		ret = getegid();
		break;
	case SYS_profil: {
		caddr_t samples = (caddr_t)va_arg(args, long);
		size_t size = (size_t)va_arg(args, long);
		u_long offset = (u_long)va_arg(args, long);
		u_int scale = (u_int)va_arg(args, long);
		ret = profil(samples, size, offset, scale);
		break;
	}
	case SYS_ktrace: {
		const char * fname = (const char *)va_arg(args, long);
		int ops = (int)va_arg(args, long);
		int facs = (int)va_arg(args, long);
		pid_t pid = (pid_t)va_arg(args, long);
		ret = ktrace(fname, ops, facs, pid);
		break;
	}
	case SYS_sigaction: {
		int signum = (int)va_arg(args, long);
		const struct sigaction * nsa = (const struct sigaction *)va_arg(args, long);
		struct sigaction * osa = (struct sigaction *)va_arg(args, long);
		ret = sigaction(signum, nsa, osa);
		break;
	}
	case SYS_getgid:
		ret = getgid();
		break;
	/* Mismatched func: int sigprocmask(int, const sigset_t *, sigset_t *); <signal.h>
	 *                  int sigprocmask(int, sigset_t); <sys/syscall.h>
	 *case SYS_sigprocmask: {
	 *	int how = (int)va_arg(args, long);
	 *	sigset_t mask = (sigset_t)va_arg(args, long);
	 *	ret = sigprocmask(how, mask);
	 *	break;
	 *}
	 */
	case SYS_mmap: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int prot = (int)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		int fd = (int)va_arg(args, long);
		off_t pos = (off_t)va_arg(args, long);
		ret = (long)mmap(addr, len, prot, flags, fd, pos);
		break;
	}
	case SYS_setlogin:
		ret = setlogin(va_arg(args, const char *)); // namebuf
		break;
	case SYS_acct:
		ret = acct(va_arg(args, const char *)); // path
		break;
	/* Mismatched func: int sigpending(sigset_t *); <signal.h>
	 *                  int sigpending(void); <sys/syscall.h>
	 *case SYS_sigpending:
	 *	ret = sigpending();
	 *	break;
	 */
	case SYS_fstat: {
		int fd = (int)va_arg(args, long);
		struct stat * sb = (struct stat *)va_arg(args, long);
		ret = fstat(fd, sb);
		break;
	}
	case SYS_ioctl: {
		int fd = (int)va_arg(args, long);
		u_long com = (u_long)va_arg(args, long);
		void * data = (void *)va_arg(args, long);
		ret = ioctl(fd, com, data);
		break;
	}
	case SYS_reboot:
		ret = reboot(va_arg(args, int)); // opt
		break;
	case SYS_revoke:
		ret = revoke(va_arg(args, const char *)); // path
		break;
	case SYS_symlink: {
		const char * path = (const char *)va_arg(args, long);
		const char * link = (const char *)va_arg(args, long);
		ret = symlink(path, link);
		break;
	}
	case SYS_readlink: {
		const char * path = (const char *)va_arg(args, long);
		char * buf = (char *)va_arg(args, long);
		size_t count = (size_t)va_arg(args, long);
		ret = readlink(path, buf, count);
		break;
	}
	case SYS_execve: {
		const char * path = (const char *)va_arg(args, long);
		char *const * argp = (char *const *)va_arg(args, long);
		char *const * envp = (char *const *)va_arg(args, long);
		ret = execve(path, argp, envp);
		break;
	}
	case SYS_umask:
		ret = umask(va_arg(args, mode_t)); // newmask
		break;
	case SYS_chroot:
		ret = chroot(va_arg(args, const char *)); // path
		break;
	case SYS_getfsstat: {
		struct statfs * buf = (struct statfs *)va_arg(args, long);
		size_t bufsize = (size_t)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = getfsstat(buf, bufsize, flags);
		break;
	}
	case SYS_statfs: {
		const char * path = (const char *)va_arg(args, long);
		struct statfs * buf = (struct statfs *)va_arg(args, long);
		ret = statfs(path, buf);
		break;
	}
	case SYS_fstatfs: {
		int fd = (int)va_arg(args, long);
		struct statfs * buf = (struct statfs *)va_arg(args, long);
		ret = fstatfs(fd, buf);
		break;
	}
	case SYS_fhstatfs: {
		const fhandle_t * fhp = (const fhandle_t *)va_arg(args, long);
		struct statfs * buf = (struct statfs *)va_arg(args, long);
		ret = fhstatfs(fhp, buf);
		break;
	}
	case SYS_vfork:
		ret = vfork();
		break;
	case SYS_gettimeofday: {
		struct timeval * tp = (struct timeval *)va_arg(args, long);
		struct timezone * tzp = (struct timezone *)va_arg(args, long);
		ret = gettimeofday(tp, tzp);
		break;
	}
	case SYS_settimeofday: {
		const struct timeval * tv = (const struct timeval *)va_arg(args, long);
		const struct timezone * tzp = (const struct timezone *)va_arg(args, long);
		ret = settimeofday(tv, tzp);
		break;
	}
	case SYS_setitimer: {
		int which = (int)va_arg(args, long);
		const struct itimerval * itv = (const struct itimerval *)va_arg(args, long);
		struct itimerval * oitv = (struct itimerval *)va_arg(args, long);
		ret = setitimer(which, itv, oitv);
		break;
	}
	case SYS_getitimer: {
		int which = (int)va_arg(args, long);
		struct itimerval * itv = (struct itimerval *)va_arg(args, long);
		ret = getitimer(which, itv);
		break;
	}
	case SYS_select: {
		int nd = (int)va_arg(args, long);
		fd_set * in = (fd_set *)va_arg(args, long);
		fd_set * ou = (fd_set *)va_arg(args, long);
		fd_set * ex = (fd_set *)va_arg(args, long);
		struct timeval * tv = (struct timeval *)va_arg(args, long);
		ret = select(nd, in, ou, ex, tv);
		break;
	}
	case SYS_kevent: {
		int fd = (int)va_arg(args, long);
		const struct kevent * changelist = (const struct kevent *)va_arg(args, long);
		int nchanges = (int)va_arg(args, long);
		struct kevent * eventlist = (struct kevent *)va_arg(args, long);
		int nevents = (int)va_arg(args, long);
		const struct timespec * timeout = (const struct timespec *)va_arg(args, long);
		ret = kevent(fd, changelist, nchanges, eventlist, nevents, timeout);
		break;
	}
	case SYS_munmap: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		ret = munmap(addr, len);
		break;
	}
	case SYS_mprotect: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int prot = (int)va_arg(args, long);
		ret = mprotect(addr, len, prot);
		break;
	}
	case SYS_madvise: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int behav = (int)va_arg(args, long);
		ret = madvise(addr, len, behav);
		break;
	}
	case SYS_utimes: {
		const char * path = (const char *)va_arg(args, long);
		const struct timeval * tptr = (const struct timeval *)va_arg(args, long);
		ret = utimes(path, tptr);
		break;
	}
	case SYS_futimes: {
		int fd = (int)va_arg(args, long);
		const struct timeval * tptr = (const struct timeval *)va_arg(args, long);
		ret = futimes(fd, tptr);
		break;
	}
	case SYS_mquery: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int prot = (int)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		int fd = (int)va_arg(args, long);
		off_t pos = (off_t)va_arg(args, long);
		ret = (long)mquery(addr, len, prot, flags, fd, pos);
		break;
	}
	case SYS_getgroups: {
		int gidsetsize = (int)va_arg(args, long);
		gid_t * gidset = (gid_t *)va_arg(args, long);
		ret = getgroups(gidsetsize, gidset);
		break;
	}
	case SYS_setgroups: {
		int gidsetsize = (int)va_arg(args, long);
		const gid_t * gidset = (const gid_t *)va_arg(args, long);
		ret = setgroups(gidsetsize, gidset);
		break;
	}
	case SYS_getpgrp:
		ret = getpgrp();
		break;
	case SYS_setpgid: {
		pid_t pid = (pid_t)va_arg(args, long);
		pid_t pgid = (pid_t)va_arg(args, long);
		ret = setpgid(pid, pgid);
		break;
	}
	case SYS_futex: {
		uint32_t * f = (uint32_t *)va_arg(args, long);
		int op = (int)va_arg(args, long);
		int val = (int)va_arg(args, long);
		const struct timespec * timeout = (const struct timespec *)va_arg(args, long);
		uint32_t * g = (uint32_t *)va_arg(args, long);
		ret = futex(f, op, val, timeout, g);
		break;
	}
	case SYS_utimensat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		const struct timespec * times = (const struct timespec *)va_arg(args, long);
		int flag = (int)va_arg(args, long);
		ret = utimensat(fd, path, times, flag);
		break;
	}
	case SYS_futimens: {
		int fd = (int)va_arg(args, long);
		const struct timespec * times = (const struct timespec *)va_arg(args, long);
		ret = futimens(fd, times);
		break;
	}
	/* No signature found in headers
	 *case SYS_kbind: {
	 *	const struct __kbind * param = (const struct __kbind *)va_arg(args, long);
	 *	size_t psize = (size_t)va_arg(args, long);
	 *	int64_t proc_cookie = (int64_t)va_arg(args, long);
	 *	ret = kbind(param, psize, proc_cookie);
	 *	break;
	 *}
	 */
	case SYS_clock_gettime: {
		clockid_t clock_id = (clockid_t)va_arg(args, long);
		struct timespec * tp = (struct timespec *)va_arg(args, long);
		ret = clock_gettime(clock_id, tp);
		break;
	}
	case SYS_clock_settime: {
		clockid_t clock_id = (clockid_t)va_arg(args, long);
		const struct timespec * tp = (const struct timespec *)va_arg(args, long);
		ret = clock_settime(clock_id, tp);
		break;
	}
	case SYS_clock_getres: {
		clockid_t clock_id = (clockid_t)va_arg(args, long);
		struct timespec * tp = (struct timespec *)va_arg(args, long);
		ret = clock_getres(clock_id, tp);
		break;
	}
	case SYS_dup2: {
		int from = (int)va_arg(args, long);
		int to = (int)va_arg(args, long);
		ret = dup2(from, to);
		break;
	}
	case SYS_nanosleep: {
		const struct timespec * rqtp = (const struct timespec *)va_arg(args, long);
		struct timespec * rmtp = (struct timespec *)va_arg(args, long);
		ret = nanosleep(rqtp, rmtp);
		break;
	}
	case SYS_fcntl: {
		int fd = (int)va_arg(args, long);
		int cmd = (int)va_arg(args, long);
		void * arg = (void *)va_arg(args, long);
		ret = fcntl(fd, cmd, arg);
		break;
	}
	case SYS_accept4: {
		int s = (int)va_arg(args, long);
		struct sockaddr * name = (struct sockaddr *)va_arg(args, long);
		socklen_t * anamelen = (socklen_t *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = accept4(s, name, anamelen, flags);
		break;
	}
	/* No signature found in headers
	 *case SYS___thrsleep: {
	 *	const volatile void * ident = (const volatile void *)va_arg(args, long);
	 *	clockid_t clock_id = (clockid_t)va_arg(args, long);
	 *	const struct timespec * tp = (const struct timespec *)va_arg(args, long);
	 *	void * lock = (void *)va_arg(args, long);
	 *	const int * abort = (const int *)va_arg(args, long);
	 *	ret = __thrsleep(ident, clock_id, tp, lock, abort);
	 *	break;
	 *}
	 */
	case SYS_fsync:
		ret = fsync(va_arg(args, int)); // fd
		break;
	case SYS_setpriority: {
		int which = (int)va_arg(args, long);
		id_t who = (id_t)va_arg(args, long);
		int prio = (int)va_arg(args, long);
		ret = setpriority(which, who, prio);
		break;
	}
	case SYS_socket: {
		int domain = (int)va_arg(args, long);
		int type = (int)va_arg(args, long);
		int protocol = (int)va_arg(args, long);
		ret = socket(domain, type, protocol);
		break;
	}
	case SYS_connect: {
		int s = (int)va_arg(args, long);
		const struct sockaddr * name = (const struct sockaddr *)va_arg(args, long);
		socklen_t namelen = (socklen_t)va_arg(args, long);
		ret = connect(s, name, namelen);
		break;
	}
	case SYS_getdents: {
		int fd = (int)va_arg(args, long);
		void * buf = (void *)va_arg(args, long);
		size_t buflen = (size_t)va_arg(args, long);
		ret = getdents(fd, buf, buflen);
		break;
	}
	case SYS_getpriority: {
		int which = (int)va_arg(args, long);
		id_t who = (id_t)va_arg(args, long);
		ret = getpriority(which, who);
		break;
	}
	case SYS_pipe2: {
		int * fdp = (int *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = pipe2(fdp, flags);
		break;
	}
	case SYS_dup3: {
		int from = (int)va_arg(args, long);
		int to = (int)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = dup3(from, to, flags);
		break;
	}
	/* No signature found in headers
	 *case SYS_sigreturn:
	 *	ret = sigreturn(va_arg(args, struct sigcontext *)); // sigcntxp
	 *	break;
	 */
	case SYS_bind: {
		int s = (int)va_arg(args, long);
		const struct sockaddr * name = (const struct sockaddr *)va_arg(args, long);
		socklen_t namelen = (socklen_t)va_arg(args, long);
		ret = bind(s, name, namelen);
		break;
	}
	case SYS_setsockopt: {
		int s = (int)va_arg(args, long);
		int level = (int)va_arg(args, long);
		int name = (int)va_arg(args, long);
		const void * val = (const void *)va_arg(args, long);
		socklen_t valsize = (socklen_t)va_arg(args, long);
		ret = setsockopt(s, level, name, val, valsize);
		break;
	}
	case SYS_listen: {
		int s = (int)va_arg(args, long);
		int backlog = (int)va_arg(args, long);
		ret = listen(s, backlog);
		break;
	}
	case SYS_chflagsat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		u_int flags = (u_int)va_arg(args, long);
		int atflags = (int)va_arg(args, long);
		ret = chflagsat(fd, path, flags, atflags);
		break;
	}
	case SYS_pledge: {
		const char * promises = (const char *)va_arg(args, long);
		const char * execpromises = (const char *)va_arg(args, long);
		ret = pledge(promises, execpromises);
		break;
	}
	case SYS_ppoll: {
		struct pollfd * fds = (struct pollfd *)va_arg(args, long);
		u_int nfds = (u_int)va_arg(args, long);
		const struct timespec * ts = (const struct timespec *)va_arg(args, long);
		const sigset_t * mask = (const sigset_t *)va_arg(args, long);
		ret = ppoll(fds, nfds, ts, mask);
		break;
	}
	case SYS_pselect: {
		int nd = (int)va_arg(args, long);
		fd_set * in = (fd_set *)va_arg(args, long);
		fd_set * ou = (fd_set *)va_arg(args, long);
		fd_set * ex = (fd_set *)va_arg(args, long);
		const struct timespec * ts = (const struct timespec *)va_arg(args, long);
		const sigset_t * mask = (const sigset_t *)va_arg(args, long);
		ret = pselect(nd, in, ou, ex, ts, mask);
		break;
	}
	/* Mismatched func: int sigsuspend(const sigset_t *); <signal.h>
	 *                  int sigsuspend(int); <sys/syscall.h>
	 *case SYS_sigsuspend:
	 *	ret = sigsuspend(va_arg(args, int)); // mask
	 *	break;
	 */
	case SYS_sendsyslog: {
		const char * buf = (const char *)va_arg(args, long);
		size_t nbyte = (size_t)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = sendsyslog(buf, nbyte, flags);
		break;
	}
	case SYS_unveil: {
		const char * path = (const char *)va_arg(args, long);
		const char * permissions = (const char *)va_arg(args, long);
		ret = unveil(path, permissions);
		break;
	}
	/* No signature found in headers
	 *case SYS___realpath: {
	 *	const char * pathname = (const char *)va_arg(args, long);
	 *	char * resolved = (char *)va_arg(args, long);
	 *	ret = __realpath(pathname, resolved);
	 *	break;
	 *}
	 */
	case SYS_recvmmsg: {
		int s = (int)va_arg(args, long);
		struct mmsghdr * mmsg = (struct mmsghdr *)va_arg(args, long);
		unsigned int vlen = (unsigned int)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		struct timespec * timeout = (struct timespec *)va_arg(args, long);
		ret = recvmmsg(s, mmsg, vlen, flags, timeout);
		break;
	}
	case SYS_sendmmsg: {
		int s = (int)va_arg(args, long);
		struct mmsghdr * mmsg = (struct mmsghdr *)va_arg(args, long);
		unsigned int vlen = (unsigned int)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = sendmmsg(s, mmsg, vlen, flags);
		break;
	}
	case SYS_getsockopt: {
		int s = (int)va_arg(args, long);
		int level = (int)va_arg(args, long);
		int name = (int)va_arg(args, long);
		void * val = (void *)va_arg(args, long);
		socklen_t * avalsize = (socklen_t *)va_arg(args, long);
		ret = getsockopt(s, level, name, val, avalsize);
		break;
	}
	case SYS_thrkill: {
		pid_t tid = (pid_t)va_arg(args, long);
		int signum = (int)va_arg(args, long);
		void * tcb = (void *)va_arg(args, long);
		ret = thrkill(tid, signum, tcb);
		break;
	}
	case SYS_readv: {
		int fd = (int)va_arg(args, long);
		const struct iovec * iovp = (const struct iovec *)va_arg(args, long);
		int iovcnt = (int)va_arg(args, long);
		ret = readv(fd, iovp, iovcnt);
		break;
	}
	case SYS_writev: {
		int fd = (int)va_arg(args, long);
		const struct iovec * iovp = (const struct iovec *)va_arg(args, long);
		int iovcnt = (int)va_arg(args, long);
		ret = writev(fd, iovp, iovcnt);
		break;
	}
	case SYS_kill: {
		int pid = (int)va_arg(args, long);
		int signum = (int)va_arg(args, long);
		ret = kill(pid, signum);
		break;
	}
	case SYS_fchown: {
		int fd = (int)va_arg(args, long);
		uid_t uid = (uid_t)va_arg(args, long);
		gid_t gid = (gid_t)va_arg(args, long);
		ret = fchown(fd, uid, gid);
		break;
	}
	case SYS_fchmod: {
		int fd = (int)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = fchmod(fd, mode);
		break;
	}
	case SYS_setreuid: {
		uid_t ruid = (uid_t)va_arg(args, long);
		uid_t euid = (uid_t)va_arg(args, long);
		ret = setreuid(ruid, euid);
		break;
	}
	case SYS_setregid: {
		gid_t rgid = (gid_t)va_arg(args, long);
		gid_t egid = (gid_t)va_arg(args, long);
		ret = setregid(rgid, egid);
		break;
	}
	case SYS_rename: {
		const char * from = (const char *)va_arg(args, long);
		const char * to = (const char *)va_arg(args, long);
		ret = rename(from, to);
		break;
	}
	case SYS_flock: {
		int fd = (int)va_arg(args, long);
		int how = (int)va_arg(args, long);
		ret = flock(fd, how);
		break;
	}
	case SYS_mkfifo: {
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = mkfifo(path, mode);
		break;
	}
	case SYS_sendto: {
		int s = (int)va_arg(args, long);
		const void * buf = (const void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		const struct sockaddr * to = (const struct sockaddr *)va_arg(args, long);
		socklen_t tolen = (socklen_t)va_arg(args, long);
		ret = sendto(s, buf, len, flags, to, tolen);
		break;
	}
	case SYS_shutdown: {
		int s = (int)va_arg(args, long);
		int how = (int)va_arg(args, long);
		ret = shutdown(s, how);
		break;
	}
	case SYS_socketpair: {
		int domain = (int)va_arg(args, long);
		int type = (int)va_arg(args, long);
		int protocol = (int)va_arg(args, long);
		int * rsv = (int *)va_arg(args, long);
		ret = socketpair(domain, type, protocol, rsv);
		break;
	}
	case SYS_mkdir: {
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = mkdir(path, mode);
		break;
	}
	case SYS_rmdir:
		ret = rmdir(va_arg(args, const char *)); // path
		break;
	case SYS_adjtime: {
		const struct timeval * delta = (const struct timeval *)va_arg(args, long);
		struct timeval * olddelta = (struct timeval *)va_arg(args, long);
		ret = adjtime(delta, olddelta);
		break;
	}
	/* Mismatched func: int getlogin_r(char *, size_t); <unistd.h>
	 *                  int getlogin_r(char *, u_int); <sys/syscall.h>
	 *case SYS_getlogin_r: {
	 *	char * namebuf = (char *)va_arg(args, long);
	 *	u_int namelen = (u_int)va_arg(args, long);
	 *	ret = getlogin_r(namebuf, namelen);
	 *	break;
	 *}
	 */
	case SYS_getthrname: {
		pid_t tid = (pid_t)va_arg(args, long);
		char * name = (char *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		ret = getthrname(tid, name, len);
		break;
	}
	case SYS_setthrname: {
		pid_t tid = (pid_t)va_arg(args, long);
		const char * name = (const char *)va_arg(args, long);
		ret = setthrname(tid, name);
		break;
	}
	/* No signature found in headers
	 *case SYS_pinsyscall: {
	 *	int syscall = (int)va_arg(args, long);
	 *	void * addr = (void *)va_arg(args, long);
	 *	size_t len = (size_t)va_arg(args, long);
	 *	ret = pinsyscall(syscall, addr, len);
	 *	break;
	 *}
	 */
	case SYS_setsid:
		ret = setsid();
		break;
	case SYS_quotactl: {
		const char * path = (const char *)va_arg(args, long);
		int cmd = (int)va_arg(args, long);
		int uid = (int)va_arg(args, long);
		char * arg = (char *)va_arg(args, long);
		ret = quotactl(path, cmd, uid, arg);
		break;
	}
	/* No signature found in headers
	 *case SYS_ypconnect:
	 *	ret = ypconnect(va_arg(args, int)); // type
	 *	break;
	 */
	case SYS_nfssvc: {
		int flag = (int)va_arg(args, long);
		void * argp = (void *)va_arg(args, long);
		ret = nfssvc(flag, argp);
		break;
	}
	case SYS_mimmutable: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		ret = mimmutable(addr, len);
		break;
	}
	case SYS_waitid: {
		int idtype = (int)va_arg(args, long);
		id_t id = (id_t)va_arg(args, long);
		siginfo_t * info = (siginfo_t *)va_arg(args, long);
		int options = (int)va_arg(args, long);
		ret = waitid(idtype, id, info, options);
		break;
	}
	case SYS_getfh: {
		const char * fname = (const char *)va_arg(args, long);
		fhandle_t * fhp = (fhandle_t *)va_arg(args, long);
		ret = getfh(fname, fhp);
		break;
	}
	/* No signature found in headers
	 *case SYS___tmpfd:
	 *	ret = __tmpfd(va_arg(args, int)); // flags
	 *	break;
	 */
	/* No signature found in headers
	 *case SYS_sysarch: {
	 *	int op = (int)va_arg(args, long);
	 *	void * parms = (void *)va_arg(args, long);
	 *	ret = sysarch(op, parms);
	 *	break;
	 *}
	 */
	case SYS_lseek: {
		int fd = (int)va_arg(args, long);
		off_t offset = (off_t)va_arg(args, long);
		int whence = (int)va_arg(args, long);
		ret = lseek(fd, offset, whence);
		break;
	}
	case SYS_truncate: {
		const char * path = (const char *)va_arg(args, long);
		off_t length = (off_t)va_arg(args, long);
		ret = truncate(path, length);
		break;
	}
	case SYS_ftruncate: {
		int fd = (int)va_arg(args, long);
		off_t length = (off_t)va_arg(args, long);
		ret = ftruncate(fd, length);
		break;
	}
	case SYS_pread: {
		int fd = (int)va_arg(args, long);
		void * buf = (void *)va_arg(args, long);
		size_t nbyte = (size_t)va_arg(args, long);
		off_t offset = (off_t)va_arg(args, long);
		ret = pread(fd, buf, nbyte, offset);
		break;
	}
	case SYS_pwrite: {
		int fd = (int)va_arg(args, long);
		const void * buf = (const void *)va_arg(args, long);
		size_t nbyte = (size_t)va_arg(args, long);
		off_t offset = (off_t)va_arg(args, long);
		ret = pwrite(fd, buf, nbyte, offset);
		break;
	}
	case SYS_preadv: {
		int fd = (int)va_arg(args, long);
		const struct iovec * iovp = (const struct iovec *)va_arg(args, long);
		int iovcnt = (int)va_arg(args, long);
		off_t offset = (off_t)va_arg(args, long);
		ret = preadv(fd, iovp, iovcnt, offset);
		break;
	}
	case SYS_pwritev: {
		int fd = (int)va_arg(args, long);
		const struct iovec * iovp = (const struct iovec *)va_arg(args, long);
		int iovcnt = (int)va_arg(args, long);
		off_t offset = (off_t)va_arg(args, long);
		ret = pwritev(fd, iovp, iovcnt, offset);
		break;
	}
	case SYS_setgid:
		ret = setgid(va_arg(args, gid_t)); // gid
		break;
	case SYS_setegid:
		ret = setegid(va_arg(args, gid_t)); // egid
		break;
	case SYS_seteuid:
		ret = seteuid(va_arg(args, uid_t)); // euid
		break;
	case SYS_pathconf: {
		const char * path = (const char *)va_arg(args, long);
		int name = (int)va_arg(args, long);
		ret = pathconf(path, name);
		break;
	}
	case SYS_fpathconf: {
		int fd = (int)va_arg(args, long);
		int name = (int)va_arg(args, long);
		ret = fpathconf(fd, name);
		break;
	}
	case SYS_swapctl: {
		int cmd = (int)va_arg(args, long);
		const void * arg = (const void *)va_arg(args, long);
		int misc = (int)va_arg(args, long);
		ret = swapctl(cmd, arg, misc);
		break;
	}
	case SYS_getrlimit: {
		int which = (int)va_arg(args, long);
		struct rlimit * rlp = (struct rlimit *)va_arg(args, long);
		ret = getrlimit(which, rlp);
		break;
	}
	case SYS_setrlimit: {
		int which = (int)va_arg(args, long);
		const struct rlimit * rlp = (const struct rlimit *)va_arg(args, long);
		ret = setrlimit(which, rlp);
		break;
	}
	case SYS_sysctl: {
		const int * name = (const int *)va_arg(args, long);
		u_int namelen = (u_int)va_arg(args, long);
		void * old = (void *)va_arg(args, long);
		size_t * oldlenp = (size_t *)va_arg(args, long);
		void * new = (void *)va_arg(args, long);
		size_t newlen = (size_t)va_arg(args, long);
		ret = sysctl(name, namelen, old, oldlenp, new, newlen);
		break;
	}
	case SYS_mlock: {
		const void * addr = (const void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		ret = mlock(addr, len);
		break;
	}
	case SYS_munlock: {
		const void * addr = (const void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		ret = munlock(addr, len);
		break;
	}
	case SYS_getpgid:
		ret = getpgid(va_arg(args, pid_t)); // pid
		break;
	case SYS_utrace: {
		const char * label = (const char *)va_arg(args, long);
		const void * addr = (const void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		ret = utrace(label, addr, len);
		break;
	}
	case SYS_semget: {
		key_t key = (key_t)va_arg(args, long);
		int nsems = (int)va_arg(args, long);
		int semflg = (int)va_arg(args, long);
		ret = semget(key, nsems, semflg);
		break;
	}
	case SYS_msgget: {
		key_t key = (key_t)va_arg(args, long);
		int msgflg = (int)va_arg(args, long);
		ret = msgget(key, msgflg);
		break;
	}
	case SYS_msgsnd: {
		int msqid = (int)va_arg(args, long);
		const void * msgp = (const void *)va_arg(args, long);
		size_t msgsz = (size_t)va_arg(args, long);
		int msgflg = (int)va_arg(args, long);
		ret = msgsnd(msqid, msgp, msgsz, msgflg);
		break;
	}
	case SYS_msgrcv: {
		int msqid = (int)va_arg(args, long);
		void * msgp = (void *)va_arg(args, long);
		size_t msgsz = (size_t)va_arg(args, long);
		long msgtyp = (long)va_arg(args, long);
		int msgflg = (int)va_arg(args, long);
		ret = msgrcv(msqid, msgp, msgsz, msgtyp, msgflg);
		break;
	}
	case SYS_shmat: {
		int shmid = (int)va_arg(args, long);
		const void * shmaddr = (const void *)va_arg(args, long);
		int shmflg = (int)va_arg(args, long);
		ret = (long)shmat(shmid, shmaddr, shmflg);
		break;
	}
	case SYS_shmdt:
		ret = shmdt(va_arg(args, const void *)); // shmaddr
		break;
	case SYS_minherit: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int inherit = (int)va_arg(args, long);
		ret = minherit(addr, len, inherit);
		break;
	}
	case SYS_poll: {
		struct pollfd * fds = (struct pollfd *)va_arg(args, long);
		u_int nfds = (u_int)va_arg(args, long);
		int timeout = (int)va_arg(args, long);
		ret = poll(fds, nfds, timeout);
		break;
	}
	case SYS_issetugid:
		ret = issetugid();
		break;
	case SYS_lchown: {
		const char * path = (const char *)va_arg(args, long);
		uid_t uid = (uid_t)va_arg(args, long);
		gid_t gid = (gid_t)va_arg(args, long);
		ret = lchown(path, uid, gid);
		break;
	}
	case SYS_getsid:
		ret = getsid(va_arg(args, pid_t)); // pid
		break;
	case SYS_msync: {
		void * addr = (void *)va_arg(args, long);
		size_t len = (size_t)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = msync(addr, len, flags);
		break;
	}
	case SYS_pipe:
		ret = pipe(va_arg(args, int *)); // fdp
		break;
	case SYS_fhopen: {
		const fhandle_t * fhp = (const fhandle_t *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		ret = fhopen(fhp, flags);
		break;
	}
	case SYS_kqueue:
		ret = kqueue();
		break;
	case SYS_mlockall:
		ret = mlockall(va_arg(args, int)); // flags
		break;
	case SYS_munlockall:
		ret = munlockall();
		break;
	case SYS_getresuid: {
		uid_t * ruid = (uid_t *)va_arg(args, long);
		uid_t * euid = (uid_t *)va_arg(args, long);
		uid_t * suid = (uid_t *)va_arg(args, long);
		ret = getresuid(ruid, euid, suid);
		break;
	}
	case SYS_setresuid: {
		uid_t ruid = (uid_t)va_arg(args, long);
		uid_t euid = (uid_t)va_arg(args, long);
		uid_t suid = (uid_t)va_arg(args, long);
		ret = setresuid(ruid, euid, suid);
		break;
	}
	case SYS_getresgid: {
		gid_t * rgid = (gid_t *)va_arg(args, long);
		gid_t * egid = (gid_t *)va_arg(args, long);
		gid_t * sgid = (gid_t *)va_arg(args, long);
		ret = getresgid(rgid, egid, sgid);
		break;
	}
	case SYS_setresgid: {
		gid_t rgid = (gid_t)va_arg(args, long);
		gid_t egid = (gid_t)va_arg(args, long);
		gid_t sgid = (gid_t)va_arg(args, long);
		ret = setresgid(rgid, egid, sgid);
		break;
	}
	case SYS_closefrom:
		ret = closefrom(va_arg(args, int)); // fd
		break;
	case SYS_sigaltstack: {
		const struct sigaltstack * nss = (const struct sigaltstack *)va_arg(args, long);
		struct sigaltstack * oss = (struct sigaltstack *)va_arg(args, long);
		ret = sigaltstack(nss, oss);
		break;
	}
	case SYS_shmget: {
		key_t key = (key_t)va_arg(args, long);
		size_t size = (size_t)va_arg(args, long);
		int shmflg = (int)va_arg(args, long);
		ret = shmget(key, size, shmflg);
		break;
	}
	case SYS_semop: {
		int semid = (int)va_arg(args, long);
		struct sembuf * sops = (struct sembuf *)va_arg(args, long);
		size_t nsops = (size_t)va_arg(args, long);
		ret = semop(semid, sops, nsops);
		break;
	}
	case SYS_fhstat: {
		const fhandle_t * fhp = (const fhandle_t *)va_arg(args, long);
		struct stat * sb = (struct stat *)va_arg(args, long);
		ret = fhstat(fhp, sb);
		break;
	}
	case SYS___semctl: {
		int semid = (int)va_arg(args, long);
		int semnum = (int)va_arg(args, long);
		int cmd = (int)va_arg(args, long);
		union semun * arg = (union semun *)va_arg(args, long);
		ret = __semctl(semid, semnum, cmd, arg);
		break;
	}
	case SYS_shmctl: {
		int shmid = (int)va_arg(args, long);
		int cmd = (int)va_arg(args, long);
		struct shmid_ds * buf = (struct shmid_ds *)va_arg(args, long);
		ret = shmctl(shmid, cmd, buf);
		break;
	}
	case SYS_msgctl: {
		int msqid = (int)va_arg(args, long);
		int cmd = (int)va_arg(args, long);
		struct msqid_ds * buf = (struct msqid_ds *)va_arg(args, long);
		ret = msgctl(msqid, cmd, buf);
		break;
	}
	case SYS_sched_yield:
		ret = sched_yield();
		break;
	case SYS_getthrid:
		ret = getthrid();
		break;
	/* No signature found in headers
	 *case SYS___thrwakeup: {
	 *	const volatile void * ident = (const volatile void *)va_arg(args, long);
	 *	int n = (int)va_arg(args, long);
	 *	ret = __thrwakeup(ident, n);
	 *	break;
	 *}
	 */
	/* No signature found in headers
	 *case SYS___threxit:
	 *	__threxit(va_arg(args, pid_t *)); // notdead
	 *	break;
	 */
	/* No signature found in headers
	 *case SYS___thrsigdivert: {
	 *	sigset_t sigmask = (sigset_t)va_arg(args, long);
	 *	siginfo_t * info = (siginfo_t *)va_arg(args, long);
	 *	const struct timespec * timeout = (const struct timespec *)va_arg(args, long);
	 *	ret = __thrsigdivert(sigmask, info, timeout);
	 *	break;
	 *}
	 */
	/* No signature found in headers
	 *case SYS___getcwd: {
	 *	char * buf = (char *)va_arg(args, long);
	 *	size_t len = (size_t)va_arg(args, long);
	 *	ret = __getcwd(buf, len);
	 *	break;
	 *}
	 */
	case SYS_adjfreq: {
		const int64_t * freq = (const int64_t *)va_arg(args, long);
		int64_t * oldfreq = (int64_t *)va_arg(args, long);
		ret = adjfreq(freq, oldfreq);
		break;
	}
	case SYS_setrtable:
		ret = setrtable(va_arg(args, int)); // rtableid
		break;
	case SYS_getrtable:
		ret = getrtable();
		break;
	case SYS_faccessat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		int amode = (int)va_arg(args, long);
		int flag = (int)va_arg(args, long);
		ret = faccessat(fd, path, amode, flag);
		break;
	}
	case SYS_fchmodat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		int flag = (int)va_arg(args, long);
		ret = fchmodat(fd, path, mode, flag);
		break;
	}
	case SYS_fchownat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		uid_t uid = (uid_t)va_arg(args, long);
		gid_t gid = (gid_t)va_arg(args, long);
		int flag = (int)va_arg(args, long);
		ret = fchownat(fd, path, uid, gid, flag);
		break;
	}
	case SYS_linkat: {
		int fd1 = (int)va_arg(args, long);
		const char * path1 = (const char *)va_arg(args, long);
		int fd2 = (int)va_arg(args, long);
		const char * path2 = (const char *)va_arg(args, long);
		int flag = (int)va_arg(args, long);
		ret = linkat(fd1, path1, fd2, path2, flag);
		break;
	}
	case SYS_mkdirat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = mkdirat(fd, path, mode);
		break;
	}
	case SYS_mkfifoat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = mkfifoat(fd, path, mode);
		break;
	}
	case SYS_mknodat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		dev_t dev = (dev_t)va_arg(args, long);
		ret = mknodat(fd, path, mode, dev);
		break;
	}
	case SYS_openat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		int flags = (int)va_arg(args, long);
		mode_t mode = (mode_t)va_arg(args, long);
		ret = openat(fd, path, flags, mode);
		break;
	}
	case SYS_readlinkat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		char * buf = (char *)va_arg(args, long);
		size_t count = (size_t)va_arg(args, long);
		ret = readlinkat(fd, path, buf, count);
		break;
	}
	case SYS_renameat: {
		int fromfd = (int)va_arg(args, long);
		const char * from = (const char *)va_arg(args, long);
		int tofd = (int)va_arg(args, long);
		const char * to = (const char *)va_arg(args, long);
		ret = renameat(fromfd, from, tofd, to);
		break;
	}
	case SYS_symlinkat: {
		const char * path = (const char *)va_arg(args, long);
		int fd = (int)va_arg(args, long);
		const char * link = (const char *)va_arg(args, long);
		ret = symlinkat(path, fd, link);
		break;
	}
	case SYS_unlinkat: {
		int fd = (int)va_arg(args, long);
		const char * path = (const char *)va_arg(args, long);
		int flag = (int)va_arg(args, long);
		ret = unlinkat(fd, path, flag);
		break;
	}
	case SYS___set_tcb:
		__set_tcb(va_arg(args, void *)); // tcb
		break;
	case SYS___get_tcb:
		ret = (long)__get_tcb();
		break;
	default:
		ret = -1;
		errno = ENOSYS;
	}
	va_end(args);

	return ret;
}
