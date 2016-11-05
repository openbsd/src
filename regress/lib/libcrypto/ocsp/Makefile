#	$OpenBSD: Makefile,v 1.3 2016/11/05 12:45:25 miod Exp $

REGRESS_TARGETS= all_tests

PROG=		ocsp_test
LDADD=		-lcrypto -lssl
DPADD=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto -lssl
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

all_tests: ${PROG}
	${.OBJDIR}/${PROG} www.amazon.com 443
	${.OBJDIR}/${PROG} cloudflare.com 443

.include <bsd.regress.mk>
