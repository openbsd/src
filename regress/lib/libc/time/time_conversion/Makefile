#	$OpenBSD: Makefile,v 1.1 2022/11/06 12:57:08 beck Exp $

PROG=		timetest
SRCS=		timetest.c

REGRESS_TARGETS=do-timetest

do-timetest: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
