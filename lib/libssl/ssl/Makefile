# $OpenBSD: Makefile,v 1.23 2010/10/01 23:02:02 djm Exp $

LIB=	ssl
WANTLINT=

SSLEAYDIST= src

LSSL_SRC= ${.CURDIR}/../${SSLEAYDIST}/ssl

CFLAGS+= -DOPENSSL_NO_IDEA -DTERMIOS -DANSI_SOURCE
CFLAGS+= -DOPENSSL_NO_MDC2
CFLAGS+= -DOPENSSL_NO_RC5 -DOPENSSL_NO_KRB5
CFLAGS+= -I${.CURDIR}/../${SSLEAYDIST} -I${.CURDIR}/../${SSLEAYDIST}/crypto

SRCS=\
	s2_meth.c s2_srvr.c s2_clnt.c s2_lib.c s2_enc.c s2_pkt.c \
	s3_meth.c s3_srvr.c s3_clnt.c s3_lib.c s3_enc.c s3_pkt.c s3_both.c \
	s23_meth.c s23_srvr.c s23_clnt.c s23_lib.c s23_pkt.c \
	t1_meth.c t1_srvr.c t1_clnt.c t1_lib.c t1_enc.c \
	d1_meth.c d1_srvr.c d1_clnt.c d1_lib.c d1_pkt.c \
	d1_both.c d1_enc.c \
	ssl_lib.c ssl_err2.c ssl_cert.c ssl_sess.c \
	ssl_ciph.c ssl_stat.c ssl_rsa.c \
	ssl_asn1.c ssl_txt.c ssl_algs.c \
	bio_ssl.c ssl_err.c kssl.c t1_reneg.c

HDRS=	ssl.h ssl2.h ssl3.h ssl23.h tls1.h dtls1.h kssl.h

.PATH:	${LSSL_SRC}

includes:
	@test -d ${DESTDIR}/usr/include/openssl || \
	    mkdir ${DESTDIR}/usr/include/openssl
	@cd ${LSSL_SRC}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/openssl/`basename $$i` || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/openssl"; \
	    echo $$j; \
	    eval "$$j"; \
	done;

.include <bsd.lib.mk>
