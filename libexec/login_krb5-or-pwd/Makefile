#	$OpenBSD: Makefile,v 1.9 2003/05/17 00:45:02 hin Exp $

.include <bsd.own.mk>

PROG=	login_krb5-or-pwd
SRCS=	login.c login_passwd.c
MAN=	${PROG}.8
CFLAGS+=-DPASSWD -Wall -Werror -I${.CURDIR}/../login_passwd

.if (${KERBEROS5:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=  ${LIBKRB} ${LIBCRYPTO} ${LIBUTIL} ${LIBKRB5} ${LIBKASN1} \
	${LIBCOM_ERR} ${LIBKAFS} ${LIBDES}
LDADD+=	-lkrb5 -lasn1 -lcrypto -lcom_err -lutil -ldes
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
