#	$OpenBSD: Makefile,v 1.4 2000/06/20 20:00:03 itojun Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

CPPFLAGS+= -DINET6

BINOWN=	root
BINGRP=	bin
BINMODE=4555

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
