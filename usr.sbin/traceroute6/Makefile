#	$OpenBSD: Makefile,v 1.3 2000/10/07 21:47:31 itojun Exp $

PROG=	traceroute6

BINOWN=	root
BINGRP=	bin
BINMODE=4555

CPPFLAGS+=-DINET6 -DHAVE_POLL

MAN=	traceroute6.8

.include <bsd.prog.mk>
