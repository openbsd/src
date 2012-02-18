#	$OpenBSD: Makefile,v 1.25 2012/02/18 16:24:02 miod Exp $

LIB=pthread
WANTLINT=
LINTFLAGS=-z
LIBCSRCDIR=	${.CURDIR}/../libc

CFLAGS+=-Wall -g -Werror -Wshadow
.if "${MACHINE_ARCH}" != "vax"
CFLAGS+=-Wmissing-prototypes
.endif
CFLAGS+=-Wstrict-prototypes
CFLAGS+=-Wsign-compare
CFLAGS+=-I${LIBCSRCDIR}/arch/${MACHINE_CPU} -I${LIBCSRCDIR}/include
LDADD = -Wl,-znodelete,-zinitfirst

.PATH: ${.CURDIR}/arch/${MACHINE_CPU}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c \
	rthread_sig.c rthread_np.c rthread_debug.c rthread_stack.c \
	rthread_libc.c rthread_fork.c rthread_file.c sched_prio.c \
	rthread_cancel.c rthread_mutexattr.c rthread_once.c \
	rthread_rwlock.c rthread_rwlockattr.c rthread_mutex_prio.c \
	rthread_sem.c

OBJS+= _atomic_lock.o rfork_thread.o cerror.o

MAN+=	__tfork_thread.3
MLINKS+=__tfork_thread.3 __tfork.2
.include <bsd.lib.mk>

SRCDIR= ${.CURDIR}/../libpthread
.include "${SRCDIR}/include/Makefile.inc"
.include "${SRCDIR}/man/Makefile.inc"
.include <bsd.lib.mk>

