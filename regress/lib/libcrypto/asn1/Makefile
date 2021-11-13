#	$OpenBSD: Makefile,v 1.6 2021/11/13 20:50:14 schwarze Exp $

TESTS = \
	asn1evp \
	asn1string_copy \
	asn1time \
	rfc5280time

PROGS = ${TESTS}

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
