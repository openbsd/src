#	$OpenBSD: Makefile,v 1.20 2018/01/08 15:37:22 markus Exp $

.PATH:		${.CURDIR}/..

SRCS=	scp.c
SRCS+=	atomicio.c cleanup.c fatal.c progressmeter.c utf8.c
SRCS+=	${SRCS_BASE}

PROG=	scp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>
