#	$OpenBSD: Makefile,v 1.2 2008/12/11 23:04:45 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	newaliases
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	newaliases.8

SRCS=	newaliases.c aliases.c map.c log.c util.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
