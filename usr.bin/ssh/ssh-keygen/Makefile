#	$OpenBSD: Makefile,v 1.20 2001/06/26 17:40:43 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keygen
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-keygen.1

SRCS=	ssh-keygen.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

#LDADD+=	-lsectok
