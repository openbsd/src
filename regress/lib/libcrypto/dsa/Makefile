#	$OpenBSD: Makefile,v 1.3 2014/07/08 15:53:52 jsing Exp $

PROG=	dsatest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS=regress-dsatest

regress-dsatest:	${PROG}
	./${PROG}
	./${PROG} -app2_1

.include <bsd.regress.mk>
