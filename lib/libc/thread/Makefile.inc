#	$OpenBSD: Makefile.inc,v 1.16 2018/09/24 11:25:09 jsg Exp $

.PATH: ${LIBCSRCDIR}/thread

SRCS+=	callbacks.c atfork.c

# threads infrastructure
SRCS+=	rthread.c \
	rthread_condattr.c \
	rthread_debug.c \
	rthread_file.c \
	rthread_libc.c \
	rthread_once.c \
	rthread_tls.c \

notyet= rthread_condattr_clock.c \
	rthread_equal.c \
	rthread_exit.c \
	spinlock.c \
	spinlocktry.c

.if ${MACHINE_ARCH} == "aarch64" || ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el" || \
    ${MACHINE_ARCH} == "sparc64"
CFLAGS+= -DFUTEX
SRCS+=	rthread_mutex.c \
	rthread_cond.c
.else
SRCS+=	rthread_sync.c
.endif

.if defined(NOPIC)
CFLAGS+=-DNO_PIC
.endif

OBJS+=	_atomic_lock.o

