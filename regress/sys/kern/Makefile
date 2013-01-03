#	$OpenBSD: Makefile,v 1.60 2013/01/03 17:43:34 bluhm Exp $

SUBDIR+= __syscall accept dup2 dup2_self exec_self execve exit extent
SUBDIR+= fchdir
SUBDIR+= fcntl_dup
SUBDIR+= flock
SUBDIR+= getpeereid getrusage gettimeofday itimer 
SUBDIR+= kqueue mbuf minherit
SUBDIR+= main-thread-exited
SUBDIR+= mmap mmap2 mmap3 mmap-fail
#SUBDIR+= mquery
SUBDIR+= nanosleep noexec pread preadv ptmget pty pwrite pwritev rcvtimeo
SUBDIR+= rlimit-file signal signal-stress sigsuspend
SUBDIR+= sosplice
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait
SUBDIR+= sig-stop
SUBDIR+= unixsock

install:

.include <bsd.subdir.mk>
