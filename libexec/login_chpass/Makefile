#	$OpenBSD: Makefile,v 1.1 2000/12/12 02:30:44 millert Exp $


PROG=	login_chpass
SRCS=	login_chpass.c
MAN=	login_chpass.8

.PATH:  ${.CURDIR}/../../usr.bin/passwd

.include <bsd.own.mk>	# For KERBEROS and YP

CFLAGS+=-Wall
.if (${KERBEROS:L} == "yes")
CFLAGS+=-DKERBEROS
SRCS+=	new_pwd.c
DPADD+= ${LIBKRB} ${LIBDES}
LDADD+= -lkadm -lkrb -ldes -lcom_err
.endif
.if (${YP:L} == "yes")
CFLAGS+=-DYP
SRCS+= yp_passwd.c pwd_check.c pwd_gensalt.c
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
.endif

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
