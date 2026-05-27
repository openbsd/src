#	$OpenBSD: Makefile,v 1.44 2026/05/27 13:57:26 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ${SRCS_PKCS11_CLIENT}
SRCS+=	compat.c fatal.c readpass.c utf8.c misc-agent.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
SRCS+=	${SRCS_SK_CLIENT}

PROG=	ssh-agent
RELINK=./${PROG} -V # test it works
BINOWN=	root
BINGRP=	_sshagnt
BINMODE=2511

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
