#	$OpenBSD: Makefile,v 1.3 1998/12/21 01:02:29 niklas Exp $
#	$EOM: Makefile,v 1.3 1998/12/21 00:41:50 niklas Exp $

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
