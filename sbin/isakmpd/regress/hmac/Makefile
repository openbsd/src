#	$OpenBSD: Makefile,v 1.4 1999/02/26 03:28:31 niklas Exp $
#	$EOM: Makefile,v 1.3 1999/02/25 15:14:24 niklas Exp $

# Test HMAC:

PROG=		hmactest
SRCS=		hash.c hmactest.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.prog.mk>
