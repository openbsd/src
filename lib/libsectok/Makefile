# $OpenBSD: Makefile,v 1.7 2001/08/03 18:29:14 rees Exp $

LIB=	sectok
SRCS=	atr.c cmdtab.c cyberflex.c input.c r1r2.c sc7816.c sectok.c todos_atr.c \
	ifdhandler.c scT1.c scio.c scrw.c
HDRS=	sectok.h
MAN=	sectok.3

CFLAGS+=-Wall

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
