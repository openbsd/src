LIB=	panel
SRCS=	panel.c
HDRS=	panel.h
MAN=	panel.3
CFLAGS+= -I${.CURDIR}/../libcurses -DHAVE_CONFIG_H

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
