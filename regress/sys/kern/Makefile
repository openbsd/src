#	$OpenBSD: Makefile,v 1.29 2002/02/22 23:34:31 art Exp $

SUBDIR+= execve getrusage kqueue mmap mmap2 dup2 minherit rlimit-file
SUBDIR+= fcntl_dup dup2_self pread preadv exit pwrite pwritev
SUBDIR+= syscall __syscall unfdpass accept nanosleep sysvmsg sysvsem
SUBDIR+= sysvshm rfork

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
