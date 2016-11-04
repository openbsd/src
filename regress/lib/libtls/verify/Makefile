#	$OpenBSD: Makefile,v 1.2 2016/11/04 17:51:54 guenther Exp $

PROG=	verifytest
LDADD=	-lcrypto -lssl -Wl,-Bstatic -ltls -Wl,-Bdynamic
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
