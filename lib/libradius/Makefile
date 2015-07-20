#	$OpenBSD: Makefile,v 1.1 2015/07/20 23:52:29 yasuoka Exp $

LIB=    	radius
SRCS=		radius.c radius_attr.c radius_msgauth.c radius_userpass.c \
		radius_mppe.c radius_eapmsk.c
INCS=		radius.h

CFLAGS+=	-Wall

MAN=		radius.3

.include <bsd.lib.mk>

includes:
	@cd ${.CURDIR}; for i in $(INCS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		    -m 444 $$i ${DESTDIR}/usr/include"; \
		echo $$j; \
		eval "$$j"; \
	done
