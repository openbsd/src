# $OpenBSD: Makefile,v 1.13 2023/10/17 09:52:10 nicm Exp $

LIB=	panel
SRCS=	p_above.c p_below.c p_bottom.c p_delete.c p_hidden.c p_hide.c \
	p_move.c p_new.c p_replace.c p_show.c p_top.c p_update.c p_user.c \
	p_win.c panel.c
HDRS=	panel.h
CFLAGS+=-I${.CURDIR}/../libcurses -D_XOPEN_SOURCE_EXTENDED -DNDEBUG
MAN=	panel.3

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.own.mk>

afterinstall:
	-cd ${DESTDIR}${LIBDIR}; \
	for i in ${_LIBS}; do \
	    ln -f $$i `echo $$i | sed 's/panel/panelw/'`; \
	done

.include <bsd.lib.mk>
