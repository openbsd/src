#	$OpenBSD: Makefile,v 1.12 2002/01/15 22:37:02 fgsch Exp $

# libc_r will be depreceated and replaced by libpthread
# when all supported architectures support weak symbols.
# See also comments in ../libc/Makefile.

.include <bsd.own.mk>

LIBCSRCDIR = ${.CURDIR}/../libc
LIBC_RSRCDIR = ${.CURDIR}

LIB=		c_r
LINTFLAGS=	-z
CFLAGS+=	-DPTHREAD_KERNEL -D_POSIX_THREADS -D_THREAD_SAFE 
CFLAGS+=	-I${LIBC_RSRCDIR}/uthread -I${LIBC_RSRCDIR}/include
CFLAGS+=	-I${LIBCSRCDIR}/include

# Uncomment this if you want libc_r to contain debug information for
# thread locking.
CFLAGS+=	-D_LOCK_DEBUG
#DEBUG=		-ggdb -Wall

# enable extra internal consistency checks
CFLAGS+=	-D_PTHREADS_INVARIANTS

.include "${LIBCSRCDIR}/Makefile.inc"

# annul man pages that are built/installed by libc
MAN=
MLINKS=

# XXX bogus version number!
SHLIB_MAJOR != . ${LIBC_RSRCDIR}/shlib_version ; echo $$major
SHLIB_MINOR != . ${LIBC_RSRCDIR}/shlib_version ; echo $$minor

AINC+=	-I${LIBC_RSRCDIR}/uthread

.include "${LIBC_RSRCDIR}/uthread/Makefile.inc"
.include "${LIBC_RSRCDIR}/sys/Makefile.inc"
.include "${LIBC_RSRCDIR}/thread/Makefile.inc"
.include "${LIBC_RSRCDIR}/man/Makefile.inc"
.include "${LIBC_RSRCDIR}/include/Makefile.inc"

.include <bsd.lib.mk>
