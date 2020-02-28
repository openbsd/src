#	$OpenBSD: Makefile,v 1.86 2020/02/28 12:50:06 mpi Exp $

SUBDIR+= __syscall access accept cmsgsize clock_gettime
SUBDIR+= dup2 dup2_accept dup2_self descrip
SUBDIR+= exec_self execve exit extent
SUBDIR+= fchdir
SUBDIR+= fchown
SUBDIR+= fcntl_dup
SUBDIR+= flock
SUBDIR+= ftruncate
SUBDIR+= futex
SUBDIR+= getpeereid getrusage gettimeofday itimer
SUBDIR+= kqueue mbuf minherit
SUBDIR+= main-thread-exited
SUBDIR+= mmap mmap2 mmap3 mmap-fail
SUBDIR+= mount
SUBDIR+= nanosleep noexec
SUBDIR+= open
SUBDIR+= pipe
SUBDIR+= pledge
SUBDIR+= pread preadv ptmget ptrace2 pty pwrite pwritev rcvtimeo
SUBDIR+= realpath
SUBDIR+= realpath-unmount
SUBDIR+= recvwait
SUBDIR+= rlimit-file
SUBDIR+= sigio signal signal-stress sigprof sigsuspend sigpthread
SUBDIR+= sosplice
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait
SUBDIR+= sig-stop
SUBDIR+= unixsock bind
SUBDIR+= unveil
SUBDIR+= unveil-unmount

# The setuid subtest creates set user/group id binaries in the obj directory.
# Do not run this test by default, it may trigger alerts from daily security.
.ifmake clean || cleandir || obj
SUBDIR+= setuid
.endif

install:

.include <bsd.subdir.mk>
