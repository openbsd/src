#	$OpenBSD: Makefile,v 1.5 2010/10/17 22:54:37 schwarze Exp $

PROG=		bc
SRCS=		bc.y scan.l
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
