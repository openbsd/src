LIB=	menu
SRCS=	m_attribs.c m_cursor.c m_driver.c m_format.c m_global.c m_hook.c \
	m_item_cur.c m_item_nam.c m_item_new.c m_item_opt.c m_item_use.c \
	m_item_val.c m_item_vis.c m_items.c m_new.c m_opts.c m_post.c \
	m_userptr.c m_win.c
HDRS=	menu.h eti.h
CFLAGS+=-I. -I${.CURDIR} -DHAVE_EXTERN_ERRNO
MAN=	menu.3 menu_driver.3 menu_items.3 menu_opts.3 menu_userptr.3 \
	menu_attribs.3 menu_format.3 menu_mark.3 menu_pattern.3 menu_win.3 \
	menu_cursor.3 menu_hook.3 menu_new.3 menu_post.3

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
