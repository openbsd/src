#	$OpenBSD: Makefile,v 1.12 2003/05/13 04:41:00 hin Exp $

.include <bsd.own.mk>

PROG=	login_krb5
SRCS=	login.c
MAN=	${PROG}.8
CFLAGS+=-Wall -Werror -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd

.if (${KERBEROS5:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=	${LIBKRB5} ${LIBASN1} ${LIBKRB} ${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} \
	${LIBDES}
LDADD+=	-lkrb5 -lasn1 -lcrypto -lcom_err -lkrb -lkafs -ldes
CFLAGS+=-DKRB5
.endif

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
