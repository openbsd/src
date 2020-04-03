#	$OpenBSD: Makefile,v 1.31 2020/04/03 02:26:56 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-add.c
SRCS+=	authfd.c cleanup.c fatal.c readpass.c utf8.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
SRCS+=	${SRCS_SK_CLIENT}

PROG=	ssh-add

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}
