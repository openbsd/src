#	$OpenBSD: Makefile,v 1.3 2002/09/02 20:01:43 avsm Exp $

PROG=		gaitest
SRCS=		gaitest.c
NOMAN=		# defined
CLEANFILES+=	out

REGRESS_TARGETS=do-test

do-test: ${PROG}
	sh ${.CURDIR}/testsuite.sh >out 2>&1
	@if diff -u out ${.CURDIR}/answer; then \
		echo SUCCESS; \
	else \
		echo FAIL; \
		exit 1; \
	fi

.include <bsd.regress.mk>
