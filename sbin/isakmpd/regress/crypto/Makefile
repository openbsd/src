# Test Crypto:

PROG=		cryptotest
SRCS=		log.c crypto.c cryptotest.c
.PATH:		${.CURDIR}/../../
LDADD+=		-ldes
DPADD+=		${LIBDES}
NOMAN=
CFLAGS+=	-I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>
