#	$OpenBSD: Makefile,v 1.2 1998/11/15 00:44:07 niklas Exp $

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
