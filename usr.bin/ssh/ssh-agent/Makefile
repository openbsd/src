#	$OpenBSD: Makefile,v 1.30 2018/07/11 18:53:29 markus Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ssh-pkcs11-client.c
SRCS+=	atomicio.c compat.c fatal.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt

BINMODE?=2555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

