#	$OpenBSD: Makefile,v 1.2 2008/09/15 18:13:36 kurt Exp $

COPTS+=-Os
# Define NO_CONV to disable character conversion
CFLAGS+=-DNO_CONV
LDSTATIC=-static

PROG=	dd
SRCS=	args.c conv.c dd.c misc.c position.c
MAN=

.PATH:	${.CURDIR}/../../../bin/dd

.include <bsd.prog.mk>
