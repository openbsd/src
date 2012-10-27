#	$OpenBSD: Makefile,v 1.2 2012/10/27 18:34:03 kettenis Exp $

.PATH:	${.CURDIR}/../ldomctl

PROG=	ldomd
SRCS=	ldomd.c ds.c mdesc.c util.c var-config.c
NOMAN=
CFLAGS+= -Wall
CFLAGS+= -I${.CURDIR}/../ldomctl
DEBUG= -g

.include <bsd.prog.mk>
