#	$OpenBSD: Makefile,v 1.11 2000/01/06 21:19:10 d Exp $

#
# The pthread library is formed from the FreeBSD uthread sources of 
# libc_r, and exploits weak symbols in libc.
#

LIBC_RSRCDIR=	${.CURDIR}/../libc_r
LIBCSRCDIR=	${.CURDIR}/../libc

.PATH:		${LIBC_RSRCDIR}

LIB=		pthread
LINTFLAGS=	-z
CFLAGS+=	-DPTHREAD_KERNEL -D_POSIX_THREADS -D_THREAD_SAFE 
CFLAGS+=	-D_PTHREADS_INVARIANTS
CFLAGS+=	-DNOPOLL -I${LIBC_RSRCDIR}/uthread
CFLAGS+=	-I${LIBC_RSRCDIR}/include
CFLAGS+=	-I${LIBCSRCDIR}/include
AINC+=		-I${LIBC_RSRCDIR}/uthread

# Uncomment this if you want libc_r to contain debug information for
# thread locking.
CFLAGS+=	-D_LOCK_DEBUG
DEBUG=		-ggdb -Wall

.include "${LIBC_RSRCDIR}/include/Makefile.inc"
.include "${LIBC_RSRCDIR}/man/Makefile.inc"
.include "${LIBC_RSRCDIR}/sys/Makefile.inc"
.include "${LIBC_RSRCDIR}/thread/Makefile.inc"
.include "${LIBC_RSRCDIR}/uthread/Makefile.inc"

.include <bsd.own.mk>
.include <bsd.lib.mk>
