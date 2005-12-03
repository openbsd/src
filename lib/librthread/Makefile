#	$OpenBSD: Makefile,v 1.2 2005/12/03 18:36:40 tedu Exp $

LIB=rthread
CFLAGS+=-Wall -g -Werror

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c rthread_sig.c
OBJS+= _atomic_lock.o rfork_thread.o # .S on alpha

.include <bsd.lib.mk>
