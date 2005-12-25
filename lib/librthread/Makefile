#	$OpenBSD: Makefile,v 1.4 2005/12/25 00:47:07 tedu Exp $

LIB=rthread
CFLAGS+=-Wall -g -Werror -Wshadow

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c rthread_sig.c
OBJS+= _atomic_lock.o rfork_thread.o

.include <bsd.lib.mk>
