#	$OpenBSD: Makefile,v 1.2 2002/08/26 13:11:41 art Exp $

PROG=		gaitest
SRCS=		gaitest.c
NOMAN=		# defined
CLEANFILES+=	out

REGRESSTARGETS=do-test

do-test: ${PROG}
	sh ${.CURDIR}/testsuite.sh >out 2>&1
	@if diff -u out ${.CURDIR}/answer; then \
		echo SUCCESS; \
	else \
		echo FAIL; \
		exit 1; \
	fi

.include <bsd.regress.mk>
