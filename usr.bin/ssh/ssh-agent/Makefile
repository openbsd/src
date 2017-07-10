#	$OpenBSD: Makefile,v 1.25 2017/07/10 14:09:59 espie Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt

BINMODE?=2555

BINDIR=	/usr/bin

SRCS=	ssh-agent.c ssh-pkcs11-client.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
