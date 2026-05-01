# $OpenBSD: Makefile,v 1.11 2026/05/01 11:23:57 tb Exp $

EOPENSSL =	eopenssl35

.if ${EOPENSSL} != "eopenssl35"
CFLAGS += -DHAVE_ASN1_BIT_STRING_GET_LENGTH
CFLAGS += -DHAVE_ASN1_BIT_STRING_SET1
CFLAGS += -DHAVE_X509_CRL_GET0_TBS_SIGALG
.endif

LDADD +=	-Wl,-rpath,/usr/local/lib/${EOPENSSL} -L/usr/local/lib/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/${EOPENSSL}/
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED

.PATH:		${.CURDIR}/..

.if exists(/usr/local/lib/${EOPENSSL}/libcrypto.a) && \
    exists(/usr/local/lib/${EOPENSSL}/libtls.a)

build-check:
	${MAKE} -C ${.CURDIR}/build

sanity: build-check
	${MAKE} -C ${.CURDIR}/build sanity

REGRESS_TARGETS += build-check
REGRESS_TARGETS += sanity

.endif

SUBDIR += build

.include <bsd.regress.mk>
