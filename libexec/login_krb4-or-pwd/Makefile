#	$OpenBSD: Makefile,v 1.5 2002/05/11 00:20:20 espie Exp $

.include <bsd.own.mk>

PROG=	login_krb4-or-pwd
SRCS=	login.c login_passwd.c
MAN=	${PROG}.8
CFLAGS+=-DPASSWD -Wall -Werror -I${.CURDIR}/../login_passwd

.if (${KERBEROS:L} == "yes")
SRCS+=	login_krb4.c 
DPADD+=  ${LIBKRB} ${LIBCRYPTO} ${LIBUTIL} ${LIBDES}
LDADD+=	-lkrb -lcrypto -lutil -ldes
CFLAGS+=-DKRB4
.PATH:	${.CURDIR}/../login_passwd ${.CURDIR}/../login_krb4
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
