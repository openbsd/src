# $OpenBSD: Makefile,v 1.1.1.1 1999/05/23 22:46:29 angelos Exp $

PROG=	keynote-sign

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/bin
LDADD=	-lkeynote -lm -lcrypto
MAN=	keynote-sign.1

KEYNOTEDIST= lib/libkeynote

KEYNOTE_SRC= ${.CURDIR}/../../${KEYNOTEDIST}

CFLAGS+= -DCRYPTO

SRCS=	keynote-sign.c

.PATH:	${KEYNOTE_SRC}

.include<bsd.prog.mk>

