#	$OpenBSD: Makefile,v 1.1 2001/06/25 00:07:07 hin Exp $

PROG=	login_krb5
SRCS=	login_krb5.c
MAN=	${PROG}.8
NOMAN=
LDADD=  -lkrb5 -lasn1 -lcrypto -lcom_err -lkrb

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
