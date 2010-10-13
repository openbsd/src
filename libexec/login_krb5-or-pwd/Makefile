#	$OpenBSD: Makefile,v 1.14 2010/10/13 08:14:22 jsg Exp $

.include <bsd.own.mk>

PROG=	login_krb5-or-pwd
SRCS=	login.c login_passwd.c pwd_gensalt.c
MAN=	${PROG}.8
CFLAGS+=-DPASSWD -Wall -I${.CURDIR}/../login_passwd

.if (${KERBEROS5:L} == "yes")
SRCS+=	login_krb5.c
DPADD+=  ${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBUTIL}
LDADD+=	-lkrb5 -lasn1 -lcrypto -lutil
CFLAGS+=-DKRB5
.PATH:	${.CURDIR}/../login_passwd ${.CURDIR}/../login_krb5 ${.CURDIR}/../../usr.bin/passwd
.else
DPADD+=  ${LIBUTIL}
LDADD+=  -lutil
.PATH:	${.CURDIR}/../login_passwd ${.CURDIR}/../../usr.bin/passwd
.endif

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
