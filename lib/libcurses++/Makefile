#	$OpenBSD: Makefile,v 1.6 2004/09/18 23:23:50 deraadt Exp $

# Uncomment this to enable tracing in libcurses
#CURSESTRACE=-DTRACE

NOLINT=

LIB=    curses++
SRCS=	cursesapp.cc cursesf.cc cursesm.cc cursesmain.cc cursesp.cc \
	cursesw.cc cursespad.cc cursslk.cc
HEADERS=cursesapp.h cursesf.h cursesm.h cursesp.h cursesw.h etip.h cursslk.h
CLEANFILES+=demo

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../libcurses ${CURSESTRACE}

includes:
	@test -d ${DESTDIR}/usr/include/g++ || \
	    install -d -o root -g bin -m 755 ${DESTDIR}/usr/include/g++
	@for i in ${HEADERS}; do \
	    cmp -s ${DESTDIR}/usr/include/g++/$$i ${.CURDIR}/$$i || \
		${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
		${.CURDIR}/$$i ${DESTDIR}/usr/include/g++/$$i; \
	done

demo:
	${CXX} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.CURDIR}/demo.cc -lform -lmenu -lpanel -lcurses -lcurses++ ${LDADD}

.include <bsd.lib.mk>
