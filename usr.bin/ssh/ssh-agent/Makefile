#	$OpenBSD: Makefile,v 1.32 2019/09/06 05:59:41 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ${SRCS_PKCS11_CLIENT}
SRCS+=	atomicio.c compat.c fatal.c readpass.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt
BINMODE=2555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

