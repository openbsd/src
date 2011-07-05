#	$OpenBSD: Makefile,v 1.55 2011/07/05 23:14:19 claudio Exp $

SUBDIR+= __syscall accept dup2 dup2_self exec_self execve exit extent
SUBDIR+= fcntl_dup
SUBDIR+= flock
SUBDIR+= getpeereid getrusage gettimeofday itimer 
SUBDIR+= kqueue mbuf minherit
SUBDIR+= mmap mmap2 mmap3 mmap-fail
#SUBDIR+= mquery
SUBDIR+= nanosleep noexec pread preadv ptmget pwrite pwritev rcvtimeo
#SUBDIR+= rfork
SUBDIR+= rlimit-file signal signal-stress sigsuspend
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait
SUBDIR+= sig-stop
SUBDIR+= unixsock

install:

.include <bsd.subdir.mk>
