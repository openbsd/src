#	$OpenBSD: Makefile,v 1.3 2022/12/03 09:55:53 tb Exp $

PROG =		biotest
LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bio/

.include <bsd.regress.mk>
