#	$OpenBSD: Makefile,v 1.10 2017/07/19 12:50:32 espie Exp $

PROG=		bc
SRCS=		bc.y scan.l tty.c dc.c bcode.c inout.c mem.c stack.c
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=
LDADD+=		-ledit -lcurses -lcrypto
DPADD+=		${LIBEDIT} ${LIBCURSES} ${LIBCRYPTO}

.PATH:		${.CURDIR}/../dc

bc.c bc.h: bc.y
	${YACC} -o bc.c -d ${.CURDIR}/bc.y

scan.o: bc.h

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
