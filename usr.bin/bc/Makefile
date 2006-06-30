#	$OpenBSD: Makefile,v 1.4 2006/06/30 19:02:28 otto Exp $

PROG=		bc
SRCS=		bc.y scan.l
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

# These get installed verbatim
.if make(install)
SUBDIR+= USD.doc
.endif

.include <bsd.prog.mk>
