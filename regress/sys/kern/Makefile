#	$OpenBSD: Makefile,v 1.66 2016/09/23 18:40:30 otto Exp $

SUBDIR+= __syscall access accept dup2 dup2_self exec_self execve exit extent
SUBDIR+= fchdir
SUBDIR+= fcntl_dup
SUBDIR+= flock
SUBDIR+= getpeereid getrusage gettimeofday itimer 
SUBDIR+= kqueue mbuf minherit
SUBDIR+= main-thread-exited
SUBDIR+= mmap mmap2 mmap3 mmap-fail
SUBDIR+= nanosleep noexec
#SUBDIR+= pledge
SUBDIR+= pread preadv ptmget pty pwrite pwritev rcvtimeo
SUBDIR+= rlimit-file signal signal-stress sigprof sigsuspend
SUBDIR+= sosplice
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait
SUBDIR+= sig-stop
SUBDIR+= unixsock bind

install:

.include <bsd.subdir.mk>
