#	$OpenBSD: Makefile,v 1.3 2003/03/28 16:03:19 millert Exp $

PROG=	login_passwd
MAN=	login_passwd.8
SRCS=	login.c login_passwd.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall -DPASSWD

BINOWN=	root
BINGRP=	_shadow
BINMODE=2555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
