#	$OpenBSD: Makefile,v 1.2 1998/11/15 00:44:10 niklas Exp $

# Test HMAC:

PROG=		prftest
SRCS=		log.c prf.c hash.c prftest.c
.PATH:		${.CURDIR}/../../
NOMAN=
CFLAGS+=	-I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>
