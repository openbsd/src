#	$OpenBSD: Makefile,v 1.21 2017/07/10 14:09:59 espie Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+= ${LIBCRYPTO}
