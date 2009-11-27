#	$OpenBSD: Makefile,v 1.14 2009/11/27 19:42:24 guenther Exp $

LIB=rthread
WANTLINT=
LINTFLAGS=-z
LIBCSRCDIR=	${.CURDIR}/../libc

CFLAGS+=-Wall -g -Werror -Wshadow
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes -Wsign-compare
CFLAGS+=-I${LIBCSRCDIR}/include

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c \
	rthread_sig.c rthread_np.c rthread_debug.c rthread_stack.c \
	rthread_libc.c rthread_fork.c rthread_file.c

OBJS+= _atomic_lock.o rfork_thread.o

.include <bsd.lib.mk>
