#	$OpenBSD: Makefile,v 1.11 2019/12/13 19:09:10 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-pkcs11-helper.c ${SRCS_PKCS11}
SRCS+=	atomicio.c compat.c fatal.c readpass.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE} ${SRCS_SK_CLIENT}

PROG=	ssh-pkcs11-helper

BINDIR=	/usr/libexec
MAN=	ssh-pkcs11-helper.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
