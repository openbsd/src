# $OpenBSD: Makefile.inc,v 1.9 2003/06/16 13:35:33 mickey Exp $
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu

LIBPTHREAD=	/usr/lib/libpthread.a

LDADD+=		-pthread
DPADD+=		${LIBPTHREAD}

CFLAGS+=	-Wall # -Werror
#DEBUG=		-ggdb
CFLAGS+=	-DSRCDIR='"${.CURDIR}"'
CFLAGS+=	-I${.CURDIR}/../include

REGRESS_MAXTIME?=30
