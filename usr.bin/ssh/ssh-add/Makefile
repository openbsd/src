#	$OpenBSD: Makefile,v 1.28 2019/11/14 21:27:30 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-add.c
SRCS+=	atomicio.c authfd.c cleanup.c fatal.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}

PROG=	ssh-add

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}

LDADD+= -lfido2 -lcbor -lusbhid
DPADD+=	${LIBFIDO2} ${LIBCBOR} ${LIBUSBHID}
