#	$OpenBSD: Makefile,v 1.1 2005/08/02 11:16:44 espie Exp $

COPTS+=-Os
# Define NO_CONV to disable character conversion
CFLAGS+=-DNO_CONV
LDFLAGS=-static

PROG=	dd
SRCS=	args.c conv.c dd.c misc.c position.c
MAN=

.PATH:	${.CURDIR}/../../../bin/dd

.include <bsd.prog.mk>
