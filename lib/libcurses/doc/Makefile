#	$OpenBSD: Makefile,v 1.1 1999/01/18 19:25:56 millert Exp $

FILES=	hackguide.html ncurses-intro.html

all:
	@echo nothing to do

install:
	for f in ${FILES}; do \
		${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
		    ${.CURDIR}/$$f ${DESTDIR}/usr/share/doc/html/curses/$$f; \
	done

.include <bsd.own.mk>
