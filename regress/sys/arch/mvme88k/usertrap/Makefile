#	$OpenBSD: Makefile,v 1.1 2003/09/01 19:45:28 miod Exp $

PROG=	trap

.if ${MACHINE_ARCH} != "m88k"
REGRESS_SKIP="yes"
.endif

REGRESS_TARGETS=	do-trap

do-trap:	${PROG}
	sh ${.CURDIR}/testsuite.sh >out 2>&1
	@if diff -u out ${.CURDIR}/answer; then \
		echo SUCCESS; \
	else \
		echo FAIL; \
		exit 1; \
	fi

.include <bsd.regress.mk>
