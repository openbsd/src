#	$OpenBSD: Makefile,v 1.7 2004/02/25 16:01:29 hshoexer Exp $
#	$EOM: Makefile,v 1.12 2000/10/13 13:04:17 ho Exp $

# Test some math

PROG=		b2ntest
SRCS=		b2ntest.c conf.c log.c math_2n.c sysdep.c util.c
NOMAN=
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
DEBUG=		-g

.include <bsd.prog.mk>
