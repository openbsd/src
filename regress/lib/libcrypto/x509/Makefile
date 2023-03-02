#	$OpenBSD: Makefile,v 1.16 2023/03/02 21:15:14 tb Exp $

PROGS =	constraints verify x509attribute x509name x509req_ext callback
PROGS += expirecallback callbackfailures
LDADD =	-lcrypto
DPADD =	${LIBCRYPTO}

LDADD_constraints = ${CRYPTO_INT}
LDADD_verify = ${CRYPTO_INT}

WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/x509
CFLAGS += 	-I${.CURDIR}/../../../../lib/libcrypto/bytestring

SUBDIR += bettertls rfc3779

CLEANFILES +=	x509name.result callback.out

.if make(clean) || make(cleandir)
. if ${.OBJDIR} != ${.CURDIR}
.BEGIN:
	 rm -rf [0-9]*
. endif
.endif

run-regress-verify: verify
	perl ${.CURDIR}/make-dir-roots.pl ${.CURDIR}/../certs .
	./verify ${.CURDIR}/../certs

run-regress-x509name: x509name
	./x509name > x509name.result
	diff -u ${.CURDIR}/x509name.expected x509name.result

run-regress-callback: callback
	./callback ${.CURDIR}/../certs
	perl ${.CURDIR}/callback.pl callback.out

run-regress-expirecallback: expirecallback
	./expirecallback ${.CURDIR}/../certs

run-regress-callbackfailures: callbackfailures
	./callbackfailures ${.CURDIR}/../certs

.include <bsd.regress.mk>
