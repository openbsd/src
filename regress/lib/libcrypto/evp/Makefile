#	$OpenBSD: Makefile,v 1.3 2014/07/08 15:53:52 jsing Exp $

PROG=	evptest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS=regress-evptest

regress-evptest:	${PROG}
	./${PROG} ${.CURDIR}/evptests.txt

.include <bsd.regress.mk>
