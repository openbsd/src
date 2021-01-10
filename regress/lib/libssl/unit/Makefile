#	$OpenBSD: Makefile,v 1.11 2021/01/10 07:30:50 tb Exp $

TEST_CASES+= cipher_list
TEST_CASES+= ssl_get_shared_ciphers
TEST_CASES+= ssl_methods
TEST_CASES+= ssl_versions
TEST_CASES+= tls_ext_alpn
TEST_CASES+= tls_prf

REGRESS_TARGETS= all_tests

WARNINGS=	Yes
LDLIBS=		${SSL_INT} -lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-DCURDIR=\"${.CURDIR}\"
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

CLEANFILES+= ${TEST_CASES}

all_tests: ${TEST_CASES}
	@for test in $>; do \
		./$$test; \
	done

${TEST_CASES}: ${LIBSSL} ${LIBCRYPTO}

.include <bsd.regress.mk>
