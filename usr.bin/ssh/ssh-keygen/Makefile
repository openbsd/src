#	$OpenBSD: Makefile,v 1.22 2003/07/28 09:49:56 djm Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keygen
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-keygen.1

SRCS=	ssh-keygen.c moduli.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
