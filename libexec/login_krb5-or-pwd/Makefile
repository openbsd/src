#	$OpenBSD: Makefile,v 1.12 2003/11/20 23:23:09 avsm Exp $

.include <bsd.own.mk>

PROG=	login_krb5-or-pwd
SRCS=	login.c login_passwd.c
MAN=	${PROG}.8
CFLAGS+=-DPASSWD -Wall -I${.CURDIR}/../login_passwd

.if (${KERBEROS5:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=  ${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBUTIL} ${LIBDES}
LDADD+=	-lkrb5 -lasn1 -lcrypto -lutil -ldes
CFLAGS+=-DKRB5
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
