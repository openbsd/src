#	$OpenBSD: Makefile,v 1.2 2001/06/26 05:03:26 hin Exp $

PROG=	login_krb4
SRCS=	login_krb4.c login.c
MAN=	${PROG}.8
DPADD=  ${LIBUTIL}
LDADD=  -lutil -lkrb -lcrypto
CFLAGS+=-DKRB4 -Wall -Werror -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
