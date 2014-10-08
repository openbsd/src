#	$OpenBSD: Makefile,v 1.5 2014/10/08 19:01:40 tedu Exp $

CFLAGS+= -Wall -Werror -Wimplicit
CFLAGS+= -DLIBRESSL_INTERNAL

LIB=	ressl

DPADD=	${LIBCRYPTO} ${LIBSSL}

HDRS=	ressl.h

SRCS=	ressl.c \
	ressl_client.c \
	ressl_config.c \
	ressl_server.c \
	ressl_util.c \
	ressl_verify.c

MAN=	ressl_init.3

MLINKS+=ressl_init.3 ressl_error.3
MLINKS+=ressl_init.3 ressl_config_new.3
MLINKS+=ressl_init.3 ressl_config_free.3
MLINKS+=ressl_init.3 ressl_set_ca_file.3
MLINKS+=ressl_init.3 ressl_set_ca_path.3
MLINKS+=ressl_init.3 ressl_set_cert_file.3
MLINKS+=ressl_init.3 ressl_set_cert_mem.3
MLINKS+=ressl_init.3 ressl_set_ciphers.3
MLINKS+=ressl_init.3 ressl_set_ecdhcurve.3
MLINKS+=ressl_init.3 ressl_set_key_file.3
MLINKS+=ressl_init.3 ressl_set_key_mem.3
MLINKS+=ressl_init.3 ressl_set_protocols.3
MLINKS+=ressl_init.3 ressl_set_verify_depth.3
MLINKS+=ressl_init.3 ressl_clear_keys.3
MLINKS+=ressl_init.3 ressl_insecure_noverifyhost.3
MLINKS+=ressl_init.3 ressl_insecure_noverifycert.3
MLINKS+=ressl_init.3 ressl_verify.3
MLINKS+=ressl_init.3 ressl_configure.3
MLINKS+=ressl_init.3 ressl_reset.3
MLINKS+=ressl_init.3 ressl_free.3
MLINKS+=ressl_init.3 ressl_close.3
MLINKS+=ressl_init.3 ressl_connect.3
MLINKS+=ressl_init.3 ressl_connect_socket.3
MLINKS+=ressl_init.3 ressl_read.3
MLINKS+=ressl_init.3 ressl_write.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/"; \
	    echo $$j; \
	    eval "$$j"; \
	done;

.include <bsd.lib.mk>
