#	$OpenBSD: Makefile,v 1.3 2001/07/02 14:07:06 millert Exp $

.include <bsd.own.mk>

PROG=	login_krb4
SRCS=	login.c
MAN=	${PROG}.8
CFLAGS+=-Wall -Werror -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd

.if (${KERBEROS:L} == "yes")
SRCS+=	login_krb4.c
DPADD+=	${LIBKRB} ${LIBCRYPTO}
LDADD+=	-lkrb -lcrypto
CFLAGS+=-DKRB4
.endif

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
