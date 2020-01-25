#	$OpenBSD: Makefile,v 1.38 2020/01/25 23:02:14 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keygen.c ${SRCS_MODULI}
SRCS+=	authfd.c cleanup.c dns.c fatal.c hmac.c hostfile.c \
	readpass.c utf8.c sshsig.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL} \
	${SRCS_PKCS11} ${SRCS_SK_CLIENT}

PROG=	ssh-keygen

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
