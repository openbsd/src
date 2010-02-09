#	$OpenBSD: Makefile,v 1.2 2010/02/09 08:25:32 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-pkcs11-helper
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	ssh-pkcs11-helper.8

SRCS=	ssh-pkcs11-helper.c ssh-pkcs11.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
