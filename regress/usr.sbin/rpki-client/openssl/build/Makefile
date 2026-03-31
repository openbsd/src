# $OpenBSD: Makefile,v 1.7 2026/03/31 06:25:39 tb Exp $

EOPENSSL =	eopenssl35

.if ${EOPENSSL} != "eopenssl35"
CFLAGS += -DHAVE_X509_CRL_GET0_TBS_SIGALG
.endif

regress: all

.if (exists(/usr/local/lib/${EOPENSSL}/libcrypto.a) && \
     exists(/usr/local/lib/${EOPENSSL}/libtls.a)) || \
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
