# $OpenBSD: Makefile,v 1.9 2010/09/06 17:26:17 nicm Exp $

LIB=	panel
WANTLINT=
SRCS=	p_above.c p_below.c p_bottom.c p_delete.c p_hidden.c p_hide.c p_move.c \
	p_new.c p_replace.c p_show.c p_top.c p_update.c p_user.c p_win.c panel.c
HDRS=	panel.h
MAN=	panel.3
MLINKS=	panel.3 new_panel.3 panel.3 bottom_panel.3 panel.3 top_panel.3 \
        panel.3 show_panel.3 panel.3 update_panels.3 panel.3 \
        hide_panel.3 panel.3 panel_window.3 panel.3 replace_panel.3 \
        panel.3 move_panel.3 panel.3 panel_hidden.3 panel.3 \
        panel_above.3 panel.3 panel_below.3 panel.3 set_panel_userptr.3 \
        panel.3 panel_userptr.3 panel.3 del_panel.3
CFLAGS+= -I${.CURDIR}/../libcurses

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
.if (${DEBUGLIBS:L} == "yes")
	-cd ${DESTDIR}${LIBDIR}/debug; \
	    ln -f lib${LIB}.a lib`echo ${LIB} | sed 's/panel/panelw/'`.a
.endif

.include <bsd.lib.mk>
