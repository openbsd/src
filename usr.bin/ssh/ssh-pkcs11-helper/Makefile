#	$OpenBSD: Makefile,v 1.8 2018/07/25 17:12:35 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-pkcs11-helper.c ssh-pkcs11.c
SRCS+=	atomicio.c compat.c fatal.c readpass.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE}

PROG=	ssh-pkcs11-helper

BINDIR=	/usr/libexec
MAN=	ssh-pkcs11-helper.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
