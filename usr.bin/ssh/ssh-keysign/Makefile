#	$OpenBSD: Makefile,v 1.4 2002/07/01 08:05:48 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keysign
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

SRCS=	ssh-keysign.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lz
DPADD+=	${LIBCRYPTO} ${LIBZ}
