#	$OpenBSD: Makefile,v 1.1 2017/05/02 04:03:06 beck Exp $

TESTS = \
	freenull

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO} ${LIBSSL}
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL

CLEANFILES+= ${TESTS}

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
