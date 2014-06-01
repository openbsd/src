#	$OpenBSD: Makefile,v 1.2 2014/06/01 11:11:46 miod Exp $

PROG=	evptest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

REGRESS_TARGETS=regress-evptest

regress-evptest:	${PROG}
	./${PROG} ${.CURDIR}/evptests.txt

.include <bsd.regress.mk>
