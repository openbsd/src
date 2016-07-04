#	$OpenBSD: Makefile,v 1.1 2016/07/04 23:43:30 beck Exp $

TESTS = \
	ocsp_test

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto -lssl
DPADD=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto -lssl
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

CLEANFILES+= ${TESTS}

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test www.amazon.com 443; \
	done

.include <bsd.regress.mk>
