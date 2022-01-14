#	$OpenBSD: Makefile,v 1.5 2022/01/14 09:38:50 tb Exp $

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
