#	$OpenBSD: Makefile,v 1.1.1.1 2014/04/15 18:01:18 miod Exp $

PROG=	dsatest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

REGRESS_TARGETS=regress-dsatest

regress-dsatest:	${PROG}
	./${PROG}
	./${PROG} -app2_1

.include <bsd.regress.mk>
