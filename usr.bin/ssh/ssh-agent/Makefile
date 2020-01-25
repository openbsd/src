#	$OpenBSD: Makefile,v 1.37 2020/01/25 23:02:14 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ${SRCS_PKCS11_CLIENT}
SRCS+=	compat.c fatal.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
SRCS+=	${SRCS_SK_CLIENT}

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt
BINMODE=2555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
