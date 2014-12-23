#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:03 deraadt Exp $

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

.PATH:  ${.CURDIR}/../../../sbin/ping6
.include <bsd.prog.mk>
