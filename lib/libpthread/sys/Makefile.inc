#	$OpenBSD: Makefile.inc,v 1.15 2004/04/09 23:56:05 brad Exp $

.PATH:	 ${SRCDIR}/sys ${SRCDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c

.if (${MACHINE_ARCH} == "mips")
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
