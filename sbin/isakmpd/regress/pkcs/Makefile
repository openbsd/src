#	$OpenBSD: Makefile,v 1.2 1998/11/15 00:44:10 niklas Exp $

# Test PKCS#1

PROG=		pkcstest
SRCS=		log.c asn.c gmp_util.c pkcs.c pkcstest.c sysdep.c \
		asn_useful.c hash.c
.PATH:		${.CURDIR}/../../
LDADD+=		-lgmp
DPADD+=		${LIBDES}
NOMAN=
CFLAGS+=	-I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>
