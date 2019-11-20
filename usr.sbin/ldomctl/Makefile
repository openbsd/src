#	$OpenBSD: Makefile,v 1.10 2019/11/20 18:11:28 kn Exp $

.if ${MACHINE} == "sparc64"

.PATH:	${.CURDIR}/../ldomd

PROG=	ldomctl
SRCS=	ldomctl.c ds.c mdesc.c util.c mdstore.c pri.c config.c parse.y
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../ldomd -I${.CURDIR}

.else

NOPROG=	yes

.endif

MAN=	ldomctl.8 ldom.conf.5
MANSUBDIR=sparc64

.include <bsd.prog.mk>
