#	$OpenBSD: Makefile,v 1.7 2001/06/26 20:44:24 hin Exp $

PROG=	login_krb5
SRCS=	login.c login_krb5.c
MAN=	${PROG}.8
LDADD=  -lkrb5 -lasn1 -lcrypto -lcom_err -lkrb
CFLAGS+=-DKRB524 -DKRB5 -Wall -Werror -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
