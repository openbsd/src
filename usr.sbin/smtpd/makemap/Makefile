#	$OpenBSD: Makefile,v 1.1 2008/12/03 21:20:20 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	makemap
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	makemap.8

SRCS=	makemap.c aliases.c map.c log.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
