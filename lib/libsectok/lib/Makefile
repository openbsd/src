# $OpenBSD: Makefile,v 1.1 2001/05/22 15:35:58 rees Exp $

LIB=	sectok
SRCS=	atr.c cmdtab.c input.c r1r2.c readers.c sc7816.c
HDRS=	sectok.h

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
