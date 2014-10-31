#	$OpenBSD: Makefile,v 1.1 2014/10/31 13:46:17 jsing Exp $

CFLAGS+= -Wall -Werror -Wimplicit
CFLAGS+= -DLIBRESSL_INTERNAL

LIB=	tls

DPADD=	${LIBCRYPTO} ${LIBSSL}

HDRS=	tls.h

SRCS=	tls.c \
	tls_client.c \
	tls_config.c \
	tls_server.c \
	tls_util.c \
	tls_verify.c

MAN=	tls_init.3

MLINKS+=tls_init.3 tls_config_new.3
MLINKS+=tls_init.3 tls_config_free.3
MLINKS+=tls_init.3 tls_config_set_ca_file.3
MLINKS+=tls_init.3 tls_config_set_ca_path.3
MLINKS+=tls_init.3 tls_config_set_cert_file.3
MLINKS+=tls_init.3 tls_config_set_cert_mem.3
MLINKS+=tls_init.3 tls_config_set_ciphers.3
MLINKS+=tls_init.3 tls_config_set_ecdhcurve.3
MLINKS+=tls_init.3 tls_config_set_key_file.3
MLINKS+=tls_init.3 tls_config_set_key_mem.3
MLINKS+=tls_init.3 tls_config_set_protocols.3
MLINKS+=tls_init.3 tls_config_set_verify_depth.3
MLINKS+=tls_init.3 tls_config_clear_keys.3
MLINKS+=tls_init.3 tls_config_insecure_noverifyhost.3
MLINKS+=tls_init.3 tls_config_insecure_noverifycert.3
MLINKS+=tls_init.3 tls_config_verify.3
MLINKS+=tls_init.3 tls_client.3
MLINKS+=tls_init.3 tls_server.3
MLINKS+=tls_init.3 tls_configure.3
MLINKS+=tls_init.3 tls_error.3
MLINKS+=tls_init.3 tls_reset.3
MLINKS+=tls_init.3 tls_free.3
MLINKS+=tls_init.3 tls_close.3
MLINKS+=tls_init.3 tls_connect.3
MLINKS+=tls_init.3 tls_connect_socket.3
MLINKS+=tls_init.3 tls_read.3
MLINKS+=tls_init.3 tls_write.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/"; \
	    echo $$j; \
	    eval "$$j"; \
	done;

.include <bsd.lib.mk>
