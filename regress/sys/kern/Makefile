#	$OpenBSD: Makefile,v 1.63 2015/02/10 00:45:31 claudio Exp $

SUBDIR+= __syscall access accept dup2 dup2_self exec_self execve exit extent
SUBDIR+= fchdir
SUBDIR+= fcntl_dup
SUBDIR+= flock
SUBDIR+= getpeereid getrusage gettimeofday itimer 
SUBDIR+= kqueue mbuf minherit
SUBDIR+= main-thread-exited
SUBDIR+= mmap mmap2 mmap3 mmap-fail
#SUBDIR+= mquery
SUBDIR+= nanosleep noexec pread preadv ptmget pty pwrite pwritev rcvtimeo
SUBDIR+= rlimit-file signal signal-stress sigprof sigsuspend
SUBDIR+= sosplice
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait
SUBDIR+= sig-stop
SUBDIR+= unixsock bind

install:

.include <bsd.subdir.mk>
