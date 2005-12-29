#	$OpenBSD: Makefile,v 1.5 2005/12/29 20:34:22 otto Exp $

LIB=rthread
CFLAGS+=-Wall -g -Werror -Wshadow

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS= rthread.c rthread_attr.c rthread_sched.c rthread_sync.c rthread_tls.c \
	rthread_sig.c rthread_np.c

OBJS+= _atomic_lock.o rfork_thread.o

.include <bsd.lib.mk>
