#	$OpenBSD: Makefile,v 1.16 2003/11/20 23:23:09 avsm Exp $

.include <bsd.own.mk>

PROG=	login_krb5
SRCS=	login.c
MAN=	${PROG}.8
CFLAGS+=-Wall -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd

.if (${KERBEROS5:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=	${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBDES}
LDADD+=	-lkrb5 -lasn1 -lcrypto -ldes
CFLAGS+=-DKRB5
.endif

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
