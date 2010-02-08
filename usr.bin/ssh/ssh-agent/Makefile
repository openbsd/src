#	$OpenBSD: Makefile,v 1.23 2010/02/08 10:50:20 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt

BINMODE?=2555

BINDIR=	/usr/bin
MAN=	ssh-agent.1

SRCS=	ssh-agent.c
#SRCS+=	ssh-pkcs11-client.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
