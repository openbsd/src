#	$OpenBSD: Makefile,v 1.24 2010/02/09 08:55:31 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt

BINMODE?=2555

BINDIR=	/usr/bin
MAN=	ssh-agent.1

SRCS=	ssh-agent.c ssh-pkcs11-client.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
