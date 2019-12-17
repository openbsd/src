# $OpenBSD: Makefile,v 1.4 2019/12/17 13:08:54 reyk Exp $

.PATH:	${.CURDIR}/man ${.CURDIR}/src

CFLAGS+= -I${.CURDIR}/src -D_FIDO_INTERNAL -DHAVE_UNISTD_H -std=c99

WARNINGS=	yes
CDIAGFLAGS+=	-Wall -Wextra
CDIAGFLAGS+=	-Werror

LDADD+=		-L${BSDOBJDIR}/lib/libcbor -lcbor

SYMBOL_LIST=	Symbols.list
VERSION_SCRIPT=	Symbols.map
CLEANFILES=	${SYMBOL_LIST} ${VERSION_SCRIPT}

LIB=	fido2
SRCS=	aes256.c assert.c authkey.c bio.c blob.c buf.c cbor.c cred.c credman.c
SRCS+=	dev.c ecdh.c eddsa.c err.c es256.c hid.c hid_openbsd.c info.c io.c
SRCS+=	iso7816.c log.c pin.c reset.c rs256.c u2f.c

HDRS=	fido.h fido/bio.h fido/credman.h fido/eddsa.h fido/err.h
HDRS+=	fido/es256.h fido/param.h fido/rs256.h

NOMAN=	# XXX pending cleanup
#MAN=	eddsa_pk.3 es256_pk.3 fido.3 fido2-assert.1 fido2-cred.1 fido2-token.1
#MAN+=	fido_assert.3 fido_assert_allow_cred.3 fido_assert_set.3
#MAN+=	fido_assert_verify.3 fido_bio_dev.3 fido_bio_enroll.3
#MAN+=	fido_bio_info.3 fido_bio_template.3 fido_cbor_info.3 fido_cred.3
#MAN+=	fido_cred_exclude.3 fido_cred_set.3 fido_cred_verify.3 fido_credman.3
#MAN+=	fido_dev_get_assert.3 fido_dev_info_manifest.3 fido_dev_make_cred.3
#MAN+=	fido_dev_open.3 fido_dev_set_io_functions.3 fido_dev_set_pin.3
#MAN+=	fido_strerr.3 rs256_pk.3

includes:
	@for i in $(HDRS); do \
		j="test -d ${DESTDIR}/usr/include/`dirname $$i` || \
		    ${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		    ${DESTDIR}/usr/include/`dirname $$i`"; \
		echo $$j; \
		eval "$$j"; \
		j="cmp -s src/$$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		    -m 444 ${.CURDIR}/src/$$i ${DESTDIR}/usr/include/$$i"; \
		echo $$j; \
		eval "$$j"; \
	done

${SYMBOL_LIST}: ${.CURDIR}/src/export.llvm
	sort ${.CURDIR}/src/export.llvm | sed 's/^_//' > $@.tmp && mv $@.tmp $@

${VERSION_SCRIPT}: ${SYMBOL_LIST}
	{ printf '{\n\tglobal:\n'; \
	  sed '/^[._a-zA-Z]/s/$$/;/; s/^/		/' ${SYMBOL_LIST}; \
	  printf '\n\tlocal:\n\t\t*;\n};\n'; } >$@.tmp && mv $@.tmp $@

.include <bsd.lib.mk>
