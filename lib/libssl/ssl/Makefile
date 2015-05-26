# $OpenBSD: Makefile,v 1.51 2015/05/26 19:38:13 miod Exp $

LIB=	ssl

SSL_SRC=	${.CURDIR}/../../libssl/src
LSSL_SRC=	${SSL_SRC}/ssl

.include <bsd.own.mk>
CFLAGS+= -Wall
.if ${COMPILER_VERSION:L} != "gcc3"
CFLAGS+= -Werror
.endif
CFLAGS+= -DLIBRESSL_INTERNAL
CFLAGS+= -I${SSL_SRC}

LDADD+= -L${BSDOBJDIR}/lib/libcrypto/crypto/obj -lcrypto

SRCS=\
	s3_meth.c s3_srvr.c s3_clnt.c s3_lib.c s3_enc.c s3_pkt.c s3_both.c \
	s23_meth.c s23_srvr.c s23_clnt.c s23_lib.c s23_pkt.c \
	t1_meth.c t1_srvr.c t1_clnt.c t1_lib.c t1_enc.c \
	d1_meth.c d1_srvr.c d1_clnt.c d1_lib.c d1_pkt.c \
	d1_both.c d1_enc.c d1_srtp.c \
	ssl_lib.c ssl_err2.c ssl_cert.c ssl_sess.c \
	ssl_ciph.c ssl_stat.c ssl_rsa.c \
	ssl_asn1.c ssl_txt.c ssl_algs.c \
	bio_ssl.c ssl_err.c t1_reneg.c \
	pqueue.c
SRCS+=	s3_cbc.c
SRCS+=	bs_ber.c bs_cbb.c bs_cbs.c

HDRS=	srtp.h ssl.h ssl2.h ssl3.h ssl23.h tls1.h dtls1.h 

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
