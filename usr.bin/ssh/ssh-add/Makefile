#	$OpenBSD: Makefile,v 1.29 2019/12/13 19:09:10 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-add.c
SRCS+=	atomicio.c authfd.c cleanup.c fatal.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
SRCS+=	${SRCS_SK_CLIENT}

PROG=	ssh-add

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}
