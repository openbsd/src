#	$OpenBSD: Makefile,v 1.6 2002/05/26 13:02:17 itojun Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

CPPFLAGS+= -DINET6

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=-static

.include <bsd.prog.mk>
