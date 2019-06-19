#	$OpenBSD: Makefile,v 1.3 2018/08/31 17:35:21 tb Exp $

TESTS = \
	freenull

PROG = ${TESTS}

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO} ${LIBSSL}
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL

CLEANFILES+= freenull.c freenull.c.body freenull.c.tmp

freenull.c: freenull.awk ../../../../lib/libcrypto/Symbols.list
	awk -f ${.CURDIR}/freenull.awk \
		< ${BSDSRCDIR}/lib/libcrypto/Symbols.list > freenull.c.body
	cat ${.CURDIR}/freenull.c.head freenull.c.body \
		${.CURDIR}/freenull.c.tail > freenull.c.tmp
	mv freenull.c.tmp freenull.c

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
