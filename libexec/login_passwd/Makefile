#	$OpenBSD: Makefile,v 1.1 2000/12/12 02:33:44 millert Exp $

PROG=	login_passwd
MAN=	login_passwd.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
