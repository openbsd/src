#	$OpenBSD: Makefile,v 1.11 2004/04/25 18:40:42 beck Exp $

PROG=	authpf
MAN=	authpf.8
BINOWN= root
BINGRP= authpf
BINMODE= 6555
SRCS=	authpf.c
CFLAGS+= -Wall

LDADD+= -lm
DPADD+= ${LIBM}

.include <bsd.prog.mk>
