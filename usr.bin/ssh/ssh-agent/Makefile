#	$OpenBSD: Makefile,v 1.15 2001/01/29 01:58:22 niklas Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-agent.1

SRCS=	ssh-agent.c log-client.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
