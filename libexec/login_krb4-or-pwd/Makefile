#	$OpenBSD: Makefile,v 1.2 2001/06/26 05:03:27 hin Exp $

PROG=	login_krb4-or-pwd
SRCS=	login_krb4.c login_passwd.c login.c
MAN=	login_krb4-or-pwd.8
DPADD=  ${LIBUTIL}
LDADD=  -lutil -lkrb -lcrypto
CFLAGS+=-Wall -Werror -DPASSWD -DKRB4 -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_krb4 ${.CURDIR}/../login_passwd

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
