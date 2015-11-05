#	$OpenBSD: Makefile,v 1.2 2015/11/05 22:08:44 nicm Exp $

.PATH:	${.CURDIR}/..

PROG=	lesskey
SRCS=	lesskey.c version.c

MAN=	lesskey.1
BINDIR=	/usr/bin

CFLAGS+=-I${.CURDIR}/..

CDIAGFLAGS+=-Wall -W

.include <bsd.prog.mk>
