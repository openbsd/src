# $OpenBSD: Makefile,v 1.3 2025/11/14 05:11:09 tb Exp $

EOPENSSL =	eopenssl35
EOPENSSL_PATH = /usr/local/lib/${EOPENSSL35}/

regress: all

.if (exists(/usr/local/lib/eopenssl35/libcrypto.a) && \
     exists(/usr/local/lib/eopenssl35/libtls.a)) || \
    defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
LDADD +=	-Wl,-rpath,/usr/local/lib/${EOPENSSL} -L/usr/local/lib/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/..
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED

.PATH:		${.CURDIR}/../../../../../usr.sbin/rpki-client

.include	"${.CURDIR}/../../../../../usr.sbin/rpki-client/Makefile"

sanity: all
	./rpki-client -f ${.CURDIR}/../../mft/*.mft
.else
all:
sanity:
	@echo SKIPPED: test depends on openssl--%3.5 and libretls
.endif
