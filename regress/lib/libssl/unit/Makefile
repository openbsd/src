#	$OpenBSD: Makefile,v 1.4 2016/08/27 16:00:27 jsing Exp $

TEST_CASES+= cipher_list
TEST_CASES+= tls_ext_alpn

REGRESS_TARGETS= all_tests

LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto -lssl
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror -I../../../../lib/libssl/src/ssl

CLEANFILES+= ${TEST_CASES}

all_tests: ${TEST_CASES}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
