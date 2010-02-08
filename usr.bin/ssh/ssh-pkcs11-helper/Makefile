#	$OpenBSD: Makefile,v 1.1 2010/02/08 10:50:20 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-pkcs11-helper
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
NOMAN=	yes

SRCS=	ssh-pkcs11-helper.c ssh-pkcs11.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
