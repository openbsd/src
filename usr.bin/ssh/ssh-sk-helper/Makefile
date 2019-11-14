#	$OpenBSD: Makefile,v 1.4 2019/11/14 21:27:31 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-sk-helper.c
SRCS+=	atomicio.c fatal.c cleanup.c msg.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE}

PROG=	ssh-sk-helper

BINDIR=	/usr/libexec
MAN=	ssh-sk-helper.8

.include <bsd.prog.mk>

LDADD+= -lfido2 -lcbor -lusbhid
DPADD+=	${LIBFIDO2} ${LIBCBOR} ${LIBUSBHID}

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
