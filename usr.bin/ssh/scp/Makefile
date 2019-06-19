#	$OpenBSD: Makefile,v 1.21 2018/07/25 17:12:35 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	scp.c
SRCS+=	atomicio.c cleanup.c fatal.c progressmeter.c utf8.c
SRCS+=	${SRCS_BASE}

PROG=	scp

BINDIR=	/usr/bin

.include <bsd.prog.mk>
