#	$OpenBSD: Makefile,v 1.2 2019/10/31 21:23:19 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-sk-helper.c
SRCS+=	atomicio.c fatal.c cleanup.c msg.c
SRCS+=	${SRCS_KEY} ${SRCS_UTL} ${SRCS_BASE}

PROG=	ssh-sk-helper

BINDIR=	/usr/libexec
#MAN=	ssh-sk-helper.8
NOMAN=1

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
