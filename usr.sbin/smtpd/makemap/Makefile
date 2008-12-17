#	$OpenBSD: Makefile,v 1.3 2008/12/17 22:59:39 jacekm Exp $

.PATH:		${.CURDIR}/..

PROG=	makemap
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	makemap.8

LINKS=	${BINDIR}/makemap ${BINDIR}/newaliases
MLINKS=	makemap.8 newaliases.8

SRCS=	makemap.c aliases.c map.c log.c util.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
