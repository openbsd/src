# $OpenBSD: Makefile,v 1.1.1.1 1999/05/23 22:47:47 angelos Exp $

PROG=	keynote-sigver

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/bin
LDADD=	-lkeynote -lm -lcrypto
MAN=	keynote-sigver.1

KEYNOTEDIST= lib/libkeynote

KEYNOTE_SRC= ${.CURDIR}/../../${KEYNOTEDIST}

CFLAGS+= -DCRYPTO

SRCS=	keynote-sigver.c

.PATH:	${KEYNOTE_SRC}

.include<bsd.prog.mk>

