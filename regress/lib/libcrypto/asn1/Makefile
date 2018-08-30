#	$OpenBSD: Makefile,v 1.4 2018/08/30 18:36:59 tb Exp $

TESTS = \
	asn1evp \
	asn1time \
	rfc5280time

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

CLEANFILES+= ${TESTS} ${TESTS:S/$/.d/}

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
