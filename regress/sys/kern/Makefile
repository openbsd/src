#	$OpenBSD: Makefile,v 1.38 2002/08/31 20:29:46 mickey Exp $

SUBDIR+= execve getrusage kqueue mmap mmap2 mmap3 dup2 minherit rlimit-file
SUBDIR+= fcntl_dup dup2_self pread preadv exit pwrite pwritev
SUBDIR+= syscall __syscall unfdpass accept nanosleep sysvmsg sysvsem
SUBDIR+= sysvshm rfork gettimeofday signal exec_self
SUBDIR+= nxstack nxstack-mprotect
SUBDIR+= nxheap  nxheap-mprotect
SUBDIR+= nxbss

install:

.include <bsd.subdir.mk>
