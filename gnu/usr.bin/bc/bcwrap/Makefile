#	$OpenBSD: Makefile,v 1.6 2000/01/14 17:46:36 millert Exp $

BINDIR?=	/usr/bin
PROG=	bc
SRCS=	wrap-anything.c
NOMAN=

CFLAGS+=-DWRAP=\"/usr/bin/gnubc\" -DBLOCK

.include <bsd.prog.mk>

