#	$OpenBSD: Makefile.inc,v 1.16 2004/08/11 11:11:49 pefo Exp $

.PATH:	 ${SRCDIR}/sys ${SRCDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c

.if (${MACHINE_ARCH} == "mips64")
SRCS+=	slow_atomic_lock.c
.endif

.if exists(${SRCDIR}/arch/${MACHINE_ARCH}/_atomic_lock.c)
SRCS+=	_atomic_lock.c
.endif
.if exists(${SRCDIR}/arch/${MACHINE_ARCH}/uthread_machdep_asm.S)
SRCS+=	uthread_machdep_asm.S
.endif
.if exists(${SRCDIR}/arch/${MACHINE_ARCH}/uthread_machdep.c)
SRCS+=	uthread_machdep.c
.endif
