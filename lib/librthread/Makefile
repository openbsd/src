#	$OpenBSD: Makefile,v 1.47 2017/07/04 14:33:43 visa Exp $

LIB=pthread
LIBCSRCDIR=	${.CURDIR}/../libc

CFLAGS+=-Wall -g -Werror -Wshadow
CFLAGS+=-Werror-implicit-function-declaration
CFLAGS+=-Wsign-compare
CFLAGS+=-I${.CURDIR} -include namespace.h \
	-I${LIBCSRCDIR}/arch/${MACHINE_CPU} -I${LIBCSRCDIR}/include
CDIAGFLAGS=
LDADD = -Wl,-znodelete

.if defined(NOPIC)
CFLAGS+=-DNO_PIC
.endif

VERSION_SCRIPT= ${.CURDIR}/Symbols.map

.PATH: ${.CURDIR}/arch/${MACHINE_CPU}
SRCS=	rthread.c \
	rthread_attr.c \
	rthread_barrier.c \
	rthread_barrier_attr.c \
	rthread_condattr.c \
	rthread_debug.c \
	rthread_file.c \
	rthread_fork.c \
	rthread_getcpuclockid.c \
	rthread_libc.c \
	rthread_mutex_prio.c \
	rthread_mutexattr.c \
	rthread_np.c \
	rthread_once.c \
	rthread_rwlock.c \
	rthread_rwlockattr.c \
	rthread_sched.c \
	rthread_sem.c \
	rthread_sig.c \
	rthread_stack.c \
	rthread_spin_lock.c \
	rthread_tls.c \
	sched_prio.c

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el"
CFLAGS+= -DFUTEX
SRCS+=	rthread_mutex.c \
	rthread_cond.c
.else
SRCS+=	rthread_sync.c
.endif

OBJS+= _atomic_lock.o

SRCDIR= ${.CURDIR}/../libpthread
.include "${SRCDIR}/include/Makefile.inc"
.include "${SRCDIR}/man/Makefile.inc"
.include <bsd.lib.mk>

${OBJS} ${GOBJS} ${POBJS} ${SOBJS} ${DOBJS}: ${.CURDIR}/namespace.h
