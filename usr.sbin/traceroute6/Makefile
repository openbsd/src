#	$OpenBSD: Makefile,v 1.4 2002/05/12 17:14:04 deraadt Exp $

PROG=	traceroute6

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=-static

CPPFLAGS+=-DINET6 -DHAVE_POLL

MAN=	traceroute6.8

.include <bsd.prog.mk>
