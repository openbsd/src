#	$OpenBSD: Makefile,v 1.5 2006/03/09 19:14:10 millert Exp $

PROG=	login_passwd
MAN=	login_passwd.8
SRCS=	login.c login_passwd.c pwd_gensalt.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall -DPASSWD
.PATH:	${.CURDIR}/../../usr.bin/passwd

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
