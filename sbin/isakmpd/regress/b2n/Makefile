#	$OpenBSD: Makefile,v 1.5 2000/04/07 22:21:55 niklas Exp $
#	$EOM: Makefile,v 1.11 2000/04/07 19:20:24 niklas Exp $

# Test some math

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
