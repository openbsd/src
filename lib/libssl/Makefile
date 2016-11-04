# $OpenBSD: Makefile,v 1.24 2016/11/04 18:28:58 guenther Exp $

SUBDIR= man
PC_FILES=openssl.pc libssl.pc

CLEANFILES=${PC_FILES}

LIB=	ssl

.include <bsd.own.mk>
CFLAGS+= -Wall -Wundef
.if ${COMPILER_VERSION:L} != "gcc3"
CFLAGS+= -Werror
.endif
CFLAGS+= -DLIBRESSL_INTERNAL
CFLAGS+= -I${.CURDIR}

LDADD+= -L${BSDOBJDIR}/lib/libcrypto -lcrypto

VERSION_SCRIPT=	Symbols.map
SYMBOL_LIST=	${.CURDIR}/Symbols.list

SRCS=\
	s3_srvr.c s3_clnt.c s3_lib.c s3_pkt.c s3_both.c \
	s23_srvr.c s23_clnt.c s23_lib.c s23_pkt.c \
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

.PATH:	${.CURDIR}

includes:
	@test -d ${DESTDIR}/usr/include/openssl || \
	    mkdir ${DESTDIR}/usr/include/openssl
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/openssl/`basename $$i` || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/openssl"; \
	    echo $$j; \
	    eval "$$j"; \
	done;

${VERSION_SCRIPT}: ${SYMBOL_LIST}
	{ printf '{\n\tglobal:\n'; \
	  sed '/^[._a-zA-Z]/s/$$/;/; s/^/		/' ${SYMBOL_LIST}; \
	  printf '\n\tlocal:\n\t\t*;\n};\n'; } >$@.tmp && mv $@.tmp $@

.include <bsd.lib.mk>

all: ${PC_FILES}
${PC_FILES}: ${.CURDIR}/../libcrypto/opensslv.h
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR}

beforeinstall:
	nm -o lib${LIB}.a | egrep -w 'printf|fprintf' && \
	    (echo please fix stdio usage in this library; false) || true
.for p in ${PC_FILES}
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.OBJDIR}/$p ${DESTDIR}/usr/lib/pkgconfig/
.endfor

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
