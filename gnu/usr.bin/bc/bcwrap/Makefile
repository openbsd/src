#	$OpenBSD: Makefile,v 1.5 1999/07/22 02:35:02 deraadt Exp $

BINDIR?=	/usr/bin
PROG=	bc
SRCS=	wrap-anything.c
NOMAN=

COPTS+=-DWRAP=\"/usr/bin/gnubc\" -DBLOCK

.include <bsd.prog.mk>

