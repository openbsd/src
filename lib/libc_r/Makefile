#	$OpenBSD: Makefile,v 1.4 1999/01/08 05:45:27 d Exp $
#
# All library objects contain sccsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_SCCS and -DSYSLIBC_SCCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_SCCS from CFLAGS.
#
# The NLS (message catalog) functions are always in libc.  To choose that
# strerror(), perror(), strsignal(), psignal(), etc. actually call the NLS
# functions, put -DNLS on the CFLAGS line below.
#
# The YP functions are always in libc. To choose that getpwent() and friends
# actually call the YP functions, put -DYP on the CFLAGS line below.

.include <bsd.own.mk>

LIB=		c_r
LINTFLAGS=	-z
CFLAGS+=	-DPTHREAD_KERNEL -D_POSIX_THREADS -D_THREAD_SAFE 
CFLAGS+=	-DNOPOLL -I${.CURDIR}/uthread
CFLAGS+=	-I${.CURDIR}/include
DEBUG=		-ggdb -Wall

# Uncomment this if you want libc_r to contain debug information for
# thread locking.
CFLAGS+=	-D_LOCK_DEBUG

#
# This is a list of syscalls that are renamed as _thread_sys_{syscall}
# so that libc_r can provide replacement functions.
#
HIDDEN_SYSCALLS= accept.o bind.o close.o connect.o dup.o dup2.o \
	execve.o fchflags.o fchmod.o fchown.o fcntl.o \
	flock.o fpathconf.o fstat.o fstatfs.o fsync.o getdirentries.o \
	getpeername.o getsockname.o getsockopt.o ioctl.o listen.o \
	nanosleep.o open.o poll.o read.o readv.o recvfrom.o \
	recvmsg.o sched_yield.o select.o sendmsg.o sendto.o \
	setsockopt.o shutdown.o sigaction.o sigaltstack.o \
	socket.o socketpair.o wait4.o write.o writev.o \
	_exit.o getlogin.o

LIBCSRCDIR = ${.CURDIR}/../libc
.include "${LIBCSRCDIR}/Makefile.inc"

# libc man pages built/installed by libc
MAN=
MLINKS=

.include "${.CURDIR}/man/Makefile.inc"
.include "${.CURDIR}/uthread/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"

AINC+=	-I${.CURDIR}/uthread

SHLIB_MAJOR != . ${LIBCSRCDIR}/shlib_version ; echo $$major
SHLIB_MINOR != . ${LIBCSRCDIR}/shlib_version ; echo $$minor

.include "${.CURDIR}/include/Makefile.inc"

.include <bsd.lib.mk>
