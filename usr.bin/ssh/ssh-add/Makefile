#	$OpenBSD: Makefile,v 1.26 2019/10/31 21:19:56 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-add.c
SRCS+=	atomicio.c authfd.c cleanup.c fatal.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL} ${SRCS_SK}

PROG=	ssh-add

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}
