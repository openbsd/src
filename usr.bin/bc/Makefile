#	$OpenBSD: Makefile,v 1.3 2006/05/27 22:53:44 deraadt Exp $

PROG=		bc
SRCS=		bc.y scan.l
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=
CLEANFILES+=	bc.c y.tab.h scan.c lex.yy.c

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

# These get installed verbatim
.if make(install)
SUBDIR+= USD.doc
.endif

.include <bsd.prog.mk>
