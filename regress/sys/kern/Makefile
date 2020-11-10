#	$OpenBSD: Makefile,v 1.88 2020/11/10 16:13:35 bluhm Exp $

SUBDIR+= __syscall
SUBDIR+= accept access
SUBDIR+= bind
SUBDIR+= clock_gettime cmsgsize
SUBDIR+= descrip dup2 dup2_accept dup2_self
SUBDIR+= exec_self execve exit extent
SUBDIR+= fchdir fchown fcntl_dup flock ftruncate futex
SUBDIR+= getpeereid getrusage gettimeofday
SUBDIR+= itimer
SUBDIR+= kqueue
SUBDIR+= main-thread-exited mbuf minherit mmap mmap-fail mmap2 mmap3 mount
SUBDIR+= nanosleep noexec
SUBDIR+= open
SUBDIR+= pipe pledge pread preadv ptmget ptrace ptrace2 pty pwrite pwritev
SUBDIR+= rcvtimeo realpath realpath-unmount recvwait rlimit-file
# The setuid subtest creates set user/group id binaries in the obj directory.
# Do not run this test by default, it may trigger alerts from daily security.
.ifmake clean || cleandir || obj
SUBDIR+= setuid
.endif
SUBDIR+= signal sosplice stackjmp stackpivot syscall syscall_segment
SUBDIR+= sysvmsg sysvsem sysvshm
SUBDIR+= unalign unfdpass unixsock unveil unveil-unmount
SUBDIR+= wait

install:

.include <bsd.subdir.mk>
