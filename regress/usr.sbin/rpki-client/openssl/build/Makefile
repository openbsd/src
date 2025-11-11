# $OpenBSD: Makefile,v 1.2 2025/11/11 09:15:53 anton Exp $

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

.else
all:
	@echo SKIPPED: test depends on openssl--%3.5 and libretls
.endif
