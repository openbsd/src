#	$OpenBSD: Makefile,v 1.2 1999/12/30 16:19:32 itojun Exp $

PROG=	ping6
MAN=	ping6.8

CPPFLAGS+= -DINET6

BINOWN=	root
BINGRP=	bin
BINMODE=4555

CPPFLAGS+=-DNRL_GETADDRINFO

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
