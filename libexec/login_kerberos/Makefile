#	$OpenBSD: Makefile,v 1.1 2000/12/12 02:31:36 millert Exp $

PROG=	login_kerberos
SRCS=	login_kerberos.c
MAN=	login_kerberos.8
DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.own.mk>	# For KERBEROS

CFLAGS+=-Wall
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
