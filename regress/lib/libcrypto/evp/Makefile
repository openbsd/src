#	$OpenBSD: Makefile,v 1.1.1.1 2014/04/15 18:01:18 miod Exp $

PROG=	evptest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

REGRESS_TARGETS=regress-evptest

regress-evptest:	${PROG}
	./${PROG} ${.CURDIR}/evptests.txt

.include <bsd.regress.mk>
