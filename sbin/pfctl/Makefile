#
#	$OpenBSD: Makefile,v 1.4 2001/07/16 21:09:36 markus Exp $

CLEANFILES+=parse.c y.tab.h

PROG=	pfctl
SRCS=	pfctl.c parse.c pfctl_parser.c
MAN=	pfctl.8

DEBUG=-g

CFLAGS+= -Wall

.include <bsd.prog.mk>
