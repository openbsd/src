#	$OpenBSD: Makefile,v 1.55 2019/02/13 13:22:14 mpi Exp $

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
	rthread_fork.c \
	rthread_getcpuclockid.c \
	rthread_mutex_prio.c \
	rthread_mutexattr.c \
	rthread_np.c \
	rthread_rwlockattr.c \
	rthread_sched.c \
	rthread_stack.c \
	rthread_spin_lock.c \
	sched_prio.c

# Architectures that implement atomics
.if ${MACHINE_ARCH} == "aarch64" || ${MACHINE_ARCH} == "alpha" || \
    ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "arm" || \
    ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "mips64" || \
    ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc64"
CFLAGS+= -DFUTEX
SRCS+=	rthread_sem.c \
	rthread_rwlock.c
.else
SRCS+=	rthread_sem_compat.c \
	rthread_rwlock_compat.c
.endif

SRCDIR= ${.CURDIR}/../libpthread
.include "${SRCDIR}/man/Makefile.inc"
.include <bsd.lib.mk>

${OBJS} ${GOBJS} ${POBJS} ${SOBJS} ${DOBJS}: ${.CURDIR}/namespace.h
