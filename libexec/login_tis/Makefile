#	$OpenBSD: Makefile,v 1.1 2004/09/28 15:02:01 millert Exp $

PROG=	login_tis
MAN=	login_tis.8
CFLAGS+=-Wall
LDADD+=	-ldes
DPADD+=	${LIBDES}

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
