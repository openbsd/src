#	$OpenBSD: Makefile,v 1.18 2003/01/31 04:46:16 marc Exp $

#
# The pthread library is formed from the FreeBSD uthread sources of 
# libc_r, and exploits weak symbols in libc.
#

SRCDIR=		${.CURDIR}
LIBCSRCDIR=	${.CURDIR}/../libc

LIB=		pthread
LINTFLAGS=	-z
CFLAGS+=	-DPTHREAD_KERNEL -D_POSIX_THREADS -D_THREAD_SAFE -Wall
#CFLAGS+=	-Wpointer-arith -Wwrite-strings -Wstrict-prototypes
#CFLAGS+=	-Wmissing-prototypes -Wsign-compare -Werror
CFLAGS+=	-I${SRCDIR}/uthread
CFLAGS+=	-I${SRCDIR}/include
CFLAGS+=	-I${LIBCSRCDIR}/include
AINC+=		-I${SRCDIR}/uthread

# Uncomment this if you want libpthread to contain debug information for
# thread locking.
CFLAGS+=	-D_LOCK_DEBUG
#DEBUG=		-g

# enable extra internal consistency checks
CFLAGS+=	-D_PTHREADS_INVARIANTS

.include "${SRCDIR}/include/Makefile.inc"
.include "${SRCDIR}/sys/Makefile.inc"
.include "${SRCDIR}/thread/Makefile.inc"
.include "${SRCDIR}/uthread/Makefile.inc"
.include "${SRCDIR}/man/Makefile.inc"

.include <bsd.own.mk>
.include <bsd.lib.mk>
