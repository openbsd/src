#	$OpenBSD: Makefile,v 1.2 2001/06/25 01:02:00 hin Exp $

PROG=	login_krb5
SRCS=	login_krb5.c
MAN=	${PROG}.8
NOMAN=
LDADD=  -lkrb5 -lasn1 -lcrypto -lcom_err -lkrb
CFLAGS+=-DKRB4

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
