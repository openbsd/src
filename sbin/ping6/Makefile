#	$OpenBSD: Makefile,v 1.7 2002/10/25 02:25:43 itojun Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

CPPFLAGS+= -DINET6 -DHAVE_POLL_H

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=-static

.include <bsd.prog.mk>
