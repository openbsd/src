#	$OpenBSD: Makefile,v 1.10 2002/03/05 00:09:32 deraadt Exp $

.include <bsd.own.mk>

PROG=	login_krb5
SRCS=	login.c
MAN=	${PROG}.8
CFLAGS+=-Wall -Werror -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd

.if (${KERBEROS:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=	${LIBKRB} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lasn1 -lcrypto -lcom_err -lkrb -lkafs -ldes
CFLAGS+=-DKRB524 -DKRB5
.endif

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
