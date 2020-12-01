#	$OpenBSD: Makefile,v 1.10 2020/12/01 07:49:03 tb Exp $

TEST_CASES+= cipher_list
TEST_CASES+= ssl_methods
TEST_CASES+= ssl_versions
TEST_CASES+= tls_ext_alpn
TEST_CASES+= tls_prf

REGRESS_TARGETS= all_tests

WARNINGS=	Yes
LDLIBS=		${SSL_INT} -lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

CLEANFILES+= ${TEST_CASES}

all_tests: ${TEST_CASES}
	@for test in $>; do \
		./$$test; \
	done

${TEST_CASES}: ${LIBSSL} ${LIBCRYPTO}

.include <bsd.regress.mk>
