#	$OpenBSD: Makefile,v 1.5 2004/11/10 22:07:46 drahn Exp $

PROG=	traceroute6

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=${STATIC}

CPPFLAGS+=-DINET6 -DHAVE_POLL

MAN=	traceroute6.8

.include <bsd.prog.mk>
