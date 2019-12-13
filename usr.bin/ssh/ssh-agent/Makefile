#	$OpenBSD: Makefile,v 1.36 2019/12/13 19:09:10 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ${SRCS_PKCS11_CLIENT}
SRCS+=	atomicio.c compat.c fatal.c readpass.c
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
