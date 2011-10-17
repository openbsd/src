#	$OpenBSD: Makefile,v 1.16 2011/10/17 06:39:20 guenther Exp $

LIB=rthread
WANTLINT=
LINTFLAGS=-z
LIBCSRCDIR=	${.CURDIR}/../libc

CFLAGS+=-Wall -g -Werror -Wshadow
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes -Wsign-compare
CFLAGS+=-I${LIBCSRCDIR}/arch/${MACHINE_CPU} -I${LIBCSRCDIR}/include

.PATH: ${.CURDIR}/arch/${MACHINE_CPU}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c \
	rthread_sig.c rthread_np.c rthread_debug.c rthread_stack.c \
	rthread_libc.c rthread_fork.c rthread_file.c

OBJS+= _atomic_lock.o rfork_thread.o cerror.o

MAN+=	__tfork_thread.3
MLINKS+=__tfork_thread.3 __tfork.2

.include <bsd.lib.mk>
