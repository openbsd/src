
LIB=	ssl

SSLEAYDIST= src-patent

LSSL_SRC= ${.CURDIR}/../${SSLEAYDIST}/ssl
LCRYPTO_SRC= ${.CURDIR}/../${SSLEAYDIST}/crypto

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -DL_ENDIAN -DBN_ASM
.else
.if ${MACHINE_ARCH} == "alpha"
# no ENDIAN stuff defined for alpha
.else
CFLAGS+= -DB_ENDIAN
.endif
.endif
CFLAGS+= -DNO_IDEA -DTERMIOS -DANSI_SOURCE 
CFLAGS+= -I${.CURDIR}/../${SSLEAYDIST}
CFLAGS+= -I${LSSL_SRC} -I${LCRYPTO_SRC}
CFLAGS+= -I${LCRYPTO_SRC}/md2
CFLAGS+= -I${LCRYPTO_SRC}/md5
CFLAGS+= -I${LCRYPTO_SRC}/sha
CFLAGS+= -I${LCRYPTO_SRC}/mdc2
CFLAGS+= -I${LCRYPTO_SRC}/hmac
CFLAGS+= -I${LCRYPTO_SRC}/ripemd
CFLAGS+= -I${LCRYPTO_SRC}/des
CFLAGS+= -I${LCRYPTO_SRC}/rc2
CFLAGS+= -I${LCRYPTO_SRC}/rc4
CFLAGS+= -I${LCRYPTO_SRC}/rc5
CFLAGS+= -I${LCRYPTO_SRC}/idea
CFLAGS+= -I${LCRYPTO_SRC}/bf
CFLAGS+= -I${LCRYPTO_SRC}/cast
CFLAGS+= -I${LCRYPTO_SRC}/bn
CFLAGS+= -I${LCRYPTO_SRC}/rsa
CFLAGS+= -I${LCRYPTO_SRC}/dsa
CFLAGS+= -I${LCRYPTO_SRC}/dh
CFLAGS+= -I${LCRYPTO_SRC}/buffer
CFLAGS+= -I${LCRYPTO_SRC}/bio
CFLAGS+= -I${LCRYPTO_SRC}/stack
CFLAGS+= -I${LCRYPTO_SRC}/lhash
CFLAGS+= -I${LCRYPTO_SRC}/rand
CFLAGS+= -I${LCRYPTO_SRC}/err
CFLAGS+= -I${LCRYPTO_SRC}/objects
CFLAGS+= -I${LCRYPTO_SRC}/evp
CFLAGS+= -I${LCRYPTO_SRC}/pem
CFLAGS+= -I${LCRYPTO_SRC}/asn1
CFLAGS+= -I${LCRYPTO_SRC}/x509
CFLAGS+= -I${LCRYPTO_SRC}/conf
CFLAGS+= -I${LCRYPTO_SRC}/txt_db
CFLAGS+= -I${LCRYPTO_SRC}/pkcs7

SRCS=	s2_meth.c   s2_srvr.c s2_clnt.c  s2_lib.c  s2_enc.c s2_pkt.c \
	s3_meth.c   s3_srvr.c s3_clnt.c  s3_lib.c  s3_enc.c s3_pkt.c s3_both.c \
	s23_meth.c s23_srvr.c s23_clnt.c s23_lib.c          s23_pkt.c \
	t1_meth.c   t1_srvr.c t1_clnt.c  t1_lib.c  t1_enc.c \
	ssl_lib.c ssl_err2.c ssl_cert.c ssl_sess.c \
	ssl_ciph.c ssl_stat.c ssl_rsa.c \
	ssl_asn1.c ssl_txt.c ssl_algs.c \
	bio_ssl.c ssl_err.c

HDRS=	ssl.h ssl2.h ssl3.h ssl23.h tls1.h

.PATH:	${LSSL_SRC}

includes:
	@cd ${LSSL_SRC}; for i in $(HDRS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/ssl/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$i ${DESTDIR}/usr/include/ssl"; \
		echo $$j; \
		eval "$$j"; \
	done


.include <bsd.lib.mk>
