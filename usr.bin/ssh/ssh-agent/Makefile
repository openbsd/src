#	$OpenBSD: Makefile,v 1.20 2001/06/26 17:40:42 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-agent.1

SRCS=	ssh-agent.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

#LDADD+=	-lsectok
