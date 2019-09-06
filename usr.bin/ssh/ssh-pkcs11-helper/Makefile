#	$OpenBSD: Makefile,v 1.9 2019/09/06 05:59:41 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-pkcs11-helper.c ${SRCS_PKCS11}
SRCS+=	atomicio.c compat.c fatal.c readpass.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE}

PROG=	ssh-pkcs11-helper

BINDIR=	/usr/libexec
MAN=	ssh-pkcs11-helper.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
