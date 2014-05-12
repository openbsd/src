#	$OpenBSD: Makefile,v 1.3 2014/05/12 19:14:14 miod Exp $

PROG=	pq_test

LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}

REGRESS_TARGETS= regress-pq_test

regress-pq_test: ${PROG}
	${.OBJDIR}/pq_test | cmp -s ${.CURDIR}/expected.txt /dev/stdin

.include <bsd.regress.mk>
