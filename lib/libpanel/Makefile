LIB=	panel
SRCS=	p_above.c p_below.c p_bottom.c p_delete.c p_hidden.c p_hide.c p_move.c \
	p_new.c p_replace.c p_show.c p_top.c p_update.c p_user.c p_win.c panel.c
HDRS=	panel.h
MAN=	panel.3
CFLAGS+= -I${.CURDIR}/../libcurses -DHAVE_CONFIG_H

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
