#	$OpenBSD: Makefile,v 1.4 2019/07/27 09:55:46 kn Exp $

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

MAN=	ldomd.8 ldom.conf.5
MANSUBDIR=sparc64

.include <bsd.prog.mk>
