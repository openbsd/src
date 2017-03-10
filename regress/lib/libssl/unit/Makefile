#	$OpenBSD: Makefile,v 1.9 2017/03/10 15:06:15 jsing Exp $

TEST_CASES+= cipher_list
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
