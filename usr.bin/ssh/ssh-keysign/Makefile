#	$OpenBSD: Makefile,v 1.1 2002/05/23 19:24:31 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keysign
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/libexec
NOMAN=yes

SRCS=	ssh-keysign.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
