#	$OpenBSD: Makefile,v 1.27 2002/02/16 14:35:42 art Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/20 22:42:51 cgd Exp $

SUBDIR+= execve getrusage kqueue mmap mmap2 dup2 minherit rlimit-file
SUBDIR+= fcntl_dup dup2_self pread preadv exit pwrite pwritev
SUBDIR+= syscall __syscall unfdpass accept nanosleep sysvmsg sysvsem
SUBDIR+= sysvshm

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
