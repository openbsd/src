#	$OpenBSD: Makefile,v 1.2 2004/01/21 18:13:48 david Exp $

PROG=		strptime_test
SRCS=		main.c
CLEANFILES+=	got

REGRESS_TARGETS=do-reg
do-reg: ${PROG}
	./${PROG} < ${.CURDIR}/tests > got
	diff ${.CURDIR}/expected got

.include <bsd.regress.mk>
