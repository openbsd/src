#	$OpenBSD: Makefile,v 1.5 2002/07/03 14:21:05 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keysign
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

SRCS=	ssh-keysign.c readconf.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lz
DPADD+=	${LIBCRYPTO} ${LIBZ}
