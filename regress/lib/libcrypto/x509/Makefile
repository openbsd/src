#	$OpenBSD: Makefile,v 1.26 2026/06/23 08:24:49 tb Exp $

PROGS =	constraints verify x509attribute x509req_ext callback
PROGS += expirecallback callbackfailures x509_asn1 x509_extensions_test
PROGS += x509_crl x509_name_test
LDADD =	-lcrypto
DPADD =	${LIBCRYPTO}

REGRESS_EXPECTED_FAILURES = run-regress-x509_crl

LDADD_constraints = ${CRYPTO_INT}
LDADD_verify = ${CRYPTO_INT}

WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/x509
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring

SUBDIR += bettertls policy rfc3779

CLEANFILES +=	callback.out

.if make(clean) || make(cleandir)
. if ${.OBJDIR} != ${.CURDIR}
.BEGIN:
	 rm -rf [0-9]*
. endif
.endif

run-regress-verify: verify
	perl ${.CURDIR}/make-dir-roots.pl ${.CURDIR}/../certs .
	./verify ${.CURDIR}/../certs

run-regress-callback: callback
	./callback ${.CURDIR}/../certs
	perl ${.CURDIR}/callback.pl callback.out

run-regress-expirecallback: expirecallback
	./expirecallback ${.CURDIR}/../certs

run-regress-callbackfailures: callbackfailures
	./callbackfailures ${.CURDIR}/../certs

.include <bsd.regress.mk>
