# $OpenBSD: Makefile.inc,v 1.5 2002/06/17 04:48:24 marc Exp $
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu

LIBC_R?=	/usr/lib/libc_r.a
LIBPTHREAD?=	/usr/lib/libpthread.a
USELIBPTHREAD?=	no

.if ${USELIBPTHREAD:L} == yes
# this tests using libpthread/libc
LDADD+=		-lpthread
DPADD+=		${LIBPTHREAD}
.else
# This tests using libc_r
LDADD+=		-pthread
DPADD+=		${LIBC_R}
.endif

CFLAGS+=	-Wall # -Werror
#DEBUG=		-ggdb
CFLAGS+=	-DSRCDIR='"${.CURDIR}"'
CFLAGS+=	-I${.CURDIR}/../include

NOMAN=

REGRESSMAXTIME?=30
