# $OpenBSD: Makefile.inc,v 1.6 2002/09/02 20:01:43 avsm Exp $
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

REGRESS_MAXTIME?=30
