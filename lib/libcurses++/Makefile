#	$OpenBSD: Makefile,v 1.1 1999/05/09 00:21:14 millert Exp $

# Uncomment this to enable tracing in libcurses
#CURSESTRACE=-DTRACE

LIB=    curses++
SRCS=	cursesapp.cc cursesf.cc cursesm.cc cursesmain.cc cursesp.cc \
	cursesw.cc cursslk.cc
HEADERS=cursesapp.h cursesf.h cursesm.h cursesp.h cursesw.h etip.h cursslk.h

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../libcurses ${CURSESTRACE}

includes:
	@for i in ${HEADERS}; do \
	    cmp -s ${DESTDIR}/usr/include/g++/$$i ${.CURDIR}/$$i || \
		${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
		${.CURDIR}/$$i ${DESTDIR}/usr/include/g++/$$i; \
	done

.include <bsd.lib.mk>
