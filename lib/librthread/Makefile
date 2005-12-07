#	$OpenBSD: Makefile,v 1.3 2005/12/07 03:18:39 tedu Exp $

LIB=rthread
CFLAGS+=-Wall -g -Werror -Wshadow

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c rthread_sig.c
OBJS+= _atomic_lock.o rfork_thread.o # .S on alpha

.include <bsd.lib.mk>
