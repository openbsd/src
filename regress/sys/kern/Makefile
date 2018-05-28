#	$OpenBSD: Makefile,v 1.71 2018/05/28 09:04:44 mpi Exp $

SUBDIR+= __syscall access accept dup2 dup2_accept dup2_self
SUBDIR+= exec_self execve exit extent
SUBDIR+= fchdir
SUBDIR+= fcntl_dup
SUBDIR+= flock
SUBDIR+= futex
SUBDIR+= getpeereid getrusage gettimeofday itimer 
SUBDIR+= kqueue mbuf minherit
SUBDIR+= main-thread-exited
SUBDIR+= mmap mmap2 mmap3 mmap-fail
SUBDIR+= mount
SUBDIR+= nanosleep noexec
SUBDIR+= pledge
SUBDIR+= pread preadv ptmget pty pwrite pwritev rcvtimeo
SUBDIR+= rlimit-file signal signal-stress sigprof sigsuspend
SUBDIR+= sosplice
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait
SUBDIR+= sig-stop
SUBDIR+= unixsock bind

# The setuid subtest creates set user/group id binaries in the obj directory.
# Do not run this test by default, it may trigger alerts from daily security.
.ifmake clean || cleandir || obj
SUBDIR+= setuid
.endif

install:

.include <bsd.subdir.mk>
