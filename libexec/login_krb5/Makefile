#	$OpenBSD: Makefile,v 1.20 2013/08/06 19:05:57 miod Exp $

.include <bsd.own.mk>

PROG=	login_krb5
SRCS=	login.c
MAN=	${PROG}.8
CFLAGS+=-Wall -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd

.if (${KERBEROS5:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=	${LIBKRB5} ${LIBASN1} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lasn1 -lcrypto
LDADD+= -lwind -lroken -lcom_err -lheimbase
CFLAGS+=-DKRB5
.if ${MACHINE} == "vax"
LDFLAGS+=-Wl,--no-keep-memory
.endif
.endif

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
