# $OpenBSD: Makefile,v 1.43 2018/11/09 00:34:55 beck Exp $

.include <bsd.own.mk>
.ifndef NOMAN
SUBDIR=	man
.endif

PC_FILES=openssl.pc libssl.pc

CLEANFILES=${PC_FILES} ${VERSION_SCRIPT}

LIB=	ssl

CFLAGS+= -Wall -Wundef
.if ${COMPILER_VERSION:L} == "clang"
CFLAGS+= -Werror
.endif
CFLAGS+= -DLIBRESSL_INTERNAL

CFLAGS+= -I${.CURDIR}

LDADD+= -L${BSDOBJDIR}/lib/libcrypto -lcrypto

VERSION_SCRIPT=	Symbols.map
SYMBOL_LIST=	${.CURDIR}/Symbols.list

SRCS= \
	ssl_srvr.c ssl_clnt.c s3_lib.c ssl_pkt.c ssl_both.c \
	t1_lib.c t1_enc.c t1_hash.c \
	d1_srvr.c d1_clnt.c d1_lib.c d1_pkt.c \
	d1_both.c d1_enc.c d1_srtp.c \
	ssl_lib.c ssl_cert.c ssl_sess.c \
	ssl_ciph.c ssl_stat.c ssl_rsa.c \
	ssl_asn1.c ssl_txt.c ssl_algs.c \
	bio_ssl.c ssl_err.c ssl_methods.c \
	ssl_packet.c ssl_tlsext.c ssl_versions.c pqueue.c ssl_init.c \
	tls13_handshake.c tls13_key_schedule.c ssl_sigalgs.c
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
