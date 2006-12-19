#	$OpenBSD: Makefile,v 1.9 2006/12/19 14:48:45 itojun Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=${STATIC}

.include <bsd.prog.mk>
