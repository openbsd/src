#	$OpenBSD: Makefile,v 1.1 2015/09/25 16:12:30 jsing Exp $

TESTS = \
	asn1time

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

CLEANFILES+= ${TESTS}

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
