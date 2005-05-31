#	$OpenBSD: Makefile,v 1.50 2005/05/31 09:28:07 art Exp $

SUBDIR+= __syscall accept dup2 dup2_self exec_self execve exit extent
SUBDIR+= fcntl_dup getrusage gettimeofday itimer kqueue mbuf minherit
SUBDIR+= mmap mmap2 mmap3
#SUBDIR+= mquery
SUBDIR+= nanosleep noexec pread preadv ptmget pwrite pwritev rcvtimeo
#SUBDIR+= rfork
SUBDIR+= rlimit-file signal signal-stress sigsuspend
SUBDIR+= syscall sysvmsg sysvsem
SUBDIR+= sysvshm unalign unfdpass wait

install:

.include <bsd.subdir.mk>
