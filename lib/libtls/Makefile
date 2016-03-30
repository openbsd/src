#	$OpenBSD: Makefile,v 1.23 2016/03/30 06:38:43 jmc Exp $

CFLAGS+= -Wall -Werror -Wimplicit
CFLAGS+= -DLIBRESSL_INTERNAL

LIB=	tls

DPADD=	${LIBCRYPTO} ${LIBSSL}

LDADD+= -L${BSDOBJDIR}/lib/libcrypto/crypto -lcrypto
LDADD+= -L${BSDOBJDIR}/lib/libssl/ssl -lssl

HDRS=	tls.h

SRCS=	tls.c \
	tls_client.c \
	tls_config.c \
	tls_conninfo.c \
	tls_peer.c \
	tls_server.c \
	tls_util.c \
	tls_verify.c

MAN=	tls_init.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/"; \
	    echo $$j; \
	    eval "$$j"; \
	done;

.include <bsd.lib.mk>
