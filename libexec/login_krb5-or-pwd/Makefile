#	$OpenBSD: Makefile,v 1.6 2002/03/05 00:09:32 deraadt Exp $

.include <bsd.own.mk>

PROG=	login_krb5-or-pwd
SRCS=	login.c login_passwd.c
MAN=	${PROG}.8
CFLAGS+=-DPASSWD -Wall -Werror -I${.CURDIR}/../login_passwd

.if (${KERBEROS:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=  ${LIBKRB} ${LIBCRYPTO} ${LIBUTIL}
LDADD+=	-lkrb5 -lasn1 -lcrypto -lcom_err -lkrb -lutil -lkafs -ldes
CFLAGS+=-DKRB524 -DKRB5
.PATH:	${.CURDIR}/../login_passwd ${.CURDIR}/../login_krb5
.else
DPADD+=  ${LIBUTIL}
LDADD+=  -lutil
.PATH:	${.CURDIR}/../login_passwd
.endif

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
