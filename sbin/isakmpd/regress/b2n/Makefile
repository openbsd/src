#	$OpenBSD: Makefile,v 1.4 1999/02/26 03:26:52 niklas Exp $
#	$EOM: Makefile,v 1.7 1999/02/25 15:10:05 niklas Exp $

# Test HMAC:

PROG=		b2ntest
SRCS=		math_2n.c b2ntest.c util.c sysdep.c
NOMAN=
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
DEBUG=		-g

.include <bsd.prog.mk>
