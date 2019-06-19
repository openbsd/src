#	$OpenBSD: Makefile,v 1.27 2018/07/25 17:12:35 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keygen.c moduli.c uuencode.c
SRCS+=	atomicio.c authfd.c cleanup.c dns.c fatal.c hmac.c hostfile.c \
	readpass.c utf8.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL} \
	${SRCS_PKCS11}

PROG=	ssh-keygen

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

