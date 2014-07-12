#	$OpenBSD: Makefile,v 1.1 2014/07/12 01:20:24 jsing Exp $

CFLAGS+= -Wall -Werror -Wimplicit
CFLAGS+= -DLIBRESSL_INTERNAL

LIB=	ressl

DPADD=	${LIBCRYPTO} ${LIBSSL}

HDRS=	ressl.h ressl_config.h

SRCS=	ressl.c \
	ressl_config.c \
	ressl_util.c \
	ressl_verify.c

includes:
	@test -d ${DESTDIR}/usr/include/ressl || \
	    mkdir ${DESTDIR}/usr/include/ressl
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/ressl/$$i || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/ressl"; \
	    echo $$j; \
	    eval "$$j"; \
	done;

.include <bsd.lib.mk>
