# $OpenBSD: Makefile,v 1.22 2008/09/06 12:20:16 djm Exp $

LIB=	ssl
WANTLINT=

SSLEAYDIST= src

LSSL_SRC= ${.CURDIR}/../${SSLEAYDIST}/ssl

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -DL_ENDIAN -DBN_ASM
.else
.if ${MACHINE_ARCH} == "vax"
CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "alpha"
# no ENDIAN stuff defined for alpha
.else
CFLAGS+= -DB_ENDIAN
.endif
.endif 
.endif

CFLAGS+= -DOPENSSL_NO_IDEA -DTERMIOS -DANSI_SOURCE -DNO_ERR 
CFLAGS+= -DOPENSSL_NO_MDC2
CFLAGS+= -DOPENSSL_NO_RC5 -DOPENSSL_NO_KRB5 -DHAVE_DLFCN_H 
CFLAGS+= -I${.CURDIR}/../${SSLEAYDIST} -I${.CURDIR}/../${SSLEAYDIST}/crypto

SRCS=	bio_ssl.c s2_clnt.c s3_both.c s3_srvr.c ssl_err2.c ssl_txt.c	\
	t1_srvr.c s23_clnt.c s2_enc.c s3_clnt.c ssl_algs.c ssl_lib.c	\
	s23_lib.c s2_lib.c s3_enc.c ssl_asn1.c ssl_rsa.c	\
	t1_clnt.c s23_meth.c s2_meth.c s3_lib.c ssl_cert.c ssl_sess.c	\
	t1_enc.c s23_pkt.c s2_pkt.c s3_meth.c ssl_ciph.c ssl_stat.c	\
	t1_lib.c s23_srvr.c s2_srvr.c s3_pkt.c ssl_err.c \
	t1_meth.c \
	d1_both.c d1_clnt.c d1_enc.c d1_lib.c d1_meth.c d1_pkt.c d1_srvr.c

HDRS=	ssl.h ssl2.h ssl3.h ssl23.h tls1.h kssl.h dtls1.h

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
