# $OpenBSD: Makefile,v 1.16 2002/09/26 11:39:50 markus Exp $

LIB=	ssl

SSLEAYDIST= src

LSSL_SRC= ${.CURDIR}/../${SSLEAYDIST}/ssl

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -DL_ENDIAN -DBN_ASM
.else
.if ${MACHINE_ARCH} == "mips"
CFLAGS+= -DL_ENDIAN
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
.endif

CFLAGS+= -DOPENSSL_NO_IDEA -DTERMIOS -DANSI_SOURCE -DNO_ERR 
CFLAGS+= -DOPENSSL_NO_MDC2
CFLAGS+= -DOPENSSL_NO_RC5 -DOPENSSL_NO_KRB5 -DSO_DLFCN -DHAVE_DLFCN_H 
CFLAGS+= -I${.CURDIR}/../${SSLEAYDIST}

SRCS=	bio_ssl.c s2_clnt.c s3_both.c s3_srvr.c ssl_err2.c ssl_txt.c	\
	t1_srvr.c s23_clnt.c s2_enc.c s3_clnt.c ssl_algs.c ssl_lib.c	\
	ssltest.c s23_lib.c s2_lib.c s3_enc.c ssl_asn1.c ssl_rsa.c	\
	t1_clnt.c s23_meth.c s2_meth.c s3_lib.c ssl_cert.c ssl_sess.c	\
	t1_enc.c s23_pkt.c s2_pkt.c s3_meth.c ssl_ciph.c ssl_stat.c	\
	t1_lib.c s23_srvr.c s2_srvr.c s3_pkt.c ssl_err.c \
	t1_meth.c 

HDRS=	ssl.h ssl2.h ssl3.h ssl23.h tls1.h kssl.h 

.PATH:	${LSSL_SRC}

includes:
	@test -d ${DESTDIR}/usr/include/ssl || mkdir ${DESTDIR}/usr/include/ssl
	@d=`mktemp -d /tmp/libsslXXXXXXXXXX`; \
	cd ${LSSL_SRC}; for i in $(HDRS); do \
		f=`basename $$i`; \
		j="sed 's/<openssl/<ssl/' \
		       $$i >$$d/$$f && \
		(cmp -s $$d/$$f ${DESTDIR}/usr/include/ssl/$$f || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$d/$$f ${DESTDIR}/usr/include/ssl)"; \
		echo $$j; \
		eval "$$j"; \
	done; \
	rm -rf $$d

.include <bsd.lib.mk>
