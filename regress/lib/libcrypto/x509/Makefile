#	$OpenBSD: Makefile,v 1.15 2022/11/11 12:02:34 beck Exp $

PROGS =	constraints verify x509attribute x509name x509req_ext callback
PROGS += expirecallback callbackfailures
LDADD =	-lcrypto
DPADD =	${LIBCRYPTO}

LDADD_constraints = ${CRYPTO_INT}
LDADD_verify = ${CRYPTO_INT}

WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror -I$(BSDSRCDIR)/lib/libcrypto/x509
CFLAGS += 	-I$(BSDSRCDIR)/lib/libcrypto/bytestring

SUBDIR += bettertls rfc3779

REGRESS_TARGETS += regress-verify
REGRESS_TARGETS += regress-constraints
REGRESS_TARGETS += regress-x509attribute
REGRESS_TARGETS += regress-x509name
REGRESS_TARGETS += regress-x509req_ext
REGRESS_TARGETS += regress-callback
REGRESS_TARGETS += regress-expirecallback
REGRESS_TARGETS += regress-callbackfailures

CLEANFILES +=	x509name.result callbackout

.if make(clean) || make(cleandir)
. if ${.OBJDIR} != ${.CURDIR}
.BEGIN:
	 rm -rf [0-9]*
. endif
.endif

regress-verify: verify
	perl ${.CURDIR}/make-dir-roots.pl ${.CURDIR}/../certs .
	./verify ${.CURDIR}/../certs

regress-constraints: constraints
	./constraints

regress-x509attribute: x509attribute
	./x509attribute

regress-x509name: x509name
	./x509name > x509name.result
	diff -u ${.CURDIR}/x509name.expected x509name.result

regress-x509req_ext: x509req_ext
	./x509req_ext

regress-callback: callback
	./callback ${.CURDIR}/../certs
	perl ${.CURDIR}/callback.pl callback.out

regress-expirecallback: expirecallback
	./expirecallback ${.CURDIR}/../certs

regress-callbackfailures: callbackfailures
	./callbackfailures ${.CURDIR}/../certs

.include <bsd.regress.mk>
