# $OpenBSD: Makefile,v 1.6 2025/11/10 16:29:08 tb Exp $

EOPENSSL =	eopenssl35

LDADD +=	-Wl,-rpath,/usr/local/lib/${EOPENSSL} -L/usr/local/lib/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/${EOPENSSL}/
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED

.PATH:		${.CURDIR}/..

.if exists(/usr/local/lib/eopenssl35/libcrypto.a) && \
    exists(/usr/local/lib/eopenssl35/libtls.a)

build-check:
	${MAKE} -C ${.CURDIR}/build

sanity: build-check
	${.OBJDIR}/build/rpki-client -f ${.CURDIR}/../mft/*.mft

REGRESS_TARGETS += build-check
REGRESS_TARGETS += sanity

.endif

SUBDIR += build

.include <bsd.regress.mk>
