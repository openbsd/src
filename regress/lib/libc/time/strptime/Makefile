#	$OpenBSD: Makefile,v 1.1 2004/01/20 16:47:55 millert Exp $

PROG=		strptime_test
SRCS=		main.c
CLEANFILES+=	got

REGRESS_TARGETS=do-reg
do-reg:
	./${PROG} < ${.CURDIR}/tests > got
	diff ${.CURDIR}/expected got

.include <bsd.regress.mk>
