#	$OpenBSD: Makefile,v 1.3 1998/12/21 01:02:34 niklas Exp $
#	$EOM: Makefile,v 1.2 1998/12/21 00:41:55 niklas Exp $

# Test HMAC:

PROG=		hmactest
SRCS=		hash.c hmactest.c
.PATH:		${.CURDIR}/../../
NOMAN=
CFLAGS+=	-I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>