#	$OpenBSD: Makefile,v 1.1 2001/06/24 23:31:45 hin Exp $

PROG=	login_krb4-or-pwd
SRCS=	login_krb4.c
MAN=	login_krb4-or-pwd.8
DPADD=  ${LIBUTIL}
LDADD=  -lutil
.PATH:	${.CURDIR}/../login_krb4

.include <bsd.own.mk>	# For KERBEROS

CFLAGS+=-Wall -DPASSWD
.if (${KERBEROS:L} == "yes")
CFLAGS+=-DKERBEROS
SRCS+= klogin.c
DPADD+= ${LIBKRB} ${LIBDES}
LDADD+= -lkrb -ldes
.endif

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
