#	$OpenBSD: Makefile,v 1.2 2002/11/21 22:26:32 millert Exp $

PROG=	login_radius
SRCS=	login_radius.c raddauth.c
MAN=	login_radius.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	_radius
BINMODE=2555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
