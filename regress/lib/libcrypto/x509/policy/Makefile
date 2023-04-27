#	$OpenBSD: Makefile,v 1.1 2023/04/27 12:23:31 beck Exp $

PROGS =		policy

LDADD =	-lcrypto
DPADD =	${LIBCRYPTO}

LDADD_policy = ${CRYPTO_INT}

WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../../lib/libcrypto/x509
CFLAGS += 	-I${.CURDIR}/../../../../../lib/libcrypto/bytestring
CFLAGS += 	-DCERTSDIR=\"${.CURDIR}/../../../libcrypto/x509/policy\"

REGRESS_TARGETS =  policy-test

policy-test:	policy
	./policy 

.include "../../Makefile.inc"
.include <bsd.regress.mk>
