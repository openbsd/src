#	$OpenBSD: Makefile,v 1.3 2000/03/23 11:26:17 hugh Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

CPPFLAGS+= -DINET6

BINOWN=	root
BINGRP=	bin
BINMODE=4555

CPPFLAGS+=-DNRL_GETADDRINFO

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
