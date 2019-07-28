#	$OpenBSD: Makefile,v 1.5 2019/07/28 15:30:45 kn Exp $

.if ${MACHINE} == "sparc64"

.PATH:	${.CURDIR}/../ldomctl

PROG=	ldomd
SRCS=	ldomd.c ds.c mdesc.c util.c var-config.c
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../ldomctl
DEBUG=	-g

.else

NOPROG=	yes

.endif

MAN=	ldomd.8
MANSUBDIR=sparc64

.include <bsd.prog.mk>
