#	$OpenBSD: Makefile,v 1.10 2008/06/05 21:06:11 kurt Exp $

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
	rthread_reaper.c rthread_libc.c rthread_fork.c

OBJS+= _atomic_lock.o rfork_thread.o

.include <bsd.lib.mk>
