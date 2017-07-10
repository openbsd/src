#	$OpenBSD: Makefile,v 1.23 2017/07/10 14:09:59 espie Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keygen
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

SRCS=	ssh-keygen.c moduli.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
