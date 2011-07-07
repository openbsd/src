#	$OpenBSD: Makefile,v 1.4 2011/07/07 21:00:59 deraadt Exp $

PROG=		gaitest
SRCS=		gaitest.c
NOMAN=		# defined
CLEANFILES+=	out

REGRESS_TARGETS=do-test

do-test: ${PROG}
	sh ${.CURDIR}/testsuite.sh >out 2>&1
	@if diff -u10 out ${.CURDIR}/answer; then \
		echo SUCCESS; \
	else \
		echo FAIL; \
		exit 1; \
	fi

.include <bsd.regress.mk>
