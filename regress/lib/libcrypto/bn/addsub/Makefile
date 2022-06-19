#	$OpenBSD: Makefile,v 1.2 2022/06/19 17:08:19 tb Exp $

.include "../../Makefile.inc"

PROG=		bnaddsub
LDADD=		${LIBCRYPTO}
DPADD=		${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
