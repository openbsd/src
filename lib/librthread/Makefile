#	$OpenBSD: Makefile,v 1.8 2006/01/04 19:48:52 otto Exp $

LIB=rthread
WANTLINT=
LINTFLAGS=-z
CFLAGS+=-Wall -g -Werror -Wshadow

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c \
	rthread_sig.c rthread_np.c rthread_debug.c rthread_stack.c \
	rthread_reaper.c

OBJS+= _atomic_lock.o rfork_thread.o

.include <bsd.lib.mk>
