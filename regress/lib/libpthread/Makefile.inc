# $OpenBSD: Makefile.inc,v 1.10 2012/08/19 18:55:16 fgsch Exp $
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu

LIBPTHREAD=	/usr/lib/libpthread.a

LDADD+=		-lpthread
DPADD+=		${LIBPTHREAD}

CFLAGS+=	-Wall # -Werror
#DEBUG=		-ggdb
CFLAGS+=	-DSRCDIR='"${.CURDIR}"'
CFLAGS+=	-I${.CURDIR}/../include

REGRESS_MAXTIME?=30
