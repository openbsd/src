#	$OpenBSD: Makefile,v 1.3 1999/07/15 19:21:11 deraadt Exp $

BINDIR?=	/usr/bin
PROG=	bc
SRCS=	bcwrap.c
NOMAN=

COPTS=-DWRAP=\"/usr/bin/gnubc\" -DBLOCK

.include <bsd.prog.mk>

