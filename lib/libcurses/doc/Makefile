#	$OpenBSD: Makefile,v 1.2 1999/01/20 17:29:30 millert Exp $

FILES=	hackguide.html ncurses-intro.html

all:
	@echo nothing to do

install:
	test -d ${DESTDIR}/usr/share/doc/html/curses || ${INSTALL} -d -m 755 \
	    -o $(BINOWN) -g $(BINGRP) ${DESTDIR}/usr/share/doc/html/curses
	for f in ${FILES}; do \
		${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
		    ${.CURDIR}/$$f ${DESTDIR}/usr/share/doc/html/curses/$$f; \
	done

.include <bsd.own.mk>
