#	$OpenBSD: Makefile,v 1.10 2000/01/04 00:54:51 d Exp $

LIBC_RSRCDIR=	${.CURDIR}/../libc_r
LIBCSRCDIR=	${.CURDIR}/../libc

.PATH:		${LIBC_RSRCDIR}

DEBUG=		-ggdb -Wall
LIB=		pthread
LINTFLAGS=	-z
CFLAGS+=	-DPTHREAD_KERNEL -D_POSIX_THREADS -D_THREAD_SAFE 
CFLAGS+=	-D_PTHREADS_INVARIANTS
CFLAGS+=	-DNOPOLL -I${LIBC_RSRCDIR}/uthread
CFLAGS+=	-I${LIBC_RSRCDIR}/include

# Uncomment this if you want libc_r to contain debug information for
# thread locking.
CFLAGS+=	-D_LOCK_DEBUG

.include "${LIBC_RSRCDIR}/uthread/Makefile.inc"

CFLAGS+=	-I${LIBCSRCDIR}/include
SRCS+=		libc_storage.c

.include "${LIBC_RSRCDIR}/man/Makefile.inc"
.include "${LIBC_RSRCDIR}/sys/Makefile.inc"

AINC+=		-I${LIBC_RSRCDIR}/uthread

.include "${LIBC_RSRCDIR}/include/Makefile.inc"

.include <bsd.own.mk>
.include <bsd.lib.mk>
