#	$OpenBSD: Makefile,v 1.1 2017/12/09 16:43:09 jsing Exp $

PROG=	configtest
LDADD=	-lcrypto -lssl -ltls
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
