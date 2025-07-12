#	$OpenBSD: Makefile,v 1.6 2025/07/12 05:28:33 miod Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-sk-helper.c
SRCS+=	atomicio.c fatal.c cleanup.c msg.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE} ${SRCS_SK}

PROG=	ssh-sk-helper

BINDIR=	/usr/libexec
MAN=	ssh-sk-helper.8

.include <bsd.prog.mk>

LDADD+= -lfido2 -lcbor -lusbhid -lz
DPADD+=	${LIBFIDO2} ${LIBCBOR} ${LIBUSBHID} ${LIBZ}

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
