#	$OpenBSD: Makefile,v 1.8 2004/11/10 22:07:46 drahn Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

CPPFLAGS+= -DINET6 -DHAVE_POLL_H

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=${STATIC}

.include <bsd.prog.mk>
