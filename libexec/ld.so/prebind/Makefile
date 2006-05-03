#	$OpenBSD: Makefile,v 1.1 2006/05/03 16:10:52 drahn Exp $

SRCS=	prebind.c sod.c debug.c objarray.c
.if (${MACHINE} == "mips64") 
NOPROG=
.else
PROG=	prebind
.endif
MAN=	prebind.8

BINDIR=	/usr/sbin

CFLAGS += -Wall -ggdb
CFLAGS += -I${.CURDIR}/..

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
