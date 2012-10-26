#	$OpenBSD: Makefile,v 1.1 2012/10/26 18:26:13 kettenis Exp $

.PATH:	${.CURDIR}/../ldomctl

PROG=	ldomd
SRCS=	ldomd.c ds.c mdesc.c util.c
NOMAN=
CFLAGS+= -Wall
CFLAGS+= -I${.CURDIR}/../ldomctl
DEBUG= -g

.include <bsd.prog.mk>
