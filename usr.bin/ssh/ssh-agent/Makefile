#	$OpenBSD: Makefile,v 1.29 2018/07/09 21:56:06 markus Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ssh-pkcs11-client.c
SRCS+=	atomicio.c compat.c fatal.c key.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt

BINMODE?=2555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

