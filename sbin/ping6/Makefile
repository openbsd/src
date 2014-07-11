#	$OpenBSD: Makefile,v 1.11 2014/07/11 15:28:27 florian Exp $

PROG=	ping6

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

BINOWN=	root
BINGRP=	bin
BINMODE=4555

.include <bsd.prog.mk>
