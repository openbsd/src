#	$OpenBSD: Makefile,v 1.2 1998/11/15 00:44:09 niklas Exp $

# Test HMAC:

PROG=		hmactest
SRCS=		hash.c hmactest.c
.PATH:		${.CURDIR}/../../
NOMAN=
CFLAGS+=	-I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>