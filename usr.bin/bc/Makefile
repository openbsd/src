#	$OpenBSD: Makefile,v 1.6 2011/03/07 08:11:15 otto Exp $

PROG=		bc
SRCS=		bc.y scan.l
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=
LDADD+=		-ledit -lcurses
DPADD+=		${LIBEDIT} ${LIBCURSES}


beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
