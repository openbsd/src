#	$OpenBSD: Makefile,v 1.8 2010/07/01 20:04:10 tedu Exp $

DOCDIR=	/usr/share/doc/psd
FILES=	Makefile
SUBDIR=
.if exists(12.make)
SUBDIR+= 12.make
.endif
.if exists(18.gprof)
SUBDIR+= 18.gprof
.endif
.if exists(19.curses)
SUBDIR+= 19.curses
.endif

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
