#	$OpenBSD: Makefile,v 1.53 2007/04/29 20:13:16 art Exp $

SUBDIR+= __syscall accept dup2 dup2_self exec_self execve exit extent
SUBDIR+= fcntl_dup getpeereid getrusage gettimeofday itimer 
SUBDIR+= kqueue mbuf minherit
SUBDIR+= mmap mmap2 mmap3 mmap-fail
#SUBDIR+= mquery
SUBDIR+= nanosleep noexec pread preadv ptmget pwrite pwritev rcvtimeo
#SUBDIR+= rfork
SUBDIR+= rlimit-file signal signal-stress sigsuspend
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait
SUBDIR+= sig-stop

install:

.include <bsd.subdir.mk>
