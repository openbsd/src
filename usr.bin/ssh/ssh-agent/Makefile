#	$OpenBSD: Makefile,v 1.34 2019/10/31 21:23:19 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ${SRCS_PKCS11_CLIENT}
SRCS+=	atomicio.c compat.c fatal.c readpass.c msg.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt
BINMODE=2555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

