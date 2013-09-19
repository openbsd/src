#	$OpenBSD: Makefile,v 1.7 2013/09/19 16:12:00 otto Exp $

PROG=		bc
SRCS=		bc.y scan.l tty.c
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=
LDADD+=		-ledit -lcurses
DPADD+=		${LIBEDIT} ${LIBCURSES}


beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
