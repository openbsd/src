#	$OpenBSD: Makefile,v 1.5 2012/11/04 18:14:09 kettenis Exp $

.if ${MACHINE} == "sparc64"

PROG=	ldomctl
SRCS=	ldomctl.c ds.c mdesc.c util.c
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../ldomd
DEBUG= -g

.else

NOPROG=	yes

.endif

MAN=	ldomctl.8
MANSUBDIR=sparc64

.include <bsd.prog.mk>
