#	$OpenBSD: Makefile,v 1.1 2001/06/25 15:51:37 hin Exp $

PROG=	login_krb5-or-pwd
SRCS=	login_krb5.c
MAN=	${PROG}.8
LDADD=  -lkrb5 -lasn1 -lcrypto -lcom_err -lkrb -lutil
CFLAGS+=-DKRB4 -DPASSWD -Wall -Werror
.PATH:	${.CURDIR}/../login_krb5

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
