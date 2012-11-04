#	$OpenBSD: Makefile,v 1.6 2012/11/04 18:59:02 kettenis Exp $

.if ${MACHINE} == "sparc64"

.PATH:	${.CURDIR}/../ldomd

PROG=	ldomctl
SRCS=	ldomctl.c ds.c mdesc.c util.c pri.c
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../ldomd -I${.CURDIR}
DEBUG= -g

.else

NOPROG=	yes

.endif

MAN=	ldomctl.8
MANSUBDIR=sparc64

.include <bsd.prog.mk>
