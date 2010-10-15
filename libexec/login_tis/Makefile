#	$OpenBSD: Makefile,v 1.2 2010/10/15 10:18:42 jsg Exp $

PROG=	login_tis
MAN=	login_tis.8
CFLAGS+=-Wall
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
