#	$OpenBSD: Makefile,v 1.33 2025/07/24 05:44:55 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-add.c
SRCS+=	authfd.c cleanup.c fatal.c readpass.c utf8.c hostfile.c hmac.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
SRCS+=	${SRCS_SK_CLIENT} ${SRCS_PKCS11_CLIENT}

PROG=	ssh-add

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}
