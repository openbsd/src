#	$OpenBSD: Makefile,v 1.1 2002/07/05 15:54:30 itojun Exp $

PROG=		gaitest
SRCS=		gaitest.c
NOMAN=		# defined
CLEANFILES+=	out

regress: ${PROG}
	sh ${.CURDIR}/testsuite.sh >out 2>&1
	@if diff -u out ${.CURDIR}/answer; then \
		echo SUCCESS; \
	else \
		echo FAIL; \
		exit 1; \
	fi

.include <bsd.prog.mk>
