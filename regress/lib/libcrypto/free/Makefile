#	$OpenBSD: Makefile,v 1.5 2023/04/15 14:10:09 tb Exp $

TESTS = \
	freenull

PROG = ${TESTS}

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}

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
