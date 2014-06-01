#	$OpenBSD: Makefile,v 1.2 2014/06/01 11:11:46 miod Exp $

PROG=	dsatest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

REGRESS_TARGETS=regress-dsatest

regress-dsatest:	${PROG}
	./${PROG}
	./${PROG} -app2_1

.include <bsd.regress.mk>
