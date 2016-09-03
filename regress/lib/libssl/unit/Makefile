#	$OpenBSD: Makefile,v 1.5 2016/09/03 16:52:18 bcook Exp $

TEST_CASES+= cipher_list
TEST_CASES+= tls_ext_alpn

REGRESS_TARGETS= all_tests

LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto -lssl
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

CLEANFILES+= ${TEST_CASES}

all_tests: ${TEST_CASES}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
