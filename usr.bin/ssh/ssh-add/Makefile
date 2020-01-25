#	$OpenBSD: Makefile,v 1.30 2020/01/25 23:02:14 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-add.c
SRCS+=	authfd.c cleanup.c fatal.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
SRCS+=	${SRCS_SK_CLIENT}

PROG=	ssh-add

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}
