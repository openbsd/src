#	$OpenBSD: Makefile,v 1.1 2001/07/08 17:56:33 millert Exp $

PROG=	login_radius
SRCS=	login_radius.c raddauth.c
MAN=	login_radius.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
