#	$OpenBSD: Makefile,v 1.12 2021/05/03 23:42:04 inoguchi Exp $

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
CFLAGS+=	-DCERTSDIR=\"${.CURDIR}/../certs\"
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

CLEANFILES+= ${TEST_CASES}

all_tests: ${TEST_CASES}
	@for test in $>; do \
		./$$test; \
	done

${TEST_CASES}: ${LIBSSL} ${LIBCRYPTO}

.include <bsd.regress.mk>
