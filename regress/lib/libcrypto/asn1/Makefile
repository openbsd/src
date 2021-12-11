#	$OpenBSD: Makefile,v 1.10 2021/12/11 22:58:48 schwarze Exp $

TESTS = \
	asn1basic \
	asn1complex \
	asn1evp \
	asn1string_copy \
	asn1time \
	asn1x509 \
	rfc5280time \
	string_table

PROGS = ${TESTS}

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
