#	$OpenBSD: Makefile,v 1.1 2005/04/08 17:12:49 cloder Exp $
#	$EOM: Makefile,v 1.3 1999/02/25 15:14:24 niklas Exp $

# Test HMAC:

PROG=		hmactest
SRCS=		hash.c hmactest.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.regress.mk>
