# $OpenBSD: Makefile.inc,v 1.3 2002/01/03 00:43:47 art Exp $
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu

LIBC_R?=	/usr/lib/libc_r.a
LIBPTHREAD?=	/usr/lib/libpthread.a

.if 1 # ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "sparc"
LDADD+=		-lpthread
DPADD+=		${LIBPTHREAD}
.else
LDADD+=		-pthread
DPADD+=		${LIBC_R}
.endif

CFLAGS+=	-Wall # -Werror
DEBUG=		-ggdb
CFLAGS+=	-DSRCDIR='"${.CURDIR}"'
CFLAGS+=	-I${.CURDIR}/../include

NOMAN=

REGRESSMAXTIME?=5
