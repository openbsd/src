# $OpenBSD: Makefile,v 1.5 2000/03/02 00:29:51 todd Exp $

LIB=	skey
SRCS=	skeylogin.c skeysubr.c put.c
HDRS=	skey.h
#CFLAGS+= -DSKEY_MDX_DEFAULT=4

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
