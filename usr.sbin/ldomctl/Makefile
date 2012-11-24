#	$OpenBSD: Makefile,v 1.8 2012/11/24 11:50:45 kettenis Exp $

.if ${MACHINE} == "sparc64"

.PATH:	${.CURDIR}/../ldomd

PROG=	ldomctl
SRCS=	ldomctl.c ds.c mdesc.c util.c mdstore.c pri.c config.c parse.y
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../ldomd -I${.CURDIR}
DEBUG= -g

.else

NOPROG=	yes

.endif

MAN=	ldomctl.8
MANSUBDIR=sparc64

.include <bsd.prog.mk>
