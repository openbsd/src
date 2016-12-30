#	$OpenBSD: Makefile,v 1.7 2016/12/30 16:58:12 jsing Exp $

TEST_CASES+= cipher_list
TEST_CASES+= ssl_versions
TEST_CASES+= tls_ext_alpn

REGRESS_TARGETS= all_tests

WARNINGS=	Yes
LDLIBS=		${SSL_INT} -lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

CLEANFILES+= ${TEST_CASES}

all_tests: ${TEST_CASES}
	@for test in $>; do \
		./$$test; \
	done

${TEST_CASES}: ${LIBSSL} ${LIBCRYPTO}

.include <bsd.regress.mk>
