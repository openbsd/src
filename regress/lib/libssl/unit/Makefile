#	$OpenBSD: Makefile,v 1.3 2015/07/13 07:34:50 doug Exp $

TEST_CASES+= cipher_list

# These don't pass yet
#TEST_CASES+= tls_ext_alpn

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
