#	$OpenBSD: Makefile,v 1.15 2002/11/16 05:01:26 marc Exp $

#
# The pthread library is formed from the FreeBSD uthread sources of 
# libc_r, and exploits weak symbols in libc.   It is installed as
# libnpthread (new pthread) so it will not be picked up in addition to
# libc_r by misbehaving ports.
#

LIBC_RSRCDIR=	${.CURDIR}/../libc_r
LIBCSRCDIR=	${.CURDIR}/../libc

.PATH:		${LIBC_RSRCDIR}

LIB=		npthread
LINTFLAGS=	-z
CFLAGS+=	-DPTHREAD_KERNEL -D_POSIX_THREADS -D_THREAD_SAFE -Wall
CFLAGS+=	-I${LIBC_RSRCDIR}/uthread
CFLAGS+=	-I${LIBC_RSRCDIR}/include
CFLAGS+=	-I${LIBCSRCDIR}/include
AINC+=		-I${LIBC_RSRCDIR}/uthread

# Uncomment this if you want libc_r to contain debug information for
# thread locking.
CFLAGS+=	-D_LOCK_DEBUG
#DEBUG=		-g

# enable extra internal consistency checks
CFLAGS+=	-D_PTHREADS_INVARIANTS

.include "${LIBC_RSRCDIR}/include/Makefile.inc"
.include "${LIBC_RSRCDIR}/sys/Makefile.inc"
.include "${LIBC_RSRCDIR}/thread/Makefile.inc"
.include "${LIBC_RSRCDIR}/uthread/Makefile.inc"

# XXX enable when libc_r is not built
#.include "${LIBC_RSRCDIR}/man/Makefile.inc"

.include <bsd.own.mk>
.include <bsd.lib.mk>
