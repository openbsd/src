#	$OpenBSD: Makefile,v 1.4 2001/06/25 15:48:16 hin Exp $

PROG=	login_krb5
MAN=	${PROG}.8
LDADD=  -lkrb5 -lasn1 -lcrypto -lcom_err -lkrb
CFLAGS+=-DKRB4 -Wall -Werror

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
