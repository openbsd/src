#	$OpenBSD: Makefile,v 1.1 2008/12/03 21:13:18 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	newaliases
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	newaliases.8

SRCS=	newaliases.c aliases.c map.c log.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
